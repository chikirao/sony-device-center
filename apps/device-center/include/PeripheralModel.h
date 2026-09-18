#pragma once

#include "PeripheralSource.h"

#include <QAbstractListModel>
#include <QHash>
#include <QVariantMap>

namespace sony::devicecenter {

class DeviceCenterController;

// The Device Hub's list: every Sony set the controller knows about, with the
// live state of the one it is talking to, plus whatever else the OS reports
// through an IPeripheralSource. Rows merge by address and the Sony side
// wins: the controller's battery and connection state beat the OS's, the OS
// only fills in what the controller cannot see (a paired set's charge as
// reported over HFP, say). Connected devices sort first (the active set at
// the very top), Sony ones before the rest, then by name. Updates are incremental (insert / move / remove /
// dataChanged), never a reset, so the hub's rows keep their identity while
// it is open.
class PeripheralModel : public QAbstractListModel {
    Q_OBJECT
    Q_PROPERTY(int count READ rowCount NOTIFY countChanged)
    Q_PROPERTY(int connectedCount READ connectedCount NOTIFY countChanged)
    Q_PROPERTY(bool systemSourceAvailable READ systemSourceAvailable CONSTANT)
public:
    enum Role {
        AddressRole = Qt::UserRole + 1,
        NameRole,
        KindRole,          // "headphones" | "earbuds" | "mouse" | "keyboard" | "gamepad" | "other"
        ConnectedRole,
        BatteryRole,       // -1 when unknown
        BatteryLeftRole,
        BatteryRightRole,
        BatteryCaseRole,
        HasDualBatteryRole,
        SonyRole,          // known to the controller (paired list or active)
        ActiveRole,        // the set the controller is connected to right now
        ChargingRole,
        CodecRole,         // active Sony only, else ""
        NoiseModeRole,     // active Sony only: "cancelling" | "ambient" | "off" | "unknown"
    };
    Q_ENUM(Role)

    // One merged line as the view sees it.
    struct Row {
        Peripheral peripheral;
        bool sony{false};
        bool active{false};
        bool charging{false};
        QString codec;
        QString noiseMode;
        bool operator==(const Row&) const = default;
        // Rows without an address (an early snapshot) fall back to the name.
        [[nodiscard]] QString key() const;
    };

    PeripheralModel(DeviceCenterController& controller, IPeripheralSource& source, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int connectedCount() const;
    [[nodiscard]] bool systemSourceAvailable() const { return _source.isAvailable(); }
    [[nodiscard]] const QList<Row>& rows() const { return _rows; }
    // Every role of one row as a map; handy from QML outside a delegate.
    Q_INVOKABLE QVariantMap get(int row) const;
    Q_INVOKABLE int indexOf(const QString& address) const;
    // Forwarded to the source: re-enumerate the OS now.
    Q_INVOKABLE void refresh();

signals:
    void countChanged();

private:
    [[nodiscard]] QList<Row> _compose() const;
    void _rebuild();

    DeviceCenterController& _controller;
    IPeripheralSource& _source;
    QList<Row> _rows;
    int _lastConnected{0};
    int _lastCount{0};
};

} // namespace sony::devicecenter
