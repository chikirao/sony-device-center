#pragma once

class QWindow;

namespace sony::devicecenter {

// On macOS a Qt.Tool window becomes a panel that AppKit hides whenever the app
// is not frontmost, and requestActivate() only makes it key, so a hub opened
// from the menu bar while another app was in front stayed invisible.
//
// No-ops on other platforms.
namespace MacPanel {

#ifdef __APPLE__
// Makes the tool window behave like a menu-bar popover: it stays up and takes
// keyboard focus while another app is in front, and clicks in it leave this app
// inactive, so the main window stays where it is. Call before it is shown.
void makeNonActivating(QWindow* window);

// Whether this macOS still has the private panel setter makeNonActivating()
// relies on. Without it the hub still works, but a click in it activates the app.
// A test checks it, so a macOS that drops it is noticed in CI first.
bool preventsActivationAvailable();
#else
inline void makeNonActivating(QWindow*) {}
inline bool preventsActivationAvailable() { return false; }
#endif

} // namespace MacPanel

} // namespace sony::devicecenter
