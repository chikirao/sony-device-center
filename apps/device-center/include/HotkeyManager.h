#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>
#include <QVariantList>

#include <array>
#include <optional>

namespace sony::devicecenter {

class DeviceCenterController;
class TrayController;

// System-wide keyboard shortcuts for the tray-menu actions. Bindings live in
// QSettings and are off by default. Only Windows (RegisterHotKey) has a
// backend today; elsewhere the settings still load and save but nothing is
// registered and supported() is false, so the UI can say so.
class HotkeyManager : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
    Q_PROPERTY(bool supported READ supported CONSTANT)
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY bindingsChanged)

public:
    enum class Action { ToggleNoiseControl, NoiseControlOff, ToggleSpeakToChat, ShowWindow };
    static constexpr int kActionCount = 4;
    // WM_HOTKEY id of an action: its index plus this. Ids are per thread and
    // must stay below 0xC000; 0 is left free.
    static constexpr int kNativeIdBase = 1;

    // Status of one binding as the OS sees it.
    enum class Status {
        Off,          // disabled or no shortcut
        Registered,
        Conflict,     // the OS refused it: another application (or another
                      // action here) already holds the combination
        Unsupported   // no backend on this platform
    };

    // settingsGroup: where the bindings are stored inside the app's QSettings;
    // tests use a scratch group so they never touch real bindings.
    HotkeyManager(DeviceCenterController& controller, TrayController& tray, QObject* parent = nullptr,
                  const QString& settingsGroup = QStringLiteral("hotkeys"));
    ~HotkeyManager() override;

    [[nodiscard]] static bool supported();
    // Stable identifier used in QSettings and QML ("toggleNoiseControl", ...).
    [[nodiscard]] static QString actionKey(Action action);
    [[nodiscard]] static std::optional<Action> actionFromKey(const QString& key);

    // Canonical portable form ("Ctrl+Alt+N") of a shortcut, or empty when the
    // text is not something a global hotkey can be: unparsable, more than one
    // chord, or a bare key without a modifier (function keys are allowed
    // alone, everything else would swallow ordinary typing).
    [[nodiscard]] static QString normalize(const QString& text);
    // The shortcut a key event describes, for the capture field. Empty while
    // only modifiers are held.
    Q_INVOKABLE static QString sequenceFromKey(int key, int modifiers);
    // Portable form rendered for the current platform ("Ctrl+Alt+N").
    Q_INVOKABLE static QString displayText(const QString& portable);

    [[nodiscard]] bool isEnabled(Action action) const { return _bindings[index(action)].enabled; }
    [[nodiscard]] QString shortcut(Action action) const { return _bindings[index(action)].shortcut; }
    [[nodiscard]] Status status(Action action) const { return _bindings[index(action)].status; }

    Q_INVOKABLE void setEnabled(const QString& action, bool enabled);
    // An unusable shortcut (see normalize) clears the binding.
    Q_INVOKABLE void setShortcut(const QString& action, const QString& shortcut);
    // One map per action: action, enabled, shortcut, display, status
    // ("off" | "registered" | "conflict" | "unsupported").
    [[nodiscard]] QVariantList bindings() const;
    // While the Settings capture field has focus the registrations are
    // released, otherwise a press of an already-bound combination would run
    // the action instead of reaching the field.
    Q_INVOKABLE void suspend(bool suspended);

    // Runs an action exactly as a key press would; the controller does the
    // rest. Public so tests and the tray can share it.
    void trigger(Action action);
    // Text of the last feedback toast, "" when none was shown. For tests.
    [[nodiscard]] QString lastFeedback() const { return _lastFeedback; }

    bool nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) override;

signals:
    void bindingsChanged();
    void showWindowRequested();

private:
    struct Binding {
        bool enabled{false};
        QString shortcut;
        Status status{Status::Off};
    };

    static constexpr int index(Action action) { return static_cast<int>(action); }
    void _load();
    void _save(Action action);
    void _apply();
    bool _registerNative(int id, const QString& shortcut);
    void _unregisterNative(int id);
    void _feedback(const QString& body);

    DeviceCenterController& _controller;
    TrayController& _tray;
    QString _settingsGroup;
    std::array<Binding, kActionCount> _bindings;
    std::array<bool, kActionCount> _registered{};
    bool _suspended{false};
    QString _lastFeedback;
};

} // namespace sony::devicecenter
