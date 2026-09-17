#include <catch2/catch_test_macros.hpp>
#include "sony/core/CliRequest.h"
#include "sony/core/DeviceService.h"
#include "sony/core/JsonProtocol.h"
#include "sony/core/SimulatedDevice.h"

#include <chrono>
#include <thread>

using namespace sony;
using namespace sony::core;
using Json = JsonProtocol::Json;

namespace {
CliRequest cli(std::initializer_list<const char*> words) {
    return cliRequestFor(std::vector<std::string>(words.begin(), words.end()));
}
template <typename Pred>
bool eventually(Pred pred) {
    for (int i = 0; i < 100 && !pred(); ++i) std::this_thread::sleep_for(std::chrono::milliseconds(10));
    return pred();
}
}

TEST_CASE("sonyctl words map onto the typed request", "[core][cli]") {
    SECTION("read-only commands are snapshots with a selection") {
        CHECK(cli({"info"}).request["method"] == "snapshot");
        CHECK(cli({"info"}).select.empty());
        CHECK(cli({"battery"}).select == "battery");
        CHECK(cli({"status"}).select == "status");
        CHECK(cli({"eq"}).select == "equalizer");
        CHECK(cli({"eq", "get"}).select == "equalizer");
        CHECK(cli({"devices"}).request["method"] == "devices");
        CHECK(cli({"Devices"}).request["method"] == "devices");
    }
    SECTION("every request carries the protocol version and an id") {
        const auto request = cli({"anc", "on"}).request;
        CHECK(request["version"] == JsonProtocol::Version);
        CHECK(request["id"] == 1);
        CHECK(request["params"]["enabled"] == true);
    }
    SECTION("switches accept on/off and friends") {
        CHECK(cli({"anc", "off"}).request["params"]["enabled"] == false);
        CHECK(cli({"dsee", "auto"}).request["params"]["enabled"] == true);
        CHECK(cli({"dsee", "0"}).request["params"]["enabled"] == false);
        CHECK(cli({"stc", "on"}).request["method"] == "speakToChat");
        CHECK(cli({"adaptive", "off"}).request["method"] == "adaptiveVolume");
        CHECK_THROWS_AS(cli({"anc"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"anc", "maybe"}), std::invalid_argument);
    }
    SECTION("ambient") {
        const auto level = cli({"ambient", "12"}).request;
        CHECK(level["method"] == "ambient");
        CHECK(level["params"]["level"] == 12);
        CHECK(level["params"]["focusOnVoice"] == false);
        const auto off = cli({"ambient", "off"}).request;
        CHECK(off["method"] == "anc");
        CHECK(off["params"]["enabled"] == false);
        // Out of range is an error, not a silent clamp.
        CHECK_THROWS_AS(cli({"ambient", "0"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"ambient", "21"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"ambient", "ten"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"ambient"}), std::invalid_argument);
    }
    SECTION("equalizer") {
        CHECK(cli({"eq", "bass-boost"}).request["params"]["preset"] == 0x16);
        CHECK(cli({"eq", "preset", "Vocal"}).request["params"]["preset"] == 0x14);
        CHECK(cli({"eq", "off"}).request["params"]["preset"] == 0);
        CHECK_THROWS_AS(cli({"eq", "loudness"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"eq", "preset"}), std::invalid_argument);
        const auto custom = cli({"eq", "custom", "3", "1", "0", "-1", "-2", "5"}).request;
        CHECK(custom["method"] == "eqCustom");
        CHECK(custom["params"]["clearBass"] == 3);
        CHECK(custom["params"]["bands"] == Json({1, 0, -1, -2, 5}));
        CHECK_THROWS_AS(cli({"eq", "custom", "3", "1", "0"}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"eq", "custom", "11", "0", "0", "0", "0", "0"}), std::invalid_argument);
    }
    SECTION("power, auto power-off, connections") {
        CHECK(cli({"power", "off"}).request["method"] == "powerOff");
        CHECK_THROWS_AS(cli({"power", "on"}), std::invalid_argument);
        CHECK(cli({"apo", "3"}).request["params"]["index"] == 3);
        CHECK_THROWS_AS(cli({"apo", "6"}), std::invalid_argument);
        const auto connect = cli({"connect", "cc:98:8b:00:11:22", "WH-1000XM5", "mine"}).request;
        CHECK(connect["method"] == "connect");
        CHECK(connect["params"]["address"] == "cc:98:8b:00:11:22");
        CHECK(connect["params"]["name"] == "WH-1000XM5 mine");
        CHECK(cli({"disconnect"}).request["method"] == "disconnect");
        CHECK_THROWS_AS(cli({"connect"}), std::invalid_argument);
    }
    SECTION("unknown words") {
        CHECK_THROWS_AS(cli({}), std::invalid_argument);
        CHECK_THROWS_AS(cli({"dance"}), std::invalid_argument);
    }
}

TEST_CASE("cliSelect narrows a reply to the requested object", "[core][cli]") {
    const Json reply = {{"version", 1}, {"id", 1}, {"ok", true},
                        {"data", {{"connected", true}, {"connectionState", "connected"}, {"address", "AA"}, {"name", "XM5"},
                                  {"lastError", ""}, {"codec", "LDAC"}, {"firmware", "1.0"},
                                  {"battery", {{"main", 85}}}, {"equalizer", {{"preset", 22}}}}}};
    CHECK(cliSelect(reply, "") == reply);
    CHECK(cliSelect(reply, "battery")["data"] == Json({{"main", 85}}));
    CHECK(cliSelect(reply, "equalizer")["data"]["preset"] == 22);
    const auto status = cliSelect(reply, "status")["data"];
    CHECK(status["connected"] == true);
    CHECK(status["codec"] == "LDAC");
    CHECK_FALSE(status.contains("battery"));
    CHECK(cliSelect(reply, "battery")["ok"] == true);
    CHECK(cliSelect(reply, "nothing")["data"] == reply["data"]);
    const Json failed = {{"version", 1}, {"id", 1}, {"ok", false}, {"error", {{"code", "Disconnected"}, {"message", "gone"}}}};
    CHECK(cliSelect(failed, "battery") == failed);
}

TEST_CASE("Typed CLI requests run end to end against the simulator", "[core][cli][simulated]") {
    auto simulated = createSimulatedDevice();
    DeviceService service(simulated.transport, simulated.discovery);
    service.startAutoConnect(simulated.address);
    service.tick();
    REQUIRE(service.isConnected());

    auto run = [&](std::initializer_list<const char*> words) {
        const auto request = cli(words);
        return cliSelect(JsonProtocol::execute(request.request, service), request.select);
    };
    auto battery = run({"battery"});
    REQUIRE(battery["ok"] == true);
    CHECK(battery["data"]["main"] == 87);
    CHECK(battery["data"]["charging"] == false);

    auto status = run({"status"});
    CHECK(status["data"]["connected"] == true);
    CHECK(status["data"]["name"] == "WH-1000XM5");

    REQUIRE(run({"ambient", "7"})["ok"] == true);
    CHECK(eventually([&] { return service.snapshot()->noiseControl.ambientLevel == 7; }));
    REQUIRE(run({"eq", "custom", "2", "1", "0", "-1", "0", "1"})["ok"] == true);
    auto eq = run({"eq", "get"});
    CHECK(eq["data"]["preset"] == 0xa0);
    CHECK(eq["data"]["clearBass"] == 2);
    CHECK(eq["data"]["bands"] == Json({1, 0, -1, 0, 1}));

    const auto devices = run({"devices"});
    REQUIRE(devices["ok"] == true);
    CHECK(devices["data"].is_array());

    // A typed error keeps the envelope, so scripts can rely on it.
    REQUIRE(run({"disconnect"})["ok"] == true);
    const auto failed = run({"anc", "on"});
    CHECK(failed["ok"] == false);
    CHECK(failed["error"]["code"] == "Disconnected");
    CHECK(run({"battery"})["ok"] == true);
}
