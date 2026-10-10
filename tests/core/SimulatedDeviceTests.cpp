#include <catch2/catch_test_macros.hpp>
#include "sony/core/CliRequest.h"
#include "sony/core/DeviceService.h"
#include "sony/core/IpcProtocol.h"
#include "sony/core/JsonProtocol.h"
#include "sony/core/SimulatedDevice.h"
#include "sony/protocol/FrameCodec.h"
#include <chrono>
#include <thread>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;

namespace {
// Notifications arrive on the device's reader thread; give it a moment.
template <typename Pred>
bool eventually(Pred pred) {
    for (int i = 0; i < 100 && !pred(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return pred();
}
}

TEST_CASE("Simulated device connects and answers the V2 refresh queries", "[core][simulated]") {
    auto simulated = createSimulatedDevice();
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    CHECK(service.selectedAddress() == "CC:98:8B:00:11:22");

    const auto state = service.snapshot();
    REQUIRE(state);
    CHECK(state->battery.main == 87);
    CHECK_FALSE(state->battery.charging);
    CHECK(state->noiseControl.mode == NoiseControlMode::NoiseCancelling);
    CHECK(state->equalizer.preset == 0x16);
    CHECK(state->equalizer.clearBass == 4);
    CHECK(state->dsee);
}

TEST_CASE("Simulated device applies writes and reports them back", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM5", "00:11:22:33:44:55");
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    auto* device = service.activeDevice();
    REQUIRE(device);

    NoiseControlState ambient;
    ambient.mode = NoiseControlMode::Ambient;
    ambient.ambientLevel = 12;
    ambient.focusOnVoice = true;
    device->setNoiseControl(ambient);
    device->refreshNoiseControl();
    auto nc = service.snapshot()->noiseControl;
    CHECK(nc.mode == NoiseControlMode::Ambient);
    CHECK(nc.ambientLevel == 12);
    CHECK(nc.focusOnVoice);

    device->setEqualizerCustom(-3, {1, 2, 3, 4, 5});
    device->refreshEqualizer();
    auto eq = service.snapshot()->equalizer;
    CHECK(eq.preset == 0xa0);
    CHECK(eq.clearBass == -3);
    CHECK(eq.bands == std::array<int, 5>{1, 2, 3, 4, 5});

    device->setDsee(false);
    device->refreshDsee();
    CHECK_FALSE(service.snapshot()->dsee);
}

TEST_CASE("Simulated device goes away after power off and stays away", "[core][simulated]") {
    auto simulated = createSimulatedDevice();
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());

    service.activeDevice()->powerOff();
    REQUIRE(eventually([&] { service.tick(); return !service.isConnected(); }));
    // Auto-connect keeps trying, but a switched-off headset does not answer.
    for (int i = 0; i < 5; ++i) service.tick();
    CHECK_FALSE(service.isConnected());
    CHECK(service.connectionState() != "manually_disconnected");
}

namespace {
// Speak-to-Chat payloads the host wrote after the first `from` frames.
std::vector<std::vector<uint8_t>> speakToChatWrites(const SimulatedDeviceTransport& transport, size_t from) {
    std::vector<std::vector<uint8_t>> writes;
    const auto frames = transport.sentFrames();
    for (size_t i = from; i < frames.size(); ++i) {
        const auto frame = FrameCodec::decode(frames[i]);
        if (frame.type == DataType::DataMdr && frame.payload.size() >= 2 && frame.payload[1] == 0x05
            && (frame.payload[0] == 0xf8 || frame.payload[0] == 0xfc))
            writes.push_back(frame.payload);
    }
    return writes;
}
}

TEST_CASE("Enabling Speak-to-Chat keeps the settings the headset holds", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM4", "00:11:22:33:44:55");
    // Chosen on the phone before this connection.
    simulated.transport->setSpeakToChatConfig(2, true, 0);
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    auto* device = service.activeDevice();
    REQUIRE(device);

    // Not read on this connection yet: enabling reads it, then writes it back.
    device->setSpeakToChat(true);
    auto config = device->readSpeakToChatConfig();
    CHECK(config.sensitivity == 2);
    CHECK(config.voicePassthrough);
    CHECK(config.timeout == 0);

    // Changed on the phone while connected: the headset's notification is
    // what the next enable writes back, without reading again.
    simulated.transport->setSpeakToChatConfig(1, false, 3);
    REQUIRE(eventually([&] { return service.snapshot()->speakToChatConfig.timeout == 3; }));
    simulated.transport->setUnanswered(0xfa, true);
    device->setSpeakToChat(false);
    const auto before = simulated.transport->sentCount();
    device->setSpeakToChat(true);
    CHECK(speakToChatWrites(*simulated.transport, before) == std::vector<std::vector<uint8_t>>{
        {0xfc, 0x05, 0x00, 0x01, 0x00, 0x03},
        {0xf8, 0x05, 0x01, 0x01},
    });
}

TEST_CASE("Speak-to-Chat writes nothing when the headset's settings cannot be read", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM4", "00:11:22:33:44:55");
    simulated.transport->setUnanswered(0xfa, true);
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    auto* device = service.activeDevice();
    REQUIRE(device);

    // Rather than guess the other settings, both fail before sending anything.
    const auto before = simulated.transport->sentCount();
    CHECK_THROWS_AS(device->setSpeakToChat(true), SonyException);
    CHECK_THROWS_AS(device->setSpeakToChatConfig(device->currentSpeakToChatConfig()), SonyException);
    CHECK(speakToChatWrites(*simulated.transport, before).empty());
}

TEST_CASE("V1 Speak-to-Chat notifications are ignored on a V2 headset", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM5", "00:11:22:33:44:55");
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());

    simulated.transport->setSpeakToChatConfig(2, true, 3);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    const auto state = service.snapshot();
    CHECK(state->speakToChatConfig.timeout == 1);
    CHECK(state->features.at("speakToChatConfig").availability == "unsupported");
}

namespace {
// Auto power-off writes (f8 04) the host sent after the first `from` frames.
size_t autoPowerOffWrites(const SimulatedDeviceTransport& transport, size_t from) {
    size_t writes = 0;
    const auto frames = transport.sentFrames();
    for (size_t i = from; i < frames.size(); ++i) {
        const auto frame = FrameCodec::decode(frames[i]);
        if (frame.type == DataType::DataMdr && frame.payload.size() >= 2 && frame.payload[0] == 0xf8 && frame.payload[1] == 0x04)
            ++writes;
    }
    return writes;
}
}

TEST_CASE("A WH-1000XM4 is only offered the auto power-off choices it has", "[core][simulated]") {
    auto simulated = createSimulatedDevice("WH-1000XM4", "00:11:22:33:44:55");
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());
    auto* device = service.activeDevice();
    REQUIRE(device);
    REQUIRE(device->capabilities().autoPowerOffWhenTakenOffOnly);
    const auto run = [&](std::initializer_list<const char*> words) {
        const auto request = cliRequestFor(std::vector<std::string>(words.begin(), words.end()));
        return cliSelect(JsonProtocol::execute(request.request, service), request.select);
    };

    // Its two: when taken off (5) and never (0).
    device->setAutoPowerOff(5);
    CHECK(device->readAutoPowerOff() == 5);
    REQUIRE(run({"apo", "0"})["ok"] == true);
    CHECK(device->readAutoPowerOff() == 0);

    // A timed choice is refused on every path, before anything is sent.
    const auto before = simulated.transport->sentCount();
    CHECK_THROWS_AS(device->setAutoPowerOff(1), SonyException);
    CHECK(run({"apo", "3"})["error"]["code"] == "Unsupported");
    CHECK_FALSE(IpcProtocol::execute(IpcProtocol::parseCommand("apo 2"), service).success);
    CHECK(autoPowerOffWrites(*simulated.transport, before) == 0);

    // "apo get" reads, and writes nothing.
    const auto read = run({"apo", "get"});
    REQUIRE(read["ok"] == true);
    CHECK(read["data"] == 0);
    CHECK(autoPowerOffWrites(*simulated.transport, before) == 0);
}
