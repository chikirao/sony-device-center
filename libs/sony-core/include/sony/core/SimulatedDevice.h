#pragma once

#include "sony/transport/FakeTransport.h"

#include <array>
#include <set>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace sony::core {

// In-memory stand-in for a Sony headset. It ACKs every command, answers the
// queries the selected command set issues from a small mutable state, applies
// SET commands to that state, and raises the same unsolicited notifications a
// real device sends after a change. Enough for the daemon, the CLI and the GUI
// to run end to end without Bluetooth hardware.
class SimulatedDeviceTransport : public transport::FakeTransport {
public:
    // Earbuds report left/right/case batteries and ignore the single-cell
    // query; over-ear models do the opposite.
    explicit SimulatedDeviceTransport(bool earbuds = false, bool legacyV1 = false);

    size_t send(std::span<const std::byte> data) override;

    // Changes the simulated charge and pushes the unsolicited battery
    // notification a real headset would send.
    void setBattery(int level, bool charging);
    // Changes the V1 Speak-to-Chat settings the way Headphones Connect on
    // another connected device would, with the NTFY the XM4 then sends.
    void setSpeakToChatConfig(int sensitivity, bool voicePassthrough, int timeout);
    // Requests with this opcode are still ACKed but get no reply, like a
    // headset that misses one: the host's read times out.
    void setUnanswered(uint8_t opcode, bool unanswered);

private:
    void handle(const std::vector<uint8_t>& request);
    void reply(std::vector<uint8_t> payload);

    std::mutex _stateMutex;
    uint8_t _sequence{0};

    bool _earbuds{false};
    bool _legacyV1{false};
    uint8_t _battery{87};
    bool _charging{false};
    uint8_t _batteryLeft{81};
    uint8_t _batteryRight{79};
    uint8_t _batteryCase{64};
    // effect, settingType (0 = NC / 1 = ambient), focus on voice, ambient level
    std::array<uint8_t, 4> _noise{0x01, 0x00, 0x00, 0x00};
    uint8_t _eqPreset{0x16};
    // clear bass followed by five bands, all encoded as value + 10
    std::array<uint8_t, 6> _eqBands{0x0e, 0x0a, 0x0a, 0x0a, 0x0a, 0x0a};
    bool _dsee{true};
    std::array<uint8_t, 2> _autoPowerOff{0x11, 0x00};
    bool _speakToChat{false};
    std::array<uint8_t, 3> _speakToChatConfig{0x00, 0x00, 0x01};  // Auto, no passthrough, Standard
    bool _adaptiveVolume{false};
    std::set<uint8_t> _unanswered;
};

// One simulated headset plus a discovery that lists only it, ready to hand to
// DeviceService.
struct SimulatedDevice {
    std::shared_ptr<SimulatedDeviceTransport> transport;
    std::shared_ptr<transport::FakeDeviceDiscovery> discovery;
    std::string name;
    std::string address;
};

// Names of the WF / LinkBuds families produce an earbuds simulation.
SimulatedDevice createSimulatedDevice(std::string name = {}, std::string address = {});

} // namespace sony::core
