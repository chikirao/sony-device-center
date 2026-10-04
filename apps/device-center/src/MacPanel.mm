#include "MacPanel.h"

#include <QDebug>
#include <QGuiApplication>
#include <QVariant>
#include <QWindow>

#import <AppKit/AppKit.h>
#import <objc/message.h>

namespace sony::devicecenter::MacPanel {

namespace {
SEL preventsActivationSelector() {
    return NSSelectorFromString(@"_setPreventsActivation:");
}
}

bool preventsActivationAvailable() {
    return [NSPanel instancesRespondToSelector:preventsActivationSelector()];
}

void makeNonActivating(QWindow* window) {
    // Only the Cocoa platform's window ids are NSViews (tests run offscreen).
    if (!window || QGuiApplication::platformName() != QLatin1String("cocoa")) return;
    // Read by Qt when it creates the panel, and again whenever it recreates it.
    window->setProperty("_q_macAlwaysShowToolWindow", true);
    auto* view = reinterpret_cast<NSView*>(window->winId());
    NSWindow* panel = view.window;
    if (![panel isKindOfClass:[NSPanel class]]) return;
    panel.hidesOnDeactivate = NO;
    // Qt keeps this style bit when it recomputes the style mask.
    panel.styleMask |= NSWindowStyleMaskNonactivatingPanel;
    // AppKit applies that style fully only to a panel created with it, and Qt
    // creates the panel itself: a click in the hub still activated the app (which
    // macOS took back a second later, closing the hub). The panel's own setter for
    // the same state does apply it. It is private, so it is used only if present;
    // without it the hub works as before.
    if (![panel respondsToSelector:preventsActivationSelector()]) {
        qWarning("NSPanel has no -_setPreventsActivation: on this macOS; clicks in the hub will activate the app");
        return;
    }
    ((void (*)(id, SEL, BOOL))objc_msgSend)(panel, preventsActivationSelector(), YES);
}

} // namespace sony::devicecenter::MacPanel
