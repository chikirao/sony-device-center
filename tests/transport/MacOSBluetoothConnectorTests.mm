#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "macos/MacOSBluetoothConnector.h"
#import <objc/runtime.h>
#include <chrono>
#include <ctime>
#include <thread>

// Substitute only the OS boundary: exercise the real connector without Bluetooth
// hardware or privacy prompts. This file is built without ARC, like the connector.

struct MacOSBluetoothConnectorTestAccess {
    static void attach(MacOSBluetoothConnector& connector, void* channel) {
        connector.retainChannel(channel);
        connector.running = true;
    }
    static void finishedWorker(MacOSBluetoothConnector& connector) {
        connector.uthread = std::thread([] {});
    }
    static void setOpenTimeout(MacOSBluetoothConnector& connector, std::chrono::milliseconds timeout) {
        connector.openTimeout = timeout;
    }
};

@interface TestRFCOMMChannel : NSObject {
@public
    int* closes;
    int* destructions;
}
@property BOOL open;
@property IOReturn writeResult;
@property int transientWrites;
@property int writeCalls;
@property BOOL stallWrites;
@end
@implementation TestRFCOMMChannel
- (BOOL)isOpen { return self.open; }
- (void)setDelegate:(id)delegate {}
- (IOReturn)closeChannel { self.open = NO; if (closes) ++*closes; return kIOReturnSuccess; }
- (IOReturn)writeSync:(void*)data length:(UInt16)length {
    ++self.writeCalls;
    // A stalled write only ends when the channel is closed underneath it.
    while (self.stallWrites && self.open) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    if (!self.open) return kIOReturnNotOpen;
    if (self.transientWrites > 0) {
        --self.transientWrites;
        return kIOReturnNoResources;
    }
    return self.writeResult;
}
- (void)dealloc { if (destructions) ++*destructions; [super dealloc]; }
@end

// How the fake reports the outcome of openRFCOMMChannelAsync.
enum class OpenDelivery {
    Immediately,     // synchronously, inside the open call
    WorkerRunLoop,   // later, through the connecting thread's run loop
    OtherThread,     // later, from another thread, as the main run loop does
    Never,           // the open never completes
    CloseInstead,    // the channel closes while opening
};

@interface TestSonyDevice : NSObject
@property BOOL failOpen;
@property BOOL linkDown;            // baseband not connected, as for a headset that is off
@property BOOL pageFails;           // the headset does not answer the page
@property BOOL pageHangs;           // no answer before the attempt is cancelled
@property BOOL pageFindsLinkUp;     // macOS reconnected meanwhile: "connection exists"
// Not retained, like IOBluetooth's reference to a page target (undocumented).
@property(assign) id pageTarget;
@property int openConnectionCalls;  // synchronous baseband opens
@property int linkOpenRequests;     // asynchronous baseband opens
@property OpenDelivery delivery;
@property(retain) TestRFCOMMChannel *channel;
@property(retain) id lastDelegate;
@end
@implementation TestSonyDevice
- (BOOL)isConnected { return !self.linkDown; }
// The synchronous baseband open blocks for the whole page timeout; connect must not use it.
- (IOReturn)openConnection {
    ++self.openConnectionCalls;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    return kIOReturnSuccess;
}
- (IOReturn)openConnection:(id)target {
    ++self.linkOpenRequests;
    self.pageTarget = target;
    if (self.pageHangs) return kIOReturnSuccess;
    if (!self.pageFails) self.linkDown = NO;
    const IOReturn status = self.pageFails ? kIOReturnTimeout
                          : self.pageFindsLinkUp ? kIOReturnExclusiveAccess : kIOReturnSuccess;
    [target connectionComplete:(IOBluetoothDevice *)self status:status];
    return kIOReturnSuccess;
}
- (id)getServiceRecordForUUID:(id)uuid { return self; }
- (IOReturn)getRFCOMMChannelID:(BluetoothRFCOMMChannelID *)channelID {
    *channelID = 9;
    return kIOReturnSuccess;
}
- (void)completeOpen:(id)delegate {
    self.channel.open = !self.failOpen;
    [delegate rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)self.channel
                                 status:self.failOpen ? kIOReturnError : kIOReturnSuccess];
}
- (IOReturn)openRFCOMMChannelAsync:(IOBluetoothRFCOMMChannel **)channel
                   withChannelID:(BluetoothRFCOMMChannelID)channelID delegate:(id)delegate {
    self.channel = [[TestRFCOMMChannel new] autorelease];
    *channel = (IOBluetoothRFCOMMChannel *)self.channel;
    self.lastDelegate = delegate;
    switch (self.delivery) {
        case OpenDelivery::Immediately:
            [self completeOpen:delegate];
            break;
        case OpenDelivery::WorkerRunLoop:
            [self performSelector:@selector(completeOpen:) withObject:delegate afterDelay:0.1];
            break;
        case OpenDelivery::OtherThread:
            // The detached thread holds its own references until it is done.
            [self retain];
            [delegate retain];
            std::thread([self, delegate] {
                std::this_thread::sleep_for(std::chrono::milliseconds(300));
                [self completeOpen:delegate];
                [delegate release];
                [self release];
            }).detach();
            break;
        case OpenDelivery::Never:
            break;
        case OpenDelivery::CloseInstead:
            [delegate rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)self.channel];
            break;
    }
    return kIOReturnSuccess; // Accepted, but establishment is reported separately.
}
@end

static TestSonyDevice *testDevice;
static id deviceForTest(id, SEL, NSString *) { return testDevice; }

struct FakeBluetoothDevice {
    Method method = class_getClassMethod([IOBluetoothDevice class], @selector(deviceWithAddressString:));
    IMP original = method_setImplementation(method, (IMP)deviceForTest);
    FakeBluetoothDevice() { testDevice = [TestSonyDevice new]; }
    ~FakeBluetoothDevice() {
        method_setImplementation(method, original);
        [testDevice release];
        testDevice = nil;
    }
};

TEST_CASE("macOS connect waits for channel establishment and propagates failure", "[transport][macos]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        SECTION("success is returned only for an open channel") {
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        SECTION("completion delivered later on the connecting thread's run loop") {
            testDevice.delivery = OpenDelivery::WorkerRunLoop;
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        SECTION("establishment failure reaches the caller and permits retry") {
            testDevice.failOpen = YES;
            CHECK_THROWS_WITH(connector.connect("80:99:E7:00:00:01"),
                              Catch::Matchers::ContainsSubstring("refused"));
            CHECK_FALSE(connector.isConnected());
            testDevice.failOpen = NO;
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        SECTION("a close while opening is reported as such") {
            testDevice.delivery = OpenDelivery::CloseInstead;
            CHECK_THROWS_WITH(connector.connect("80:99:E7:00:00:01"),
                              Catch::Matchers::ContainsSubstring("closed while opening"));
            CHECK_FALSE(connector.isConnected());
        }
        SECTION("an open that never completes times out") {
            testDevice.delivery = OpenDelivery::Never;
            MacOSBluetoothConnectorTestAccess::setOpenTimeout(connector, std::chrono::milliseconds(200));
            CHECK_THROWS_WITH(connector.connect("80:99:E7:00:00:01"),
                              Catch::Matchers::ContainsSubstring("Timed out"));
            CHECK_FALSE(connector.isConnected());
        }
        connector.disconnect();
    }
}

TEST_CASE("macOS connect brings a down link up without blocking and can be cancelled", "[transport][macos]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        testDevice.linkDown = YES;
        MacOSBluetoothConnector connector;
        // As when the app quits while paging a headset that was switched off.
        auto expectPromptCancel = [&] {
            std::thread canceller([&] {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                connector.abort();
            });
            const auto start = std::chrono::steady_clock::now();
            CHECK_THROWS_WITH(connector.connect("80:99:E7:00:00:01"),
                              Catch::Matchers::ContainsSubstring("cancelled"));
            CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
            canceller.join();
            CHECK_FALSE(connector.isConnected());
        };
        SECTION("the link is opened asynchronously, not with the blocking call") {
            const auto start = std::chrono::steady_clock::now();
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
            CHECK(testDevice.linkOpenRequests == 1);
            CHECK(testDevice.openConnectionCalls == 0);
            CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(1));
        }
        SECTION("a headset that does not answer the page is reported as such") {
            testDevice.pageFails = YES;
            CHECK_THROWS_WITH(connector.connect("80:99:E7:00:00:01"),
                              Catch::Matchers::ContainsSubstring("did not answer"));
            CHECK(testDevice.channel == nil); // no RFCOMM open without a link
        }
        SECTION("a link that came up meanwhile is used despite an error status") {
            testDevice.pageFindsLinkUp = YES;
            connector.connect("80:99:E7:00:00:01");
            CHECK(connector.isConnected());
        }
        SECTION("abort ends a pending page, and its late completion is harmless") {
            testDevice.pageHangs = YES;
            expectPromptCancel();
            connector.disconnect();
            // Let the main queue release the connector's reference to the delegate.
            for (int i = 0; i < 5; ++i) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, false);
            // The page ends later; its target must still exist and must not reach
            // the connector.
            [testDevice.pageTarget connectionComplete:(IOBluetoothDevice *)testDevice status:kIOReturnTimeout];
            CHECK_FALSE(connector.isConnected());
        }
        SECTION("abort ends a pending RFCOMM open") {
            testDevice.linkDown = NO;
            testDevice.delivery = OpenDelivery::Never;
            expectPromptCancel();
            CHECK(testDevice.linkOpenRequests == 0); // the link was already up
        }
        connector.disconnect();
    }
}

TEST_CASE("macOS connect blocks without spinning until another thread completes the open", "[transport][macos]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        testDevice.delivery = OpenDelivery::OtherThread;
        MacOSBluetoothConnector connector;
        const std::clock_t cpuBefore = std::clock();
        std::thread opener([&] { connector.connect("80:99:E7:00:00:01"); });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Accepted but not yet established: not usable.
        CHECK_FALSE(connector.isConnected());
        opener.join();
        const double cpuSeconds = double(std::clock() - cpuBefore) / CLOCKS_PER_SEC;
        CHECK(connector.isConnected());
        // The worker's run loop has no sources here; a polling loop would burn
        // roughly the whole 300 ms wait.
        CHECK(cpuSeconds < 0.1);
        connector.disconnect();
    }
}

TEST_CASE("macOS writes retry transient failures and report persistent ones", "[transport][macos]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        connector.connect("80:99:E7:00:00:01");
        char byte = 0x3e;
        SECTION("a transient failure is retried") {
            testDevice.channel.transientWrites = 1;
            CHECK(connector.send(&byte, 1) == 1);
            CHECK(testDevice.channel.writeCalls == 2);
        }
        SECTION("a persistent transient failure is reported after bounded retries") {
            testDevice.channel.transientWrites = 100;
            CHECK_THROWS_AS(connector.send(&byte, 1), RecoverableException);
            CHECK(testDevice.channel.writeCalls == 3);
        }
        SECTION("a hard failure is reported instead of being dropped") {
            testDevice.channel.writeResult = kIOReturnNotOpen;
            CHECK_THROWS_AS(connector.send(&byte, 1), RecoverableException);
            CHECK(testDevice.channel.writeCalls == 1);
        }
        connector.disconnect();
    }
}

TEST_CASE("macOS disconnect aborts a stalled write", "[transport][macos][lifecycle]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        connector.connect("80:99:E7:00:00:01");
        testDevice.channel.stallWrites = YES;
        std::thread writer([&] {
            char byte = 0x3e;
            CHECK_THROWS_AS(connector.send(&byte, 1), RecoverableException);
        });
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        const auto start = std::chrono::steady_clock::now();
        connector.disconnect();
        writer.join();
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2000));
    }
}

TEST_CASE("macOS callbacks after teardown never reach the connector", "[transport][macos][lifecycle]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        connector.connect("80:99:E7:00:00:01");
        id delegate = [[testDevice.lastDelegate retain] autorelease];
        connector.disconnect();
        // A callback IOBluetooth had already dispatched before the delegate was cleared.
        unsigned char bytes[3] = {1, 2, 3};
        [delegate rfcommChannelData:(IOBluetoothRFCOMMChannel *)testDevice.channel data:bytes length:sizeof bytes];
        [delegate rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)testDevice.channel status:kIOReturnSuccess];
        CHECK(connector.receivedBytes.empty());
        CHECK_FALSE(connector.isConnected());
    }
}

TEST_CASE("macOS abort ends an open connection and wakes a blocked receiver", "[transport][macos][lifecycle]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        connector.connect("80:99:E7:00:00:01");
        std::thread aborter([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            connector.abort();
        });
        const auto start = std::chrono::steady_clock::now();
        char buf[8];
        CHECK_THROWS_AS(connector.recv(buf, sizeof buf), RecoverableException);
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(1000));
        CHECK_FALSE(connector.isConnected());
        aborter.join();
        connector.disconnect();
    }
}

TEST_CASE("macOS remote close wakes a blocked receiver", "[transport][macos][lifecycle]") {
    @autoreleasepool {
        FakeBluetoothDevice fake;
        MacOSBluetoothConnector connector;
        connector.connect("80:99:E7:00:00:01");
        std::thread closer([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            connector.channelClosed();
        });
        const auto start = std::chrono::steady_clock::now();
        char buf[8];
        CHECK_THROWS_AS(connector.recv(buf, sizeof buf), RecoverableException);
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(2000));
        CHECK_FALSE(connector.isConnected());
        closer.join();
        connector.disconnect();
    }
}

TEST_CASE("macOS channel survives caller release and disconnect is repeatable", "[transport][macos][lifecycle]") {
    int closes = 0, destructions = 0;
    {
        MacOSBluetoothConnector connector;
        TestRFCOMMChannel* channel = [[TestRFCOMMChannel alloc] init];
        channel->closes = &closes;
        channel->destructions = &destructions;
        channel.open = YES;
        MacOSBluetoothConnectorTestAccess::attach(connector, (__bridge void*)channel);
        [channel release];
        CHECK(destructions == 0);
        CHECK(connector.isConnected());
        connector.receivedBytes.push_back({1, 2, 3});
        connector.disconnect();
        CHECK(closes == 1);
        CHECK(destructions == 1);
        CHECK_FALSE(connector.isConnected());
        CHECK(connector.receivedBytes.empty());
        connector.disconnect();
        CHECK(closes == 1);
        CHECK(destructions == 1);
    }
    CHECK(destructions == 1);
}

TEST_CASE("macOS destructor joins a failed or remotely closed worker", "[transport][macos][lifecycle]") {
    MacOSBluetoothConnector connector;
    MacOSBluetoothConnectorTestAccess::finishedWorker(connector);
    CHECK_FALSE(connector.isConnected());
}
