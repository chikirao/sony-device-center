#include "sony/core/DeviceService.h"
#include "sony/protocol/DeviceProfileRegistry.h"
#include "sony/transport/Logger.h"
#include <algorithm>

namespace sony::core {

namespace {
// A typed address (sonyd -d, sonyctl connect) can differ from the discovered form in
// case and separator. The discovered entry carries the name that selects the profile.
std::vector<DiscoveredDevice>::const_iterator findDevice(
    const std::vector<DiscoveredDevice>& devices, const transport::DeviceAddress& address) {
    return std::find_if(devices.begin(), devices.end(), [&](const auto& d) { return address.sameDevice(d.address); });
}
} // namespace

DeviceService::DeviceService(
    std::shared_ptr<transport::ITransport> transport,
    std::shared_ptr<transport::IDeviceDiscovery> discovery, Now now)
    : _transport(std::move(transport)), _discovery(std::move(discovery)), _now(std::move(now)) {}

DeviceService::~DeviceService() {
    disconnect();
}

std::vector<DiscoveredDevice> DeviceService::discoverDevices() {
    if (!_discovery) {
        return {};
    }
    auto rawDevices = _discovery->discover();
    std::vector<DiscoveredDevice> result;
    result.reserve(rawDevices.size());
    for (const auto& dev : rawDevices) {
        auto profile = protocol::DeviceProfileRegistry::getProfileForDevice(dev.name);
        result.push_back(DiscoveredDevice{
            .address = dev.address.str(),
            .name = dev.name,
            .version = profile.protocol,
            .paired = dev.paired, .connected = dev.connected
        });
    }
    std::stable_sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
        if (a.connected.value_or(false) != b.connected.value_or(false)) return a.connected.value_or(false);
        return a.address < b.address;
    });
    return result;
}

void DeviceService::connect(const transport::DeviceAddress& address, std::string_view name) {
    std::lock_guard lock(_mutex);
    // Use the discovered entry for this address when there is one: its address is the
    // form the UI lists, and its name selects the profile when the caller gave none.
    DiscoveredDevice selected{.address = address.str(), .name = std::string(name)};
    try {
        const auto devices = discoverDevices();
        if (const auto found = findDevice(devices, address); found != devices.end()) {
            selected.address = found->address;
            if (selected.name.empty()) selected.name = found->name;
        }
    } catch (const std::exception&) {
        // Discovery is only a lookup here; connect with what the caller gave.
    }
    _target = selected.address; _automatic = true; _retrySeconds = 1;
    try { _connect(transport::DeviceAddress(selected.address), selected.name); }
    catch (const std::exception& ex) {
        _wasConnected = false;
        if (_device) _device->disconnect();
        _lastError = ex.what(); _connectionState = "retrying";
        _nextAttempt = _now() + std::chrono::seconds(1); throw;
    }
}

void DeviceService::_connect(const transport::DeviceAddress& address, std::string_view name) {
    _selected = address.str(); _connectionState = "connecting";
    if (!_device) {
        // The version passed here is provisional; SonyDevice::connect() resolves
        // it from the device profile. Start from V1 so that a failure to resolve
        // can never leave a legacy device on the V2 command set.
        _device = std::make_unique<SonyDevice>(_transport, SonyProtocolVersion::V1);
    }
    _device->connect(address, name);
    if (!_device->isConnected()) throw SonyException(SonyErrorCode::Disconnected, "Bluetooth link closed during initialization");
    _wasConnected = true; _connectionState = "connected"; _lastError.clear(); _retrySeconds = 1;
    _nextSettings = _now() + std::chrono::seconds(5);
    _nextBattery = _now() + std::chrono::seconds(30);
    Logger::info(LogCategory::Device, "Connected to " + std::string(name));
}

void DeviceService::disconnect() noexcept {
    std::lock_guard lock(_mutex);
    _automatic = false; _wasConnected = false; _connectionState = "manually_disconnected";
    if (_device) {
        _device->disconnect();
    }
}

bool DeviceService::isConnected() const noexcept {
    std::lock_guard lock(_mutex);
    return _device && _device->isConnected();
}

SonyDevice* DeviceService::activeDevice() noexcept {
    std::lock_guard lock(_mutex);
    return _device.get();
}

protocol::DeviceStateSnapshot DeviceService::snapshot() const {
    std::lock_guard lock(_mutex);
    if (_device) {
        return _device->snapshot();
    }
    return std::make_shared<const protocol::DeviceState>();
}

void DeviceService::startAutoConnect(std::string address) {
    std::lock_guard lock(_mutex);
    _target = std::move(address); _automatic = true; _retrySeconds = 1;
    _nextAttempt = _now(); _connectionState = "searching";
}
void DeviceService::wake() {
    std::lock_guard lock(_mutex);
    if (!_automatic || isConnected()) return;
    _retrySeconds = 1;
    _nextAttempt = _now();
}
std::string DeviceService::connectionState() const {
    std::lock_guard lock(_mutex);
    return _connectionState == "connected" && !isConnected() ? "retrying" : _connectionState;
}
std::string DeviceService::selectedAddress() const { std::lock_guard lock(_mutex); return _selected; }
std::string DeviceService::lastError() const { std::lock_guard lock(_mutex); return _lastError; }
void DeviceService::tick() {
    std::lock_guard lock(_mutex);
    if (isConnected()) {
        if (_now() >= _nextSettings) {
            // One inquiry per tick keeps maintenance from monopolizing requests.
            _device->refreshSettingsStep();
            _nextSettings = _now() + std::chrono::milliseconds(500);
        }
        if (_now() >= _nextBattery) {
            _device->refreshBattery(); _nextBattery = _now() + std::chrono::seconds(30);
        }
        return;
    }
    if (!_automatic) return;
    if (_wasConnected) {
        _wasConnected = false;
        _device->disconnect();
        _lastError = "Bluetooth connection lost; reconnecting";
        _nextAttempt = _now(); _connectionState = "retrying";
        Logger::warn(LogCategory::Device, _lastError);
    }
    if (_now() < _nextAttempt) return;
    try {
        auto candidates = discoverDevices();
        if (!_target.empty()) {
            const auto found = findDevice(candidates, transport::DeviceAddress(_target));
            DiscoveredDevice selected{.address = _target};
            if (found != candidates.end()) selected = *found;
            candidates = {selected};
        }
        _connectionState = "searching";
        for (const auto& candidate : candidates) {
            try { _connect(transport::DeviceAddress(candidate.address), candidate.name); return; }
            catch (const std::exception& ex) { _lastError = ex.what(); if (_device) _device->disconnect(); }
        }
        if (candidates.empty()) _lastError = "No paired Sony device found";
    } catch (const std::exception& ex) { _lastError = ex.what(); }
    _connectionState = "retrying";
    Logger::warn(LogCategory::Device, _lastError + "; retry in " + std::to_string(_retrySeconds) + "s");
    _nextAttempt = _now() + std::chrono::seconds(_retrySeconds);
    _retrySeconds = std::min(30u, _retrySeconds * 2);
}
} // namespace sony::core
