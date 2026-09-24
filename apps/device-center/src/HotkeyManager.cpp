#include "HotkeyManager.h"
#include "DeviceCenterController.h"
#include "TrayController.h"

#include <QCoreApplication>
#include <QKeySequence>
#include <QSettings>
#include <QVariantMap>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace sony::devicecenter {

namespace {
constexpr const char* kActionKeys[HotkeyManager::kActionCount] = {
    "toggleNoiseControl", "noiseControlOff", "toggleSpeakToChat", "showWindow"};

bool isModifierKey(int key) {
    return key == Qt::Key_Control || key == Qt::Key_Shift || key == Qt::Key_Alt || key == Qt::Key_Meta
        || key == Qt::Key_AltGr || key == Qt::Key_unknown || key == 0;
}

bool isFunctionKey(int key) { return key >= Qt::Key_F1 && key <= Qt::Key_F35; }

#ifdef Q_OS_WIN
// Qt::Key -> virtual-key code. Letters, digits and function keys line up
// with a constant offset; the rest is a short table plus the layout lookup
// for punctuation.
UINT virtualKeyFor(int key) {
    if (key >= Qt::Key_A && key <= Qt::Key_Z) return static_cast<UINT>('A' + (key - Qt::Key_A));
    if (key >= Qt::Key_0 && key <= Qt::Key_9) return static_cast<UINT>('0' + (key - Qt::Key_0));
    if (key >= Qt::Key_F1 && key <= Qt::Key_F24) return static_cast<UINT>(VK_F1 + (key - Qt::Key_F1));
    switch (key) {
    case Qt::Key_Space: return VK_SPACE;
    case Qt::Key_Escape: return VK_ESCAPE;
    case Qt::Key_Tab: case Qt::Key_Backtab: return VK_TAB;
    case Qt::Key_Backspace: return VK_BACK;
    case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
    case Qt::Key_Insert: return VK_INSERT;
    case Qt::Key_Delete: return VK_DELETE;
    case Qt::Key_Home: return VK_HOME;
    case Qt::Key_End: return VK_END;
    case Qt::Key_PageUp: return VK_PRIOR;
    case Qt::Key_PageDown: return VK_NEXT;
    case Qt::Key_Left: return VK_LEFT;
    case Qt::Key_Up: return VK_UP;
    case Qt::Key_Right: return VK_RIGHT;
    case Qt::Key_Down: return VK_DOWN;
    case Qt::Key_Pause: return VK_PAUSE;
    case Qt::Key_Print: return VK_SNAPSHOT;
    case Qt::Key_ScrollLock: return VK_SCROLL;
    case Qt::Key_MediaPlay: case Qt::Key_MediaTogglePlayPause: return VK_MEDIA_PLAY_PAUSE;
    case Qt::Key_MediaStop: return VK_MEDIA_STOP;
    case Qt::Key_MediaNext: return VK_MEDIA_NEXT_TRACK;
    case Qt::Key_MediaPrevious: return VK_MEDIA_PREV_TRACK;
    case Qt::Key_VolumeUp: return VK_VOLUME_UP;
    case Qt::Key_VolumeDown: return VK_VOLUME_DOWN;
    case Qt::Key_VolumeMute: return VK_VOLUME_MUTE;
    default: break;
    }
    // Printable Latin-1 punctuation: ask the current layout which key makes it.
    if (key > 0x20 && key < 0x100) {
        const SHORT scan = VkKeyScanW(static_cast<WCHAR>(key));
        if (scan != -1) return static_cast<UINT>(scan & 0xff);
    }
    return 0;
}

UINT modifiersFor(Qt::KeyboardModifiers mods) {
    UINT out = 0;
    if (mods & Qt::ControlModifier) out |= MOD_CONTROL;
    if (mods & Qt::AltModifier) out |= MOD_ALT;
    if (mods & Qt::ShiftModifier) out |= MOD_SHIFT;
    if (mods & Qt::MetaModifier) out |= MOD_WIN;
    return out;
}
#endif
}

HotkeyManager::HotkeyManager(DeviceCenterController& controller, TrayController& tray, QObject* parent,
                             const QString& settingsGroup)
    : QObject(parent), _controller(controller), _tray(tray), _settingsGroup(settingsGroup) {
    _load();
    if (auto* app = QCoreApplication::instance()) app->installNativeEventFilter(this);
    _apply();
}

HotkeyManager::~HotkeyManager() {
    for (int i = 0; i < kActionCount; ++i) _unregisterNative(i);
    if (auto* app = QCoreApplication::instance()) app->removeNativeEventFilter(this);
}

bool HotkeyManager::supported() {
#ifdef Q_OS_WIN
    return true;
#else
    return false;
#endif
}

QString HotkeyManager::actionKey(Action action) { return QString::fromLatin1(kActionKeys[index(action)]); }

std::optional<HotkeyManager::Action> HotkeyManager::actionFromKey(const QString& key) {
    for (int i = 0; i < kActionCount; ++i)
        if (key == QLatin1String(kActionKeys[i])) return static_cast<Action>(i);
    return std::nullopt;
}

QString HotkeyManager::normalize(const QString& text) {
    const auto trimmed = text.trimmed();
    if (trimmed.isEmpty()) return {};
    const QKeySequence sequence = QKeySequence::fromString(trimmed, QKeySequence::PortableText);
    if (sequence.count() != 1) return {};
    const QKeyCombination combination = sequence[0];
    const int key = combination.key();
    if (isModifierKey(key)) return {};
    const auto mods = combination.keyboardModifiers()
        & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    if (mods == Qt::NoModifier && !isFunctionKey(key)) return {};
    return QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(key))).toString(QKeySequence::PortableText);
}

QString HotkeyManager::sequenceFromKey(int key, int modifiers) {
    if (isModifierKey(key)) return {};
    const auto mods = Qt::KeyboardModifiers(modifiers)
        & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);
    return normalize(QKeySequence(QKeyCombination(mods, static_cast<Qt::Key>(key))).toString(QKeySequence::PortableText));
}

QString HotkeyManager::displayText(const QString& portable) {
    if (portable.isEmpty()) return {};
    return QKeySequence::fromString(portable, QKeySequence::PortableText).toString(QKeySequence::NativeText);
}

void HotkeyManager::_load() {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.beginGroup(_settingsGroup);
    for (int i = 0; i < kActionCount; ++i) {
        const auto key = QLatin1String(kActionKeys[i]);
        _bindings[i].enabled = settings.value(key + "/enabled", false).toBool();
        _bindings[i].shortcut = normalize(settings.value(key + "/shortcut").toString());
    }
}

void HotkeyManager::_save(Action action) {
    QSettings settings("SonyBridge", "SonyDeviceCenter");
    settings.beginGroup(_settingsGroup);
    const auto& binding = _bindings[index(action)];
    settings.setValue(actionKey(action) + "/enabled", binding.enabled);
    settings.setValue(actionKey(action) + "/shortcut", binding.shortcut);
}

void HotkeyManager::setEnabled(const QString& action, bool enabled) {
    const auto parsed = actionFromKey(action);
    if (!parsed) return;
    auto& binding = _bindings[index(*parsed)];
    if (binding.enabled == enabled) return;
    binding.enabled = enabled;
    _save(*parsed);
    _apply();
}

void HotkeyManager::setShortcut(const QString& action, const QString& shortcut) {
    const auto parsed = actionFromKey(action);
    if (!parsed) return;
    auto& binding = _bindings[index(*parsed)];
    const auto normalized = normalize(shortcut);
    if (binding.shortcut == normalized) return;
    binding.shortcut = normalized;
    _save(*parsed);
    _apply();
}

void HotkeyManager::suspend(bool suspended) {
    if (_suspended == suspended) return;
    _suspended = suspended;
    _apply();
}

// (Re)registers every binding from scratch. Registration order is the action
// order, so when two actions share a combination the later one reports the
// conflict.
void HotkeyManager::_apply() {
    for (int i = 0; i < kActionCount; ++i) _unregisterNative(i);
    for (int i = 0; i < kActionCount; ++i) {
        auto& binding = _bindings[i];
        if (!binding.enabled || binding.shortcut.isEmpty()) binding.status = Status::Off;
        else if (!supported()) binding.status = Status::Unsupported;
        else if (_suspended) binding.status = Status::Registered;
        else binding.status = _registerNative(i, binding.shortcut) ? Status::Registered : Status::Conflict;
    }
    emit bindingsChanged();
}

bool HotkeyManager::_registerNative(int id, const QString& shortcut) {
#ifdef Q_OS_WIN
    const QKeySequence sequence = QKeySequence::fromString(shortcut, QKeySequence::PortableText);
    if (sequence.count() != 1) return false;
    const QKeyCombination combination = sequence[0];
    const UINT vk = virtualKeyFor(combination.key());
    if (vk == 0) return false;
    // MOD_NOREPEAT: holding the keys fires once, not on every auto-repeat.
    if (!RegisterHotKey(nullptr, HotkeyManager::kNativeIdBase + id, modifiersFor(combination.keyboardModifiers()) | MOD_NOREPEAT, vk))
        return false;
    _registered[id] = true;
    return true;
#else
    Q_UNUSED(id); Q_UNUSED(shortcut);
    return false;
#endif
}

void HotkeyManager::_unregisterNative(int id) {
    if (!_registered[id]) return;
#ifdef Q_OS_WIN
    UnregisterHotKey(nullptr, HotkeyManager::kNativeIdBase + id);
#endif
    _registered[id] = false;
}

bool HotkeyManager::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result) {
    Q_UNUSED(eventType); Q_UNUSED(result);
#ifdef Q_OS_WIN
    // Hotkeys registered without a window arrive as thread messages.
    auto* msg = static_cast<MSG*>(message);
    if (msg->message != WM_HOTKEY) return false;
    const int id = static_cast<int>(msg->wParam) - HotkeyManager::kNativeIdBase;
    if (id < 0 || id >= kActionCount || !_registered[id]) return false;
    trigger(static_cast<Action>(id));
    return true;
#else
    Q_UNUSED(message);
    return false;
#endif
}

QVariantList HotkeyManager::bindings() const {
    static const char* const statusNames[] = {"off", "registered", "conflict", "unsupported"};
    QVariantList out;
    for (int i = 0; i < kActionCount; ++i) {
        const auto& binding = _bindings[i];
        out.append(QVariantMap{
            {"action", QString::fromLatin1(kActionKeys[i])},
            {"enabled", binding.enabled},
            {"shortcut", binding.shortcut},
            {"display", displayText(binding.shortcut)},
            {"status", QString::fromLatin1(statusNames[static_cast<int>(binding.status)])},
        });
    }
    return out;
}

void HotkeyManager::trigger(Action action) {
    auto t = [this](const char* key) { return _controller.t(QString::fromLatin1(key)); };
    if (action == Action::ShowWindow) {
        _tray.showWindow();
        emit showWindowRequested();
        return;
    }
    if (!_controller.isConnected()) {
        _feedback(t("disconnected"));
        return;
    }
    switch (action) {
    case Action::ToggleNoiseControl:
        // Cancelling and Off both go to Ambient; only Ambient goes back to
        // Cancelling. A model without an ambient mode just gets Cancelling.
        if (_controller.noiseControlMode() != "ambient" && _controller.hasAmbient()) {
            _controller.setAmbient(_controller.ambientLevel(), _controller.focusOnVoice());
            _feedback(t("ambient_sound"));
        } else {
            _controller.setAnc(true);
            _feedback(t("noise_cancelling"));
        }
        break;
    case Action::NoiseControlOff:
        _controller.setNoiseControlOff();
        _feedback(t("noise_control_off"));
        break;
    case Action::ToggleSpeakToChat: {
        const bool enable = !_controller.speakToChat();
        _controller.setSpeakToChat(enable);
        _feedback(QStringLiteral("Speak-to-Chat: ") + t(enable ? "hotkey_state_on" : "hotkey_state_off"));
        break;
    }
    case Action::ShowWindow:
        break;
    }
}

void HotkeyManager::_feedback(const QString& body) {
    if (!_controller.notifyHotkeys()) return;
    const auto name = _controller.displayName();
    _lastFeedback = body;
    _tray.showMessage(name.isEmpty() ? QStringLiteral("Sony Device Center") : name, body);
}

} // namespace sony::devicecenter
