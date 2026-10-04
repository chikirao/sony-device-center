#include "sony/transport/SonyDeviceFilter.h"
#include "sony/transport/DeviceAddress.h"
#include "SonyOuiTable.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <string>

namespace sony::transport {

namespace {

constexpr std::array<std::string_view, 7> kSonyNameTokens = {
    "WH-", "WF-", "WI-", "MDR-", "LINKBUDS", "ULT WEAR", "SONY",
};

} // namespace

std::optional<std::uint32_t> addressOui(std::string_view address) {
    const auto canonical = DeviceAddress(address).canonical();
    if (!canonical) return std::nullopt;
    // "AC:80:0A:12:34:56": the OUI is the first three octets.
    std::uint32_t oui = 0;
    for (const char c : canonical->substr(0, 8)) {
        if (c == ':') continue;
        oui = (oui << 4) | static_cast<std::uint32_t>(std::isdigit(static_cast<unsigned char>(c)) ? c - '0' : c - 'A' + 10);
    }
    return oui;
}

bool hasSonyOui(std::string_view address) {
    const auto oui = addressOui(address);
    return oui && std::binary_search(detail::kSonyOuis.begin(), detail::kSonyOuis.end(), *oui);
}

bool hasSonyName(std::string_view name) {
    std::string upper(name);
    std::transform(upper.begin(), upper.end(), upper.begin(), [](unsigned char c) {
        return static_cast<char>(std::toupper(c));
    });
    return std::any_of(kSonyNameTokens.begin(), kSonyNameTokens.end(), [&](std::string_view token) {
        return upper.find(token) != std::string::npos;
    });
}

bool isSonyCandidate(const DiscoveredDevice& device) {
    return hasSonyOui(device.address.str()) || hasSonyName(device.name);
}

SonyDeviceDiscovery::SonyDeviceDiscovery(std::unique_ptr<IDeviceDiscovery> inner)
    : _inner(std::move(inner)) {}

std::vector<DiscoveredDevice> SonyDeviceDiscovery::discover() {
    if (!_inner) {
        return {};
    }
    auto devices = _inner->discover();
    std::erase_if(devices, [](const DiscoveredDevice& device) { return !isSonyCandidate(device); });
    return devices;
}

} // namespace sony::transport
