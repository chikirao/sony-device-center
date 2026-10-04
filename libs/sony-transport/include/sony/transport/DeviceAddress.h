#pragma once

#include <cctype>
#include <compare>
#include <optional>
#include <string>
#include <string_view>

namespace sony::transport {

class DeviceAddress {
public:
    DeviceAddress() = default;
    /* implicit */ DeviceAddress(std::string address) : _address(std::move(address)) {}
    /* implicit */ DeviceAddress(std::string_view address) : _address(address) {}
    /* implicit */ DeviceAddress(const char* address) : _address(address ? address : "") {}

    [[nodiscard]] const std::string& str() const noexcept { return _address; }
    [[nodiscard]] bool empty() const noexcept { return _address.empty(); }

    /// The address as "AC:80:0A:12:34:56". Platforms format addresses differently:
    /// Linux "AC:80:0A:12:34:56", Windows "ac:80:0a:12:34:56", macOS
    /// "ac-80-0a-12-34-56". Accepts six hex octets separated by ':' or '-' in
    /// either case, and gives std::nullopt for other text.
    [[nodiscard]] std::optional<std::string> canonical() const {
        constexpr std::size_t kLength = 17;
        if (_address.size() != kLength) return std::nullopt;
        std::string result(kLength, ':');
        for (std::size_t i = 0; i < kLength; ++i) {
            const auto c = static_cast<unsigned char>(_address[i]);
            if (i % 3 == 2) {
                if (c != ':' && c != '-') return std::nullopt;
                continue;
            }
            if (!std::isxdigit(c)) return std::nullopt;
            result[i] = static_cast<char>(std::toupper(c));
        }
        return result;
    }

    /// True when both name the same device, however each is formatted. Text that
    /// is not an address only matches itself.
    [[nodiscard]] bool sameDevice(const DeviceAddress& other) const {
        const auto mine = canonical();
        const auto theirs = other.canonical();
        return mine && theirs ? *mine == *theirs : _address == other._address;
    }

    auto operator<=>(const DeviceAddress&) const = default;
    bool operator==(const DeviceAddress&) const = default;

private:
    std::string _address;
};

} // namespace sony::transport
