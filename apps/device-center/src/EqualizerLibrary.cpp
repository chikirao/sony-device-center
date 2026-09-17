#include "EqualizerLibrary.h"
#include "DeviceCenterController.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QVariantMap>

namespace sony::devicecenter {

namespace {
constexpr auto kFileName = "equalizer-presets.json";

QJsonObject presetToJson(const EqualizerLibrary::Preset& preset) {
    QJsonArray bands;
    for (const int gain : preset.bands) bands.append(gain);
    return {{"name", preset.name}, {"clearBass", preset.clearBass}, {"bands", bands}};
}

QVariantList bandsToVariant(const QVector<int>& bands) {
    QVariantList out;
    for (const int gain : bands) out.append(gain);
    return out;
}
}

EqualizerLibrary::EqualizerLibrary(DeviceCenterController& controller, const QString& storageDir, QObject* parent)
    : QObject(parent), _controller(controller) {
    const auto dir = storageDir.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) : storageDir;
    _file = QDir(dir).filePath(kFileName);
    _load();
    connect(&_controller, &DeviceCenterController::stateChanged, this, &EqualizerLibrary::_refreshActive);
    _refreshActive();
}

QVariantList EqualizerLibrary::presets() const {
    QVariantList out;
    for (const auto& preset : _presets)
        out.append(QVariantMap{{"id", preset.id}, {"name", preset.name}, {"clearBass", preset.clearBass},
                               {"bands", bandsToVariant(preset.bands)}});
    return out;
}

bool EqualizerLibrary::_validBands(const QVariantList& bands, QVector<int>& out) {
    if (bands.size() != kBandCount) return false;
    out.clear();
    for (const auto& value : bands) {
        bool ok = false;
        const int gain = value.toInt(&ok);
        if (!ok || !_validGain(gain)) return false;
        out.append(gain);
    }
    return true;
}

int EqualizerLibrary::_indexOf(const QString& id) const {
    for (int i = 0; i < _presets.size(); ++i)
        if (_presets[i].id == id) return i;
    return -1;
}

QString EqualizerLibrary::_uniqueName(const QString& wanted, const QString& skipId) const {
    auto taken = [&](const QString& candidate) {
        for (const auto& preset : _presets)
            if (preset.id != skipId && preset.name.compare(candidate, Qt::CaseInsensitive) == 0) return true;
        return false;
    };
    QString base = wanted.trimmed();
    if (base.isEmpty()) base = QStringLiteral("Preset %1").arg(_presets.size() + 1);
    QString candidate = base;
    for (int n = 2; taken(candidate); ++n) candidate = QStringLiteral("%1 (%2)").arg(base).arg(n);
    return candidate;
}

QString EqualizerLibrary::save(const QString& name, int clearBass, const QVariantList& bands) {
    Preset preset;
    if (!_validBands(bands, preset.bands) || !_validGain(clearBass)) {
        _setError(QStringLiteral("invalid curve"));
        return {};
    }
    preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    preset.name = _uniqueName(name);
    preset.clearBass = clearBass;
    _presets.append(preset);
    _persist();
    emit presetsChanged();
    _refreshActive();
    return preset.id;
}

QString EqualizerLibrary::saveCurrent(const QString& name) {
    return save(name, _controller.clearBass(), _controller.equalizerBands());
}

bool EqualizerLibrary::rename(const QString& id, const QString& name) {
    const int i = _indexOf(id);
    if (i < 0) return false;
    const auto next = _uniqueName(name, id);
    if (_presets[i].name == next) return true;
    _presets[i].name = next;
    _persist();
    emit presetsChanged();
    return true;
}

bool EqualizerLibrary::updateFromCurrent(const QString& id) {
    const int i = _indexOf(id);
    if (i < 0) return false;
    QVector<int> bands;
    if (!_validBands(_controller.equalizerBands(), bands) || !_validGain(_controller.clearBass())) return false;
    _presets[i].bands = bands;
    _presets[i].clearBass = _controller.clearBass();
    _persist();
    emit presetsChanged();
    _refreshActive();
    return true;
}

bool EqualizerLibrary::remove(const QString& id) {
    const int i = _indexOf(id);
    if (i < 0) return false;
    _presets.removeAt(i);
    _persist();
    emit presetsChanged();
    _refreshActive();
    return true;
}

void EqualizerLibrary::apply(const QString& id) {
    const int i = _indexOf(id);
    if (i < 0) return;
    _controller.setEqualizerCustom(_presets[i].clearBass, bandsToVariant(_presets[i].bands));
}

QByteArray EqualizerLibrary::toJson(const QString& id) const {
    QJsonArray presets;
    for (const auto& preset : _presets)
        if (id.isEmpty() || preset.id == id) presets.append(presetToJson(preset));
    return QJsonDocument(QJsonObject{{"format", kFormat}, {"version", kFormatVersion}, {"presets", presets}})
        .toJson(QJsonDocument::Indented);
}

bool EqualizerLibrary::exportPreset(const QString& id, const QString& path) {
    if (!id.isEmpty() && _indexOf(id) < 0) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(toJson(id)) < 0 || !file.commit()) {
        _setError(file.errorString());
        return false;
    }
    _setError({});
    return true;
}

int EqualizerLibrary::importFile(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { _setError(file.errorString()); return -1; }
    QJsonParseError error{};
    const auto doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) { _setError(error.errorString()); return -1; }

    // Either the library envelope or a bare preset object / array of them.
    QJsonArray items;
    if (doc.isArray()) items = doc.array();
    else if (doc.object().contains("presets")) items = doc.object().value("presets").toArray();
    else if (doc.isObject()) items.append(doc.object());

    int added = 0;
    for (const auto& item : items) {
        const auto object = item.toObject();
        Preset preset;
        if (!_validBands(object.value("bands").toArray().toVariantList(), preset.bands)) continue;
        preset.clearBass = object.value("clearBass").toInt();
        if (!_validGain(preset.clearBass)) continue;
        preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        preset.name = _uniqueName(object.value("name").toString());
        _presets.append(preset);
        ++added;
    }
    if (added == 0) { _setError(QStringLiteral("no presets in file")); return 0; }
    _setError({});
    _persist();
    emit presetsChanged();
    _refreshActive();
    return added;
}

void EqualizerLibrary::exportWithDialog(const QString& id) {
    const int i = _indexOf(id);
    const auto suggested = i >= 0 ? _presets[i].name : QStringLiteral("equalizer-presets");
    const auto path = QFileDialog::getSaveFileName(nullptr, QString(), suggested + ".json",
                                                   QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty()) exportPreset(id, path);
}

void EqualizerLibrary::importWithDialog() {
    const auto path = QFileDialog::getOpenFileName(nullptr, QString(), QString(), QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty()) importFile(path);
}

void EqualizerLibrary::_load() {
    _presets.clear();
    QFile file(_file);
    if (!file.open(QIODevice::ReadOnly)) return;
    const auto doc = QJsonDocument::fromJson(file.readAll());
    for (const auto& item : doc.object().value("presets").toArray()) {
        const auto object = item.toObject();
        Preset preset;
        if (!_validBands(object.value("bands").toArray().toVariantList(), preset.bands)) continue;
        preset.clearBass = object.value("clearBass").toInt();
        if (!_validGain(preset.clearBass)) continue;
        preset.id = object.value("id").toString();
        if (preset.id.isEmpty()) preset.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        preset.name = _uniqueName(object.value("name").toString());
        _presets.append(preset);
    }
}

void EqualizerLibrary::_persist() {
    QDir().mkpath(QFileInfo(_file).absolutePath());
    QJsonArray presets;
    for (const auto& preset : _presets) {
        auto object = presetToJson(preset);
        object.insert("id", preset.id);
        presets.append(object);
    }
    QSaveFile file(_file);
    if (!file.open(QIODevice::WriteOnly)) { _setError(file.errorString()); return; }
    file.write(QJsonDocument(QJsonObject{{"format", kFormat}, {"version", kFormatVersion}, {"presets", presets}})
                   .toJson(QJsonDocument::Indented));
    if (!file.commit()) _setError(file.errorString());
}

// The active preset is a pure function of the headphones' state: the custom
// slot, and its curve equal to one of ours.
void EqualizerLibrary::_refreshActive() {
    QString next;
    if (_controller.isConnected() && _controller.equalizerPreset() == kCustomPreset) {
        QVector<int> bands;
        if (_validBands(_controller.equalizerBands(), bands)) {
            for (const auto& preset : _presets) {
                if (preset.bands == bands && preset.clearBass == _controller.clearBass()) { next = preset.id; break; }
            }
        }
    }
    if (next == _activeId) return;
    _activeId = next;
    emit activeChanged();
}

void EqualizerLibrary::_setError(const QString& message) {
    if (_lastError == message) return;
    _lastError = message;
    emit lastErrorChanged();
}

} // namespace sony::devicecenter
