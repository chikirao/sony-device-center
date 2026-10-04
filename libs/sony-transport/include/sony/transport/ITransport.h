#pragma once

#include "DeviceAddress.h"
#include <cstddef>
#include <span>

namespace sony::transport {

class ITransport {
public:
    virtual ~ITransport() = default;

    virtual void connect(const DeviceAddress& address) = 0;
    virtual void disconnect() noexcept = 0;
    // Makes a connect() or receive() blocked on another thread fail promptly, so a
    // shutdown does not wait for them. Thread-safe and non-blocking. The connection is
    // unusable afterwards; the owner still calls disconnect(). Transports that cannot
    // interrupt these calls keep the no-op.
    virtual void abort() noexcept {}
    [[nodiscard]] virtual bool isConnected() const noexcept = 0;
    virtual size_t send(std::span<const std::byte> data) = 0;
    virtual size_t receive(std::span<std::byte> buffer) = 0;
};

} // namespace sony::transport
