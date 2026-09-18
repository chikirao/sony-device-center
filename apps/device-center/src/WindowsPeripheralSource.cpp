#include "WindowsPeripheralSource.h"

#include <QDebug>
#include <QFile>
#include <QHash>
#include <QUuid>
#include <QTextStream>

#include <algorithm>
#include <optional>
#include <string_view>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include <devpropdef.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Storage.Streams.h>
#endif

namespace sony::devicecenter {

// Runs on WindowsPeripheralSource's worker thread. One scan() per request;
// the result goes back as a plain list.
class PeripheralScanner : public QObject {
    Q_OBJECT
public slots:
    void start();
    void scan();
signals:
    void scanned(const QList<Peripheral>& peripherals);
};

#ifdef Q_OS_WIN
namespace {

using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using winrt::Windows::Foundation::IInspectable;

// Windows keeps the HFP-reported charge as a byte on the device node of the
// Hands-Free profile (BTHENUM\{0000111e-...}), not on the endpoint itself.
constexpr DEVPROPKEY kBatteryKey{{0x104EA319, 0x6EE2, 0x4701, {0xBD, 0x47, 0x8D, 0xDB, 0xF4, 0x25, 0xBB, 0xE5}}, 2};
// DEVPKEY_Device_ContainerId: the same GUID the endpoint reports as
// System.Devices.Aep.ContainerId, which is how a node is tied to a device.
constexpr DEVPROPKEY kContainerKey{{0x8C7ED206, 0x3F8A, 0x4827, {0xB3, 0xAB, 0xAE, 0x9E, 0x1F, 0xAE, 0xFC, 0x6C}}, 2};

template <typename T> std::optional<T> property(const DeviceInformation& info, const wchar_t* key) {
    const auto value = info.Properties().TryLookup(key);
    if (!value) return std::nullopt;
    if (const auto typed = value.try_as<winrt::Windows::Foundation::IReference<T>>()) return typed.Value();
    return std::nullopt;
}

// Any of the integer boxings the property system uses for small numbers.
std::optional<quint32> number(const DeviceInformation& info, const wchar_t* key) {
    if (const auto v = property<uint32_t>(info, key)) return *v;
    if (const auto v = property<uint16_t>(info, key)) return *v;
    if (const auto v = property<uint8_t>(info, key)) return *v;
    if (const auto v = property<int32_t>(info, key)) return static_cast<quint32>(*v);
    return std::nullopt;
}

QString guidText(const GUID& guid) {
    return QUuid(guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
                 guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]).toString(QUuid::WithoutBraces).toLower();
}

// Bluetooth Class of Device. Major 4 is audio/video, where minor 1 (headset),
// 2 (hands-free), 6 (headphones) and 7 (portable audio) are worn and the
// rest (speakers, car kits, TVs) are not; major 5 is a peripheral whose
// minor says keyboard / pointing device / gamepad. Windows hands the minor
// out already shifted (0..63), as seen on a WH-1000XM5 (4/1) and a
// DualShock (5/2).
PeripheralKind kindFromClassOfDevice(quint32 major, quint32 minor, const QString& name) {
    if (major == 4) {
        const auto byName = peripheralKindFromName(name);
        if (byName == PeripheralKind::Earbuds || byName == PeripheralKind::Headphones) return byName;
        return minor == 1 || minor == 2 || minor == 6 || minor == 7 ? PeripheralKind::Headphones : PeripheralKind::Other;
    }
    if (major == 5) {
        if ((minor & 0x30) == 0x20) return PeripheralKind::Mouse;
        if (minor & 0x10) return PeripheralKind::Keyboard;
        if (const auto sub = minor & 0x0F; sub == 1 || sub == 2 || sub == 3) return PeripheralKind::Gamepad;
    }
    return peripheralKindFromName(name);
}

// GAP Appearance: category 0x00F is HID with the device in the low bits,
// 0x025 is a wearable audio device.
PeripheralKind kindFromAppearance(quint32 appearance, const QString& name) {
    const auto category = appearance >> 6, sub = appearance & 0x3F;
    if (category == 0x0F) {
        if (sub == 1) return PeripheralKind::Keyboard;
        if (sub == 2) return PeripheralKind::Mouse;
        if (sub == 3 || sub == 4) return PeripheralKind::Gamepad;
    }
    if (category == 0x25) return sub == 1 ? PeripheralKind::Earbuds : PeripheralKind::Headphones;
    return peripheralKindFromName(name);
}

// container id -> charge, from every present device node that carries the
// battery property (one per connected HFP headset, in practice).
QHash<QString, int> batteryByContainer() {
    QHash<QString, int> out;
    const HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_PRESENT | DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return out;
    SP_DEVINFO_DATA data{};
    data.cbSize = sizeof(data);
    for (DWORD index = 0; SetupDiEnumDeviceInfo(set, index, &data); ++index) {
        DEVPROPTYPE type = 0;
        BYTE level = 0;
        DWORD size = 0;
        if (!SetupDiGetDevicePropertyW(set, &data, &kBatteryKey, &type, &level, sizeof(level), &size, 0) || type != DEVPROP_TYPE_BYTE)
            continue;
        GUID container{};
        if (!SetupDiGetDevicePropertyW(set, &data, &kContainerKey, &type, reinterpret_cast<PBYTE>(&container), sizeof(container), &size, 0)
            || type != DEVPROP_TYPE_GUID)
            continue;
        const auto key = guidText(container);
        // Several nodes of one device may report; keep the best-informed one.
        if (level <= 100) out[key] = std::max(out.value(key, -1), static_cast<int>(level));
    }
    SetupDiDestroyDeviceInfoList(set);
    return out;
}

// One read of the Battery Level characteristic; -1 when the device has no
// Battery Service or the read fails. Only called for connected devices, so
// this never wakes a sleeping peripheral.
int gattBatteryLevel(const winrt::hstring& id) {
    try {
        const auto device = BluetoothLEDevice::FromIdAsync(id).get();
        if (!device) return -1;
        const auto services = device.GetGattServicesForUuidAsync(GattServiceUuids::Battery(), BluetoothCacheMode::Cached).get();
        if (services.Status() != GattCommunicationStatus::Success || services.Services().Size() == 0) return -1;
        const auto characteristics = services.Services().GetAt(0)
            .GetCharacteristicsForUuidAsync(GattCharacteristicUuids::BatteryLevel(), BluetoothCacheMode::Cached).get();
        if (characteristics.Status() != GattCommunicationStatus::Success || characteristics.Characteristics().Size() == 0) return -1;
        const auto value = characteristics.Characteristics().GetAt(0).ReadValueAsync(BluetoothCacheMode::Uncached).get();
        if (value.Status() != GattCommunicationStatus::Success || value.Value().Length() == 0) return -1;
        const auto reader = winrt::Windows::Storage::Streams::DataReader::FromBuffer(value.Value());
        const int level = reader.ReadByte();
        return level <= 100 ? level : -1;
    } catch (const winrt::hresult_error& error) {
        qInfo().noquote() << "Peripheral battery (GATT) failed:" << QString::fromWCharArray(error.message().c_str());
        return -1;
    } catch (...) {
        return -1;
    }
}

QList<Peripheral> enumerate() {
    const auto properties = winrt::single_threaded_vector<winrt::hstring>({
        L"System.Devices.Aep.IsConnected", L"System.Devices.Aep.DeviceAddress", L"System.Devices.Aep.ContainerId",
        L"System.Devices.Aep.Bluetooth.Cod.Major", L"System.Devices.Aep.Bluetooth.Cod.Minor",
        L"System.Devices.Aep.Bluetooth.Le.Appearance"});
    // The stock selectors, not a hand-written ProtocolId/IsPaired query: they
    // add IssueInquiry:=False, without which the enumeration runs a live
    // 30-second inquiry before returning the very same paired list.
    QList<std::pair<bool, DeviceInformationCollection>> batches;
    batches.append({false, DeviceInformation::FindAllAsync(BluetoothDevice::GetDeviceSelectorFromPairingState(true), properties,
                                                            DeviceInformationKind::AssociationEndpoint).get()});
    batches.append({true, DeviceInformation::FindAllAsync(BluetoothLEDevice::GetDeviceSelectorFromPairingState(true), properties,
                                                           DeviceInformationKind::AssociationEndpoint).get()});

    // A dual-mode headset shows up in both batches; the classic endpoint
    // names it better and carries the class of device.
    QHash<QString, Peripheral> byAddress;
    QHash<QString, QString> containers;
    QHash<QString, winrt::hstring> leIds;
    QStringList order;
    for (const auto& [le, found] : batches) {
        for (const auto& info : found) {
            const auto address = property<winrt::hstring>(info, L"System.Devices.Aep.DeviceAddress");
            if (!address) continue;
            const auto key = normalizePeripheralAddress(QString::fromWCharArray(address->c_str()));
            if (key.size() != 17) continue;
            const auto name = QString::fromWCharArray(info.Name().c_str()).trimmed();
            const bool connected = property<bool>(info, L"System.Devices.Aep.IsConnected").value_or(false);

            auto& entry = byAddress[key];
            if (entry.address.isEmpty()) { entry.address = key; order.append(key); }
            entry.connected = entry.connected || connected;
            if (const auto container = property<winrt::guid>(info, L"System.Devices.Aep.ContainerId"); container && !containers.contains(key))
                containers[key] = guidText(*container);
            if (le) {
                if (connected) leIds[key] = info.Id();
                if (entry.name.isEmpty() && !name.isEmpty()) {
                    entry.name = name;
                    entry.kind = kindFromAppearance(number(info, L"System.Devices.Aep.Bluetooth.Le.Appearance").value_or(0), name);
                }
            } else if (!name.isEmpty()) {
                entry.name = name;
                entry.kind = kindFromClassOfDevice(number(info, L"System.Devices.Aep.Bluetooth.Cod.Major").value_or(0),
                                                   number(info, L"System.Devices.Aep.Bluetooth.Cod.Minor").value_or(0), name);
            }
        }
    }

    const auto levels = batteryByContainer();
    QList<Peripheral> out;
    for (const auto& key : order) {
        auto entry = byAddress.value(key);
        if (entry.name.isEmpty()) continue;
        entry.battery = levels.value(containers.value(key), -1);
        if (entry.battery < 0 && entry.connected && leIds.contains(key)) entry.battery = gattBatteryLevel(leIds.value(key));
        out.append(entry);
    }
    return out;
}

} // namespace

void PeripheralScanner::start() {
    // Blocking .get() on WinRT async operations needs a multithreaded
    // apartment; this thread is ours alone, so it gets one.
    try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
    catch (const winrt::hresult_error&) {}
}

void PeripheralScanner::scan() {
    QList<Peripheral> peripherals;
    QString failure;
    try {
        peripherals = enumerate();
    } catch (const winrt::hresult_error& error) {
        failure = QString::fromWCharArray(error.message().c_str()) + QString(" (0x%1)").arg(quint32(error.code()), 8, 16, QChar('0'));
    } catch (const std::exception& error) {
        failure = QString::fromUtf8(error.what());
    }
    if (!failure.isEmpty()) qWarning().noquote() << "Peripheral scan failed:" << failure;
    // Same diagnostic sink main() uses for the results.
    if (const auto logPath = qEnvironmentVariable("SONY_PERIPHERALS_LOG"); !logPath.isEmpty()) {
        QFile file(logPath);
        if (file.open(QIODevice::Append | QIODevice::Text))
            QTextStream(&file) << "scan: " << peripherals.size() << " devices" << (failure.isEmpty() ? "" : ", failed: " + failure) << "\n";
    }
    emit scanned(peripherals);
}

bool WindowsPeripheralSource::isAvailable() const { return true; }

#else

void PeripheralScanner::start() {}
void PeripheralScanner::scan() { emit scanned({}); }
bool WindowsPeripheralSource::isAvailable() const { return false; }

#endif

WindowsPeripheralSource::WindowsPeripheralSource(QObject* parent) : IPeripheralSource(parent) {
    _scanner = new PeripheralScanner;
    _scanner->moveToThread(&_thread);
    connect(&_thread, &QThread::started, _scanner, &PeripheralScanner::start);
    connect(&_thread, &QThread::finished, _scanner, &QObject::deleteLater);
    connect(this, &WindowsPeripheralSource::scanRequested, _scanner, &PeripheralScanner::scan);
    connect(_scanner, &PeripheralScanner::scanned, this, &WindowsPeripheralSource::_apply);
    _thread.setObjectName("peripheral-scan");
    _thread.start();

    _poll.setTimerType(Qt::VeryCoarseTimer);
    connect(&_poll, &QTimer::timeout, this, &WindowsPeripheralSource::refresh);
    setPollInterval(30);
    refresh();
}

WindowsPeripheralSource::~WindowsPeripheralSource() {
    _thread.quit();
    _thread.wait();
}

void WindowsPeripheralSource::setPollInterval(int seconds) {
    if (seconds <= 0) { _poll.stop(); return; }
    _poll.start(seconds * 1000);
}

void WindowsPeripheralSource::refresh() {
    // One scan in flight at a time; a request that arrives meanwhile queues
    // exactly one more, so a burst of link events costs two scans, not ten.
    if (_scanning) { _again = true; return; }
    _scanning = true;
    emit scanRequested();
}

void WindowsPeripheralSource::_apply(const QList<Peripheral>& peripherals) {
    _scanning = false;
    ++_scanCount;
    if (peripherals != _peripherals) {
        _peripherals = peripherals;
        emit changed();
    }
    if (_again) { _again = false; refresh(); }
}

} // namespace sony::devicecenter

#include "WindowsPeripheralSource.moc"
