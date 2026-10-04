#include "DockReopen.h"

#import <AppKit/AppKit.h>
#import <CoreServices/CoreServices.h>
#include <libproc.h>

namespace {
std::function<void()>& reopenHandler() {
    static std::function<void()> handler;
    return handler;
}
}

@interface SonyDockReopenDelegate : NSObject <NSApplicationDelegate>
@end

@implementation SonyDockReopenDelegate
- (BOOL)applicationShouldHandleReopen:(NSApplication *)sender hasVisibleWindows:(BOOL)hasVisibleWindows {
    // The Dock, Finder, Spotlight or a notification click can ask for this. One
    // reopen was seen that nobody remembered asking for; logging the sender makes a
    // repeat traceable: log show --last 1h --predicate 'process == "sony-device-center"'
    NSAppleEventDescriptor* request = [[NSAppleEventManager sharedAppleEventManager] currentAppleEvent];
    const pid_t senderPid = [[request attributeDescriptorForKeyword:keySenderPIDAttr] int32Value];
    NSString* senderId = [NSRunningApplication runningApplicationWithProcessIdentifier:senderPid].bundleIdentifier;
    // A short-lived sender such as the `open` command has exited by now.
    char senderName[2 * MAXCOMLEN] = "a process that has exited";
    if (!senderId) proc_name(senderPid, senderName, sizeof senderName);
    NSLog(@"Reopen request from %@ (pid %d); windows visible: %d", senderId ?: @(senderName), senderPid, (int)hasVisibleWindows);
    if (!reopenHandler()) return YES;
    reopenHandler()();
    // Handled: AppKit's default would only deminiaturize, which showWindow does too.
    return NO;
}
@end

namespace sony::devicecenter::DockReopen {

void install() {
    // This creates NSApp before Qt, so Qt cannot make it its QNSApplication subclass.
    // Qt supports that case: QNSApplication only replaces -sendEvent:, and Qt then
    // redirects NSApplication's -sendEvent: to the same replacement instead.
    // NSApplication holds its delegate weakly; this one lives as long as the app.
    static SonyDockReopenDelegate* delegate = [[SonyDockReopenDelegate alloc] init];
    [NSApplication sharedApplication].delegate = delegate;
}

void setHandler(std::function<void()> handler) {
    reopenHandler() = std::move(handler);
}

} // namespace sony::devicecenter::DockReopen
