#include "PeripheralModel.h"
#include "DeviceCenterController.h"
#include "sony/transport/SonyDeviceFilter.h"

#include <algorithm>

namespace sony::devicecenter {

QString PeripheralModel::Row::key() const {
    return peripheral.address.isEmpty() ? "name:" + peripheral.name : peripheral.address;
}

PeripheralModel::PeripheralModel(DeviceCenterController& controller, IPeripheralSource& source, QObject* parent)
    : QAbstractListModel(parent), _controller(controller), _source(source) {
    connect(&_controller, &DeviceCenterController::stateChanged, this, &PeripheralModel::_rebuild);
    connect(&_controller, &DeviceCenterController::pairedDevicesChanged, this, &PeripheralModel::_rebuild);
    connect(&_source, &IPeripheralSource::changed, this, &PeripheralModel::_rebuild);
    connect(&_controller, &DeviceCenterController::aliasesChanged, this, &PeripheralModel::_rebuild);
    _rows = _compose();
    _lastConnected = connectedCount();
    _lastCount = int(_rows.size());
}

int PeripheralModel::rowCount(const QModelIndex& parent) const { return parent.isValid() ? 0 : int(_rows.size()); }

QVariant PeripheralModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _rows.size()) return {};
    const auto& row = _rows[index.row()];
    const auto& p = row.peripheral;
    switch (role) {
    case AddressRole: return p.address;
    case NameRole: return p.name;
    case KindRole: return peripheralKindName(p.kind);
    case ConnectedRole: return p.connected;
    case BatteryRole: return p.battery;
    case BatteryLeftRole: return p.batteryLeft;
    case BatteryRightRole: return p.batteryRight;
    case BatteryCaseRole: return p.batteryCase;
    case HasDualBatteryRole: return p.batteryLeft >= 0 || p.batteryRight >= 0;
    case SonyRole: return row.sony;
    case ActiveRole: return row.active;
    case ChargingRole: return row.charging;
    case CodecRole: return row.codec;
    case NoiseModeRole: return row.noiseMode;
    case ModelNameRole: return row.modelName;
    }
    return {};
}

QHash<int, QByteArray> PeripheralModel::roleNames() const {
    return {
        {AddressRole, "address"}, {NameRole, "name"}, {KindRole, "kind"}, {ConnectedRole, "connected"},
        {BatteryRole, "battery"}, {BatteryLeftRole, "batteryLeft"}, {BatteryRightRole, "batteryRight"},
        {BatteryCaseRole, "batteryCase"}, {HasDualBatteryRole, "hasDualBattery"}, {SonyRole, "sony"},
        {ActiveRole, "active"}, {ChargingRole, "charging"}, {CodecRole, "codec"}, {NoiseModeRole, "noiseMode"},
        {ModelNameRole, "modelName"},
    };
}

int PeripheralModel::connectedCount() const {
    return int(std::count_if(_rows.begin(), _rows.end(), [](const Row& r) { return r.peripheral.connected; }));
}

QVariantMap PeripheralModel::get(int row) const {
    QVariantMap out;
    if (row < 0 || row >= _rows.size()) return out;
    const auto names = roleNames();
    for (auto it = names.begin(); it != names.end(); ++it) out.insert(QString::fromUtf8(it.value()), data(index(row), it.key()));
    return out;
}

int PeripheralModel::indexOf(const QString& address) const {
    const auto key = normalizePeripheralAddress(address);
    for (int i = 0; i < _rows.size(); ++i)
        if (_rows[i].peripheral.address == key) return i;
    return -1;
}

void PeripheralModel::refresh() { _source.refresh(); }

void PeripheralModel::setIncludeSystem(bool on) {
    if (_includeSystem == on) return;
    _includeSystem = on;
    emit includeSystemChanged();
    _rebuild();
}

QList<PeripheralModel::Row> PeripheralModel::_compose() const {
    QList<Row> rows;
    auto find = [&rows](const QString& address) -> Row* {
        if (address.isEmpty()) return nullptr;
        for (auto& row : rows)
            if (row.peripheral.address == address) return &row;
        return nullptr;
    };

    // Sony sets the controller knows: the paired list from discovery...
    for (const auto& entry : _controller.pairedDevices()) {
        const auto map = entry.toMap();
        Row row;
        row.sony = true;
        row.peripheral.address = normalizePeripheralAddress(map.value("address").toString());
        row.peripheral.name = map.value("name").toString();
        const auto kind = peripheralKindFromName(row.peripheral.name);
        row.peripheral.kind = kind == PeripheralKind::Other ? PeripheralKind::Headphones : kind;
        row.peripheral.connected = map.value("systemConnected").toBool();
        if (find(row.peripheral.address)) continue;
        rows.append(row);
    }

    // ...and the one it is connected to, whose live state is authoritative.
    const auto activeAddress = normalizePeripheralAddress(_controller.deviceAddress());
    if (_controller.isConnected() && !_controller.deviceName().isEmpty()) {
        Row* row = find(activeAddress);
        if (!row) {
            // Match by name when the snapshot has no address yet (or the
            // discovery does not list what we are talking to).
            for (auto& candidate : rows)
                if (activeAddress.isEmpty() && candidate.peripheral.name == _controller.deviceName()) { row = &candidate; break; }
        }
        if (!row) {
            rows.append(Row{.sony = true});
            row = &rows.last();
            row->peripheral.address = activeAddress;
            row->peripheral.name = _controller.deviceName();
        }
        auto& p = row->peripheral;
        row->active = true;
        p.connected = true;
        p.battery = _controller.batteryLevel();
        p.batteryLeft = _controller.batteryLeft();
        p.batteryRight = _controller.batteryRight();
        p.batteryCase = _controller.batteryCase();
        p.kind = _controller.hasDualBattery() ? PeripheralKind::Earbuds
               : peripheralKindFromName(p.name) == PeripheralKind::Earbuds ? PeripheralKind::Earbuds : PeripheralKind::Headphones;
        row->charging = _controller.isCharging();
        row->codec = _controller.codec() == "Unknown" ? QString() : _controller.codec();
        row->noiseMode = _controller.noiseControlMode();
    }

    // Everything the OS reports; Sony rows only take what they lack. A Sony
    // set the controller does not list (nothing connected yet on a platform
    // whose discovery is empty) is still recognised as Sony by its address
    // prefix or name, so "Sony only" keeps it.
    for (const auto& system : _source.peripherals()) {
        const auto address = normalizePeripheralAddress(system.address);
        const bool sony = transport::hasSonyOui(address.toStdString()) || transport::hasSonyName(system.name.toStdString());
        if (!_includeSystem && !sony && !find(address)) continue;
        if (Row* row = find(address)) {
            auto& p = row->peripheral;
            p.connected = p.connected || system.connected;
            if (p.battery < 0) p.battery = system.battery;
            if (p.batteryLeft < 0 && p.batteryRight < 0) {
                p.batteryLeft = system.batteryLeft;
                p.batteryRight = system.batteryRight;
                p.batteryCase = system.batteryCase;
            }
            continue;
        }
        Row row;
        row.sony = sony;
        row.peripheral = system;
        row.peripheral.address = address;
        rows.append(row);
    }

    // The user's names go on last: matching above is by what devices report.
    for (auto& row : rows) {
        row.modelName = row.peripheral.name;
        if (const auto alias = _controller.aliasFor(row.peripheral.address); !alias.isEmpty()) row.peripheral.name = alias;
    }

    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) {
        if (a.peripheral.connected != b.peripheral.connected) return a.peripheral.connected;
        if (a.active != b.active) return a.active;
        if (a.sony != b.sony) return a.sony;
        if (const int byName = a.peripheral.name.compare(b.peripheral.name, Qt::CaseInsensitive)) return byName < 0;
        return a.peripheral.address < b.peripheral.address;
    });
    return rows;
}

void PeripheralModel::_rebuild() {
    const auto fresh = _compose();
    auto indexOfKey = [](const QList<Row>& rows, const QString& key, int from) {
        for (int i = from; i < rows.size(); ++i)
            if (rows[i].key() == key) return i;
        return -1;
    };

    // Rows that vanished go first, so the walk below only has to insert,
    // move and update.
    for (int i = int(_rows.size()) - 1; i >= 0; --i) {
        if (indexOfKey(fresh, _rows[i].key(), 0) >= 0) continue;
        beginRemoveRows({}, i, i);
        _rows.removeAt(i);
        endRemoveRows();
    }
    for (int i = 0; i < fresh.size(); ++i) {
        const auto& wanted = fresh[i];
        if (i < _rows.size() && _rows[i].key() == wanted.key()) {
            if (!(_rows[i] == wanted)) {
                _rows[i] = wanted;
                emit dataChanged(index(i), index(i));
            }
            continue;
        }
        if (const int from = indexOfKey(_rows, wanted.key(), i + 1); from >= 0) {
            beginMoveRows({}, from, from, {}, i);
            _rows.move(from, i);
            endMoveRows();
            if (!(_rows[i] == wanted)) {
                _rows[i] = wanted;
                emit dataChanged(index(i), index(i));
            }
            continue;
        }
        beginInsertRows({}, i, i);
        _rows.insert(i, wanted);
        endInsertRows();
    }

    if (const int connected = connectedCount(); connected != _lastConnected || _rows.size() != _lastCount) {
        _lastConnected = connected;
        _lastCount = int(_rows.size());
        emit countChanged();
    }
}

} // namespace sony::devicecenter
