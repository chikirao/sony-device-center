#include "sony/core/CliRequest.h"
#include "sony/protocol/EqualizerPresets.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>

namespace sony::core {

using Json = JsonProtocol::Json;

namespace {

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

bool onOff(const std::vector<std::string>& tokens, size_t at, const char* what) {
    if (tokens.size() <= at) throw std::invalid_argument(std::string(what) + " expects on or off");
    const auto value = lower(tokens[at]);
    if (value == "on" || value == "true" || value == "1") return true;
    if (value == "off" || value == "false" || value == "0") return false;
    throw std::invalid_argument(std::string(what) + " expects on or off, not '" + tokens[at] + "'");
}

int integer(const std::string& text, int lo, int hi, const char* what) {
    size_t used = 0;
    long long value = 0;
    try { value = std::stoll(text, &used); } catch (...) { used = 0; }
    if (used != text.size() || value < lo || value > hi)
        throw std::invalid_argument(std::string(what) + " must be a number from " + std::to_string(lo) + " to " + std::to_string(hi));
    return static_cast<int>(value);
}

Json envelope(const char* method, Json params = Json::object()) {
    return {{"version", JsonProtocol::Version}, {"id", 1}, {"method", method}, {"params", std::move(params)}};
}

} // namespace

CliRequest cliRequestFor(const std::vector<std::string>& tokens) {
    if (tokens.empty()) throw std::invalid_argument("No command given");
    const auto verb = lower(tokens[0]);
    const auto arg = [&](size_t i) { return tokens.size() > i ? lower(tokens[i]) : std::string{}; };

    if (verb == "devices") return {envelope("devices"), ""};
    if (verb == "info") return {envelope("snapshot"), ""};
    if (verb == "status") return {envelope("snapshot"), "status"};
    if (verb == "battery") return {envelope("snapshot"), "battery"};
    if (verb == "anc") return {envelope("anc", {{"enabled", onOff(tokens, 1, "anc")}}), ""};
    if (verb == "ambient") {
        if (arg(1) == "off") return {envelope("anc", {{"enabled", false}}), ""};
        if (tokens.size() < 2) throw std::invalid_argument("ambient expects a level from 1 to 20, or off");
        return {envelope("ambient", {{"level", integer(tokens[1], 1, 20, "ambient level")}, {"focusOnVoice", false}}), ""};
    }
    if (verb == "eq") {
        const auto sub = arg(1);
        if (sub.empty() || sub == "get") return {envelope("snapshot"), "equalizer"};
        if (sub == "custom") {
            if (tokens.size() != 8) throw std::invalid_argument("eq custom expects Clear Bass and five band values");
            Json bands = Json::array();
            for (size_t i = 3; i < 8; ++i) bands.push_back(integer(tokens[i], -10, 10, "band"));
            return {envelope("eqCustom", {{"clearBass", integer(tokens[2], -10, 10, "Clear Bass")}, {"bands", bands}}), ""};
        }
        const auto& name = sub == "preset" ? (tokens.size() > 2 ? tokens[2] : std::string{}) : tokens[1];
        const int preset = protocol::equalizerPresetFromName(name);
        if (preset < 0 || protocol::equalizerPresetId(preset).empty())
            throw std::invalid_argument("Unknown equalizer preset: " + name);
        return {envelope("eqPreset", {{"preset", preset}}), ""};
    }
    if (verb == "dsee") {
        // "auto" is what the headphones call the enabled state.
        const bool enabled = arg(1) == "auto" ? true : onOff(tokens, 1, "dsee");
        return {envelope("dsee", {{"enabled", enabled}}), ""};
    }
    if (verb == "stc" || verb == "speak-to-chat" || verb == "speaktochat")
        return {envelope("speakToChat", {{"enabled", onOff(tokens, 1, "speak-to-chat")}}), ""};
    if (verb == "adaptive" || verb == "adaptive-volume")
        return {envelope("adaptiveVolume", {{"enabled", onOff(tokens, 1, "adaptive volume")}}), ""};
    if (verb == "apo" || verb == "autopoweroff") {
        if (tokens.size() < 2) throw std::invalid_argument("apo expects a preset index from 0 to 5");
        return {envelope("autoPowerOff", {{"index", integer(tokens[1], 0, 5, "apo index")}}), ""};
    }
    if (verb == "power") {
        if (arg(1) != "off") throw std::invalid_argument("power expects off");
        return {envelope("powerOff"), ""};
    }
    if (verb == "connect") {
        if (tokens.size() < 2) throw std::invalid_argument("connect expects a Bluetooth address");
        std::string name;
        for (size_t i = 2; i < tokens.size(); ++i) name += (i > 2 ? " " : "") + tokens[i];
        return {envelope("connect", {{"address", tokens[1]}, {"name", name}}), ""};
    }
    if (verb == "disconnect") return {envelope("disconnect"), ""};
    throw std::invalid_argument("Unknown command: " + tokens[0]);
}

Json cliSelect(const Json& response, const std::string& select) {
    if (select.empty() || !response.is_object() || !response.value("ok", false) || !response.contains("data")) return response;
    const auto& data = response.at("data");
    Json out = response;
    if (select == "status") {
        out["data"] = Json::object();
        for (const auto* key : {"connected", "connectionState", "address", "name", "lastError", "codec", "firmware"})
            if (data.contains(key)) out["data"][key] = data.at(key);
    } else if (data.contains(select)) {
        out["data"] = data.at(select);
    }
    return out;
}

} // namespace sony::core
