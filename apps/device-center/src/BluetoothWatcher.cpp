#include "BluetoothWatcher.h"

#include <QDebug>

#ifdef Q_OS_WIN
// initguid.h first, so the Bluetooth GUIDs are defined here rather than
// merely declared.
#include <initguid.h>
#include <windows.h>
#include <bluetoothapis.h>
#include <bthdef.h>
#include <dbt.h>
#endif

namespace sony::devicecenter {

namespace {
#ifdef Q_OS_WIN
constexpr wchar_t kWindowClass[] = L"SonyDeviceCenterBluetoothWatcher";

LRESULT CALLBACK watcherProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message != WM_DEVICECHANGE || !lParam) return DefWindowProcW(hwnd, message, wParam, lParam);
    auto* watcher = reinterpret_cast<BluetoothWatcher*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    const auto* header = reinterpret_cast<const DEV_BROADCAST_HDR*>(lParam);
    if (wParam == DBT_CUSTOMEVENT && header->dbch_devicetype == DBT_DEVTYP_HANDLE) {
        // An HCI connection event on one of the watched radios.
        const auto* handle = reinterpret_cast<const DEV_BROADCAST_HANDLE*>(lParam);
        if (watcher && IsEqualGUID(handle->dbch_eventguid, GUID_BLUETOOTH_HCI_EVENT)) {
            const auto* info = reinterpret_cast<const BTH_HCI_EVENT_INFO*>(handle->dbch_data);
            emit watcher->connectionChanged(BluetoothWatcher::formatAddress(info->bthAddress), info->connected != 0);
        }
    } else if ((wParam == DBT_DEVICEARRIVAL || wParam == DBT_DEVICEREMOVECOMPLETE)
               && header->dbch_devicetype == DBT_DEVTYP_DEVICEINTERFACE) {
        // A radio appeared or went away (dongle plugged in, Bluetooth toggled
        // in Settings): start over with whatever radios exist now.
        if (watcher) watcher->rescanRadios();
    }
    return TRUE;
}
#endif
}

BluetoothWatcher::BluetoothWatcher(QObject* parent) : QObject(parent) { _start(); }

BluetoothWatcher::~BluetoothWatcher() { _stop(); }

QString BluetoothWatcher::formatAddress(quint64 address) {
    QString out;
    for (int shift = 40; shift >= 0; shift -= 8) {
        if (!out.isEmpty()) out += ':';
        out += QStringLiteral("%1").arg((address >> shift) & 0xff, 2, 16, QLatin1Char('0')).toUpper();
    }
    return out;
}

void BluetoothWatcher::_start() {
#ifdef Q_OS_WIN
    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = watcherProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.lpszClassName = kWindowClass;
    RegisterClassW(&windowClass);  // fails harmlessly when already registered
    auto* window = CreateWindowExW(0, kWindowClass, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, nullptr, windowClass.hInstance, nullptr);
    if (!window) { qWarning() << "Bluetooth watcher: CreateWindowExW failed:" << GetLastError(); return; }
    SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    _window = window;

    // Radio arrivals and removals, so a radio switched on after startup is
    // picked up too.
    DEV_BROADCAST_DEVICEINTERFACE_W radios{};
    radios.dbcc_size = sizeof(radios);
    radios.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
    radios.dbcc_classguid = GUID_BTHPORT_DEVICE_INTERFACE;
    _radioNotification = RegisterDeviceNotificationW(window, &radios, DEVICE_NOTIFY_WINDOW_HANDLE);

    rescanRadios();
#endif
}

void BluetoothWatcher::rescanRadios() {
    _unwatchRadios();
#ifdef Q_OS_WIN
    BLUETOOTH_FIND_RADIO_PARAMS params{sizeof(BLUETOOTH_FIND_RADIO_PARAMS)};
    HANDLE radio = nullptr;
    // A failed find (ERROR_NO_MORE_ITEMS) simply means Bluetooth is off or absent.
    HBLUETOOTH_RADIO_FIND find = _window ? BluetoothFindFirstRadio(&params, &radio) : nullptr;
    if (find) {
        do {
            DEV_BROADCAST_HANDLE filter{};
            filter.dbch_size = sizeof(filter);
            filter.dbch_devicetype = DBT_DEVTYP_HANDLE;
            filter.dbch_handle = radio;
            HDEVNOTIFY notification = RegisterDeviceNotificationW(static_cast<HWND>(_window), &filter, DEVICE_NOTIFY_WINDOW_HANDLE);
            if (notification) _registrations.push_back({radio, notification});
            else { qWarning() << "Bluetooth watcher: RegisterDeviceNotificationW failed:" << GetLastError(); CloseHandle(radio); }
        } while (BluetoothFindNextRadio(find, &radio));
        BluetoothFindRadioClose(find);
    }
#endif
    emit availabilityChanged();
}

void BluetoothWatcher::_unwatchRadios() {
#ifdef Q_OS_WIN
    for (auto& registration : _registrations) {
        UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(registration.notification));
        CloseHandle(static_cast<HANDLE>(registration.radio));
    }
#endif
    _registrations.clear();
}

void BluetoothWatcher::_stop() {
    _unwatchRadios();
#ifdef Q_OS_WIN
    if (_radioNotification) { UnregisterDeviceNotification(static_cast<HDEVNOTIFY>(_radioNotification)); _radioNotification = nullptr; }
    if (_window) {
        SetWindowLongPtrW(static_cast<HWND>(_window), GWLP_USERDATA, 0);
        DestroyWindow(static_cast<HWND>(_window));
        _window = nullptr;
    }
#endif
}

} // namespace sony::devicecenter
