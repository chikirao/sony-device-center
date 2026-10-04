#pragma once
#include <mutex>

#include "IpcProtocol.h"
#include <chrono>
#include <string>
#include <string_view>

namespace sony::core {

std::string defaultSocketPath();

class IpcClient {
public:
    explicit IpcClient(std::string socketPath = defaultSocketPath());
    ~IpcClient() = default;

    [[nodiscard]] bool isDaemonRunning(std::chrono::milliseconds timeout = std::chrono::milliseconds(200));
    IpcResponse sendCommand(std::string_view commandLine, std::chrono::milliseconds timeout = std::chrono::milliseconds(3000));

    std::string request(std::string_view line, std::chrono::milliseconds timeout = std::chrono::seconds(30));

    // Thread-safe, for shutdown: a request() in progress on another thread fails
    // now instead of waiting for a busy daemon, and every later one fails at once.
    void abort() noexcept;

    [[nodiscard]] const std::string& socketPath() const noexcept;

private:
    struct ActiveRequest;
    std::string _socketPath;
    std::mutex _activeMutex;
    int _activeFd{-1};
    bool _aborted{false};
};

} // namespace sony::core
