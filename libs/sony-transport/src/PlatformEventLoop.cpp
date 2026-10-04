#include "sony/transport/PlatformTransport.h"

#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <pthread.h>
#include <atomic>
#include <exception>
#include <thread>
#endif

namespace sony::transport {

#ifdef __APPLE__

int runWithPlatformEventLoop(const std::function<int()>& body) {
    // Only the main thread's run loop receives IOBluetooth callbacks.
    if (!pthread_main_np()) return body();

    CFRunLoopRef mainLoop = CFRunLoopGetCurrent();
    // A version-0 source keeps the run loop alive while the worker runs and lets
    // the worker stop it. A signal sent before the loop starts is still handled.
    CFRunLoopSourceContext context{};
    context.info = mainLoop;
    context.perform = [](void* info) { CFRunLoopStop(static_cast<CFRunLoopRef>(info)); };
    CFRunLoopSourceRef stopSource = CFRunLoopSourceCreate(kCFAllocatorDefault, 0, &context);
    CFRunLoopAddSource(mainLoop, stopSource, kCFRunLoopDefaultMode);

    std::atomic<bool> done{false};
    int result = 1;
    std::exception_ptr error;
    std::thread worker([&] {
        try {
            result = body();
        } catch (...) {
            error = std::current_exception();
        }
        done = true;
        CFRunLoopSourceSignal(stopSource);
        CFRunLoopWakeUp(mainLoop);
    });
    while (!done) {
        CFRunLoopRun();
    }
    worker.join();

    CFRunLoopRemoveSource(mainLoop, stopSource, kCFRunLoopDefaultMode);
    CFRelease(stopSource);
    if (error) std::rethrow_exception(error);
    return result;
}

#else

int runWithPlatformEventLoop(const std::function<int()>& body) {
    return body();
}

#endif

} // namespace sony::transport
