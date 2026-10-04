#pragma once

#include "sony/transport/ITransport.h"
#include "sony/transport/IDeviceDiscovery.h"
#include <functional>
#include <memory>

namespace sony::transport {

/// Creates the native Bluetooth transport for the current operating system
/// (Linux BlueZ/RFCOMM, Windows WinSock RFCOMM, macOS IOBluetooth),
/// or falls back to FakeTransport if platform Bluetooth is unavailable.
std::unique_ptr<ITransport> createPlatformTransport();

/// Creates the native Bluetooth device discovery service for the current OS.
/// It lists paired and connected devices, and SonyDeviceDiscovery keeps only
/// the devices with a Sony address prefix or a Sony name.
std::unique_ptr<IDeviceDiscovery> createPlatformDiscovery();

/// Runs `body` and returns its result while the native Bluetooth stack can
/// deliver its callbacks. On macOS, IOBluetooth delivers RFCOMM open, data and
/// close callbacks through the main thread's run loop; when called on the main
/// thread this runs `body` on a worker and services that run loop until `body`
/// returns. GUI apps already run that loop and need not use this. Elsewhere,
/// or off the main thread, it simply calls `body`. Exceptions propagate.
int runWithPlatformEventLoop(const std::function<int()>& body);

} // namespace sony::transport
