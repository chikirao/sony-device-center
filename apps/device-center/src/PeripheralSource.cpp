#include "PeripheralSource.h"
#include "WindowsPeripheralSource.h"

#include <QRegularExpression>

#include <utility>

namespace sony::devicecenter {

QString peripheralKindName(PeripheralKind kind) {
    switch (kind) {
    case PeripheralKind::Headphones: return QStringLiteral("headphones");
    case PeripheralKind::Earbuds: return QStringLiteral("earbuds");
    case PeripheralKind::Mouse: return QStringLiteral("mouse");
    case PeripheralKind::Keyboard: return QStringLiteral("keyboard");
    case PeripheralKind::Gamepad: return QStringLiteral("gamepad");
    case PeripheralKind::Other: break;
    }
    return QStringLiteral("other");
}

PeripheralKind peripheralKindFromName(const QString& name) {
    const auto lower = name.toLower();
    auto has = [&lower](std::initializer_list<const char*> words) {
        for (const auto* word : words)
            if (lower.contains(QLatin1String(word))) return true;
        return false;
    };
    // Sony's own families first: the model prefix is more reliable than the
    // generic words further down.
    if (lower.startsWith("wf-") || has({"linkbuds", "buds", "earbud", "airpods"})) return PeripheralKind::Earbuds;
    if (lower.startsWith("wh-") || lower.startsWith("wi-") || lower.startsWith("mdr-") || lower.startsWith("ult wear")
        || has({"headphone", "headset"})) return PeripheralKind::Headphones;
    if (has({"dualsense", "dualshock", "xbox", "gamepad", "controller", "joy-con", "stadia"})) return PeripheralKind::Gamepad;
    if (has({"keyboard", "keys", "keychron", "k380", "k780"})) return PeripheralKind::Keyboard;
    if (has({"mouse", "mx master", "mx anywhere", "trackpad", "trackball", "pebble"})) return PeripheralKind::Mouse;
    return PeripheralKind::Other;
}

QString normalizePeripheralAddress(const QString& address) {
    QString hex;
    for (const auto ch : address)
        if (ch.isLetterOrNumber()) hex += ch.toUpper();
    if (hex.size() != 12) return address.toUpper();
    QString out;
    for (int i = 0; i < 12; i += 2) {
        if (!out.isEmpty()) out += ':';
        out += hex.mid(i, 2);
    }
    return out;
}

void IPeripheralSource::refreshLater(int delayMs) {
    if (!_later) {
        _later = new QTimer(this);
        _later->setSingleShot(true);
        connect(_later, &QTimer::timeout, this, &IPeripheralSource::refresh);
    }
    _later->start(delayMs);
}

void FakePeripheralSource::setPeripherals(QList<Peripheral> peripherals) {
    for (auto& p : peripherals) p.address = normalizePeripheralAddress(p.address);
    if (peripherals == _peripherals) return;
    _peripherals = std::move(peripherals);
    emit changed();
}

void FakePeripheralSource::update(const QString& address, const std::function<void(Peripheral&)>& change) {
    const auto key = normalizePeripheralAddress(address);
    for (auto& p : _peripherals) {
        if (p.address != key) continue;
        const auto before = p;
        change(p);
        p.address = key;
        if (!(before == p)) emit changed();
        return;
    }
}

QList<Peripheral> FakePeripheralSource::simulatedSet() {
    return {
        {.address = "F4:73:35:AA:10:01", .name = "MX Master 3S", .kind = PeripheralKind::Mouse, .connected = true, .battery = 50},
        {.address = "F4:73:35:AA:10:02", .name = "Keychron K3", .kind = PeripheralKind::Keyboard, .connected = true, .battery = 100},
        {.address = "F4:73:35:AA:10:03", .name = "DualSense Wireless Controller", .kind = PeripheralKind::Gamepad,
         .connected = false, .battery = 90},
    };
}

IPeripheralSource* createPlatformPeripheralSource(QObject* parent) {
#ifdef Q_OS_WIN
    return new WindowsPeripheralSource(parent);
#else
    return new NullPeripheralSource(parent);
#endif
}

} // namespace sony::devicecenter
