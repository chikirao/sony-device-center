#pragma once
#include <QObject>
#include <QTimer>
#include <atomic>
#include <optional>
#include "sony/core/IDeviceService.h"
#include "sony/core/IpcClient.h"
#include "sony/core/JsonProtocol.h"
namespace sony::devicecenter {
// Lives exclusively on the controller's worker thread. Reader callbacks only
// post work; they never read service state or manipulate Qt properties.
class DeviceBackend : public QObject {
    Q_OBJECT
public:
    explicit DeviceBackend(std::shared_ptr<core::IDeviceService> service = {});
    ~DeviceBackend() override;
    void start();
    void command(QByteArray request, quint64 generation);
    void discover();
    // The OS saw a Bluetooth link come up: retry the connection now.
    void wake();
    void shutdown();
    // The one member another thread may call: makes a connection attempt in
    // progress on the worker fail promptly, so the shutdown() queued after it
    // runs without waiting for a headset that is off or out of range.
    void requestShutdown() noexcept;
signals:
    void snapshotReady(QByteArray data, quint64 generation);
    void devicesReady(QByteArray data);
    void error(QString message, quint64 generation);
    void completed(quint64 generation);
private:
    using Json = core::JsonProtocol::Json;
    Json request(const std::string& method, const Json& params = Json::object());
    void poll();
    void publish();
    void subscribe();
    // Reads the paired list. A refresh the user did not ask for only logs a failure:
    // its error would replace a more useful one on screen.
    void refreshDevices(bool reportErrors);
    std::shared_ptr<core::IDeviceService> _service;
    // _service for requestShutdown(), published once it exists.
    std::atomic<core::IDeviceService*> _serviceForShutdown{nullptr};
    core::IpcClient _ipc;
    bool _usingIpc{false}, _stopped{false}, _incompatible{false};
    QTimer* _timer{nullptr};
    quint64 _generation{0}, _requestId{0};
    core::SonyDevice* _subscribed{nullptr};
    protocol::DeviceEventDispatcher::SubscriptionId _subscription{0};
    std::atomic<bool> _notificationPending{false};
    // Connected state in the last snapshot, to notice it change.
    std::optional<bool> _lastConnected;
};
}
