#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVector>

namespace sony::devicecenter {

class DeviceCenterController;

// Named equalizer curves kept on the PC. The headphones have a single custom
// slot; this is the list of curves the user can put into it. Stored as one
// JSON file in the config directory, importable/exportable in the same
// format so curves can be shared between machines.
class EqualizerLibrary : public QObject {
    Q_OBJECT
    // [{id, name, clearBass, bands: [5 ints]}] in user order.
    Q_PROPERTY(QVariantList presets READ presets NOTIFY presetsChanged)
    // Id of the preset whose curve the headphones currently play, "" when the
    // device is on a built-in preset or the custom curve matches nothing.
    Q_PROPERTY(QString activeId READ activeId NOTIFY activeChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    struct Preset {
        QString id;
        QString name;
        int clearBass{0};
        QVector<int> bands;
    };

    static constexpr int kBandCount = 5;
    static constexpr int kMinGain = -10;
    static constexpr int kMaxGain = 10;
    // The headphones' preset id for "custom curve".
    static constexpr int kCustomPreset = 0xa0;
    static constexpr const char* kFormat = "sony-device-center-eq";
    static constexpr int kFormatVersion = 1;

    // storageDir: where equalizer-presets.json lives; defaults to
    // AppConfigLocation. Tests point it at a temporary directory.
    explicit EqualizerLibrary(DeviceCenterController& controller, const QString& storageDir = {},
                              QObject* parent = nullptr);

    [[nodiscard]] QVariantList presets() const;
    [[nodiscard]] const QVector<Preset>& list() const { return _presets; }
    [[nodiscard]] QString activeId() const { return _activeId; }
    [[nodiscard]] QString lastError() const { return _lastError; }
    [[nodiscard]] QString filePath() const { return _file; }

    // Adds a curve and returns its id. An empty name becomes "Preset N";
    // a taken name gets a " (2)" style suffix. Invalid bands return "".
    Q_INVOKABLE QString save(const QString& name, int clearBass, const QVariantList& bands);
    // The curve the headphones play right now, under the given name.
    Q_INVOKABLE QString saveCurrent(const QString& name);
    Q_INVOKABLE bool rename(const QString& id, const QString& name);
    // Overwrites a preset's curve with what the headphones play right now.
    Q_INVOKABLE bool updateFromCurrent(const QString& id);
    Q_INVOKABLE bool remove(const QString& id);
    // Sends the curve to the headphones (custom slot).
    Q_INVOKABLE void apply(const QString& id);

    // JSON file exchange. Paths are local file paths; an empty id in
    // exportPreset writes the whole library. importFile returns how many
    // presets were added, or -1 with lastError set.
    Q_INVOKABLE bool exportPreset(const QString& id, const QString& path);
    Q_INVOKABLE int importFile(const QString& path);
    // The same through native file dialogs; no-ops in tests.
    Q_INVOKABLE void exportWithDialog(const QString& id);
    Q_INVOKABLE void importWithDialog();

    // Serialised form of one preset or of the whole library (empty id).
    [[nodiscard]] QByteArray toJson(const QString& id = {}) const;

signals:
    void presetsChanged();
    void activeChanged();
    void lastErrorChanged();

private:
    static bool _validBands(const QVariantList& bands, QVector<int>& out);
    static bool _validGain(int gain) { return gain >= kMinGain && gain <= kMaxGain; }
    int _indexOf(const QString& id) const;
    QString _uniqueName(const QString& wanted, const QString& skipId = {}) const;
    void _load();
    void _persist();
    void _refreshActive();
    void _setError(const QString& message);

    DeviceCenterController& _controller;
    QString _file;
    QVector<Preset> _presets;
    QString _activeId;
    QString _lastError;
};

} // namespace sony::devicecenter
