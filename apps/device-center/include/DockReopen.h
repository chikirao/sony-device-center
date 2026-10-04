#pragma once

#include <functional>

namespace sony::devicecenter {

// Clicking the Dock icon asks a macOS app to reopen. Qt reports that only as
// the application becoming active, which launching and Cmd-Tab also do, so a
// window closed to the tray could not tell a Dock click apart and never came
// back. This hears the reopen request itself.
//
// No-ops on other platforms.
namespace DockReopen {

#ifdef __APPLE__
// Call before QApplication is created: Qt keeps an application delegate that
// already exists and forwards the reopen request to it.
void install();

// Runs on the main thread for each Dock click; replaces any earlier handler.
void setHandler(std::function<void()> handler);
#else
inline void install() {}
inline void setHandler(std::function<void()>) {}
#endif

} // namespace DockReopen

} // namespace sony::devicecenter
