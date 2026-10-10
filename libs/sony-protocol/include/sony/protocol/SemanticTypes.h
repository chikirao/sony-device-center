#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace sony::protocol {

struct BatteryState {
    std::optional<int> main;
    std::optional<int> left;
    std::optional<int> right;
    std::optional<int> caseBattery;
    bool charging{false};
};

enum class NoiseControlMode {
    Off,
    NoiseCancelling,
    Ambient
};

struct NoiseControlState {
    NoiseControlMode mode{NoiseControlMode::Off};
    int ambientLevel{0};
    bool focusOnVoice{false};
};

struct EqualizerState {
    int preset{0};
    int clearBass{0};
    std::array<int, 5> bands{0, 0, 0, 0, 0};
};

// Speak-to-Chat tuning, as the wire codes. Sensitivity: 0 Auto, 1 High,
// 2 Low. Timeout: 0 Short (~15 s), 1 Standard (~30 s), 2 Long (~1 min),
// 3 never ends on its own. Voice passthrough is Headphones Connect's name;
// Gadgetbridge calls the same byte focus on voice.
struct SpeakToChatConfig {
    int sensitivity{0};
    bool voicePassthrough{false};
    int timeout{1};
};

} // namespace sony::protocol
