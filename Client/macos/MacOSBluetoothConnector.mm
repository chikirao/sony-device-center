#include "MacOSBluetoothConnector.h"
#include <cassert>

namespace {
// writeSync results a short retry can clear: the controller is momentarily out of
// buffers or busy. Anything else means the channel is unusable.
bool isTransientWriteError(IOReturn result)
{
    return result == kIOReturnBusy || result == kIOReturnNoResources;
}

// Waits on the connector's worker thread until done() (read under the mutex) or the
// deadline. IOBluetooth normally delivers its callbacks on the main run loop and they
// signal the condition variable. If it attached sources to this thread's run loop
// instead, run it; an empty run loop returns at once, so block rather than spin.
template <class Done>
void waitOnWorker(std::mutex& mutex, std::condition_variable& condition,
                  std::chrono::steady_clock::time_point deadline, Done done)
{
    while (true) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (done()) return;
        }
        const auto remaining = deadline - std::chrono::steady_clock::now();
        if (remaining <= std::chrono::steady_clock::duration::zero()) return;
        @autoreleasepool {
            const double slice = std::min(0.25, std::chrono::duration<double>(remaining).count());
            if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, slice, true) == kCFRunLoopRunFinished) {
                std::unique_lock<std::mutex> lock(mutex);
                condition.wait_until(lock, deadline, done);
            }
        }
    }
}
}

MacOSBluetoothConnector::MacOSBluetoothConnector()
{
    
}
MacOSBluetoothConnector::~MacOSBluetoothConnector()
{
    // Always release the worker thread: a failed connect or a remote close leaves it
    // joinable, and destroying a joinable std::thread calls std::terminate().
    disconnect();
}

@interface AsyncCommDelegate : NSObject <IOBluetoothRFCOMMChannelDelegate> {
@public
    // Callbacks run on another thread (normally the main run loop) and may still be
    // executing while the owner tears down. detachOwner waits for them and then cuts
    // the link, so no callback can reach a disconnected or destroyed connector.
    std::mutex ownerMutex;
    MacOSBluetoothConnector* owner;
    // Set while an openConnection: page this object is the target of is running.
    BOOL pageInFlight;
}
- (void)detachOwner;
// A page holds its own reference to its target: it can outlive the attempt (and the
// connector's reference) when the attempt gives up first.
- (void)pageStarted;
- (void)pageEnded;
@end

@implementation AsyncCommDelegate {
}
- (void)detachOwner {
    std::lock_guard<std::mutex> lock(ownerMutex);
    owner = nullptr;
}

- (void)pageStarted {
    [self retain];
    std::lock_guard<std::mutex> lock(ownerMutex);
    pageInFlight = YES;
}

- (void)pageEnded {
    BOOL wasInFlight;
    {
        std::lock_guard<std::mutex> lock(ownerMutex);
        wasInFlight = pageInFlight;
        pageInFlight = NO;
    }
    // May free this object; nothing may touch it afterwards.
    if (wasInFlight) [self release];
}

- (void)rfcommChannelClosed:(IOBluetoothRFCOMMChannel *)rfcommChannel{
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] rfcommChannelClosed\n");
#endif
    // Never tear down the channel from inside its own callback. The owner joins the
    // worker and releases native objects in disconnect().
    std::lock_guard<std::mutex> lock(ownerMutex);
    if (owner) owner->channelClosed();
}

// Target of -[IOBluetoothDevice openConnection:]: the baseband link is up or failed.
- (void)connectionComplete:(IOBluetoothDevice *)device status:(IOReturn)status {
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] connectionComplete status=0x%x\n", status);
#endif
    {
        std::lock_guard<std::mutex> lock(ownerMutex);
        if (owner) owner->linkOpenComplete(status);
    }
    [self pageEnded];
}

- (void)rfcommChannelOpenComplete:(IOBluetoothRFCOMMChannel *)rfcommChannel status:(IOReturn)error {
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] rfcommChannelOpenComplete status=0x%x\n", error);
#endif
    std::lock_guard<std::mutex> lock(ownerMutex);
    if (owner) owner->channelOpenComplete(error);
}

-(void)rfcommChannelData:(IOBluetoothRFCOMMChannel *)rfcommChannel data:(void *)dataPointer length:(size_t)dataLength
{
    std::lock_guard<std::mutex> lock(ownerMutex);
    if (owner) owner->channelData(dataPointer, dataLength);
}


@end

#ifdef SHC_DEBUG_PROTOCOL
static void _debugHexDump(const char* label, const char* buf, size_t length)
{
    fprintf(stderr, "[%s] ", label);
    for (size_t i = 0; i < length; i++)
    {
        fprintf(stderr, "%02x ", (unsigned char)buf[i]);
    }
    fprintf(stderr, "\n");
}
#endif

int MacOSBluetoothConnector::send(char* buf, size_t length)
{
#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("send", buf, length);
#endif
    // Hold the lock only to take a reference: a stalled write must not block
    // disconnect(), whose closeChannel is what aborts it.
    IOBluetoothRFCOMMChannel *chan = nil;
    {
        std::lock_guard<std::mutex> lock(channelMutex);
        chan = (__bridge IOBluetoothRFCOMMChannel*)rfcommchannel;
        if (!running || chan == nil || !chan.isOpen) {
            throw RecoverableException("Bluetooth RFCOMM channel is not connected", true);
        }
        CFRetain((__bridge CFTypeRef)chan);
    }
    IOReturn writeResult = kIOReturnError;
    for (int attempt = 1; ; ++attempt) {
        writeResult = [chan writeSync:(void*)buf length:length];
        if (!isTransientWriteError(writeResult) || attempt == 3) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    CFRelease((__bridge CFTypeRef)chan);
    if (writeResult != kIOReturnSuccess) {
        // A silently dropped write surfaces later as an ACK timeout, so report it here.
        char message[64];
        snprintf(message, sizeof message, "RFCOMM write failed (IOReturn 0x%x)", (unsigned)writeResult);
        throw RecoverableException(message, true);
    }
    return (int)length;
}


void MacOSBluetoothConnector::connectToMac(MacOSBluetoothConnector* macOSBluetoothConnector, std::promise<void> connectPromise)
{
    auto& connector = *macOSBluetoothConnector;
    auto fail = [&](const char* message) {
        connectPromise.set_exception(std::make_exception_ptr(RecoverableException(message, false)));
    };
    auto cancelled = [&] {
        std::lock_guard<std::mutex> lock(connector.stateMutex);
        return connector.openState == OpenState::Cancelled;
    };
    char message[128];
    // get device
    IOBluetoothDevice *device = (__bridge IOBluetoothDevice *)connector.rfcommDevice;
    // filled in by openRFCOMMChannelAsync
    IOBluetoothRFCOMMChannel *channel = nil;

    // try the v1 service UUID first, then fall back to the v2 (newer-generation) UUID.
    IOBluetoothSDPUUID *sppServiceUUIDV1 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_IN_BYTES length: 16];
    IOBluetoothSDPServiceRecord *sppServiceRecord = [device getServiceRecordForUUID:sppServiceUUIDV1];
    SonyProtocolVersion protocolVersion = SonyProtocolVersion::V1;
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] v1 getServiceRecordForUUID -> %s\n", sppServiceRecord == nil ? "nil" : "found");
#endif

    if (sppServiceRecord == nil) {
        IOBluetoothSDPUUID *sppServiceUUIDV2 = [IOBluetoothSDPUUID uuidWithBytes:(void*)SERVICE_UUID_V2_IN_BYTES length: 16];
        sppServiceRecord = [device getServiceRecordForUUID:sppServiceUUIDV2];
        protocolVersion = SonyProtocolVersion::V2;
#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[connect] v2 getServiceRecordForUUID -> %s\n", sppServiceRecord == nil ? "nil" : "found");
#endif
    }

    if (sppServiceRecord == nil) {
        fail("Couldn't find the Sony service record on this device (neither protocol version) - is this a supported headset?");
        return;
    }

    // get rfcommChannelID from sppServiceRecord
    UInt8 rfcommChannelID = 0;
    IOReturn channelIdStatus = [sppServiceRecord getRFCOMMChannelID:&rfcommChannelID];
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] protocolVersion=%s getRFCOMMChannelID -> 0x%x, channelID=%u\n", protocolVersion == SonyProtocolVersion::V2 ? "V2" : "V1", channelIdStatus, (unsigned)rfcommChannelID);
#endif
    if (channelIdStatus != kIOReturnSuccess) {
        fail("Found the Sony service record, but it has no RFCOMM channel.");
        return;
    }
    // Let disconnect() and abort() wake this thread wherever it waits.
    {
        std::lock_guard<std::mutex> lock(connector.stateMutex);
        connector.workerRunLoop = (void*)CFRetain(CFRunLoopGetCurrent());
    }
    // Don't start radio work for an attempt that was already aborted.
    if (cancelled()) {
        fail("The connection attempt was cancelled.");
        return;
    }
    // setup delegate; the connector keeps the +1 reference from alloc until disconnect()
    AsyncCommDelegate* asyncCommDelegate = [[AsyncCommDelegate alloc] init];
    asyncCommDelegate->owner = macOSBluetoothConnector;
    connector.rfcommDelegate = (__bridge void*)asyncCommDelegate;
    // One deadline for the whole attempt: bringing the link up, then the channel.
    const auto deadline = std::chrono::steady_clock::now() + connector.openTimeout;

    // Bring a link that is down (headset off, out of range, or not reconnected by
    // macOS) up first. The asynchronous form, because the synchronous openConnection
    // blocks for the whole page timeout and cannot be cancelled.
    if (![device isConnected]) {
        {
            std::lock_guard<std::mutex> lock(connector.stateMutex);
            connector.linkPending = true;
            connector.linkStatus = kIOReturnSuccess;
        }
        // The page outlives this attempt if it is abandoned; its target must too.
        [asyncCommDelegate pageStarted];
        const IOReturn pageResult = [device openConnection:asyncCommDelegate];
#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[connect] openConnection: -> 0x%x\n", pageResult);
#endif
        if (pageResult != kIOReturnSuccess) {
            // Not issued, so no completion will follow.
            [asyncCommDelegate pageEnded];
            connector.linkOpenComplete(pageResult);
        }
        waitOnWorker(connector.stateMutex, connector.stateConditionVariable, deadline,
                     [&] { return !connector.linkPending || connector.openState != OpenState::Pending; });
        bool pending;
        IOReturn status;
        {
            std::lock_guard<std::mutex> lock(connector.stateMutex);
            pending = connector.linkPending;
            status = connector.linkStatus;
        }
        if (cancelled()) {
            fail("The connection attempt was cancelled.");
            return;
        }
        // Judge by the link itself: since 10.7 a link that came up meanwhile (macOS
        // reconnecting the headset) is reported as a "connection exists" error.
        if (![device isConnected]) {
            snprintf(message, sizeof message, "The headset did not answer; is it switched on and in range? (IOReturn 0x%x)",
                     (unsigned)(pending ? kIOReturnTimeout : status));
            fail(message);
            return;
        }
    }

    // try to open channel
    IOReturn openResult = [device openRFCOMMChannelAsync:&channel withChannelID:rfcommChannelID delegate:asyncCommDelegate];
    // Keep the channel for cleanup whatever the outcome. The header says the channel
    // comes back already retained for the caller, but IOBluetooth releases that
    // reference itself once the link goes down: releasing it here as well crashed
    // (message sent to a deallocated channel, macOS 27, NSZombieEnabled) when the
    // headset's link dropped. So hold exactly one reference of our own, released in
    // closeConnection(); the channel is freed once the link is gone.
    connector.retainChannel((__bridge void*)channel);
#ifdef SHC_DEBUG_PROTOCOL
    fprintf(stderr, "[connect] openRFCOMMChannelAsync -> 0x%x\n", openResult);
#endif
    if ( openResult != kIOReturnSuccess ) {
        fail("Could not open the rfcomm.");
        return;
    }

    // openRFCOMMChannelAsync only starts the open; writes before rfcommChannelOpenComplete
    // are lost.
    waitOnWorker(connector.stateMutex, connector.stateConditionVariable, deadline,
                 [&] { return connector.openState != OpenState::Pending; });
    OpenState outcome;
    IOReturn status;
    {
        std::lock_guard<std::mutex> lock(connector.stateMutex);
        outcome = connector.openState;
        status = connector.openStatus;
    }
    if (outcome != OpenState::Opened || !channel.isOpen) {
#ifdef SHC_DEBUG_PROTOCOL
        fprintf(stderr, "[connect] RFCOMM open failed: state=%d status=0x%x\n", (int)outcome, status);
#endif
        switch (outcome) {
            case OpenState::Pending:
                snprintf(message, sizeof message, "Timed out opening the RFCOMM channel.");
                break;
            case OpenState::Cancelled:
                snprintf(message, sizeof message, "The connection attempt was cancelled.");
                break;
            case OpenState::Failed:
                snprintf(message, sizeof message, "The headset refused the RFCOMM channel (IOReturn 0x%x).", (unsigned)status);
                break;
            default:
                snprintf(message, sizeof message, "The RFCOMM channel closed while opening.");
                break;
        }
        fail(message);
        return;
    }

    connector.protocolVersion = protocolVersion;

    connector.running = true;

    // tell the other tread that we are done connecting
    connectPromise.set_value();

    // Service this thread's run loop while it has sources (disconnect() stops it);
    // with none, the callbacks arrive elsewhere, so park until disconnect().
    while (connector.running) {
        @autoreleasepool {
            if (CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.5, false) == kCFRunLoopRunFinished) {
                std::unique_lock<std::mutex> lock(connector.stateMutex);
                connector.stateConditionVariable.wait(lock, [&] {
                    return !connector.running;
                });
            }
        }
    }
}
void MacOSBluetoothConnector::connect(const std::string& addrStr){
    // An abort() from now on applies to this attempt, even one that arrives
    // before the state below is reset.
    const unsigned epoch = cancelEpoch;
    // A failed attempt or remote close leaves the previous worker joinable, and
    // assigning a new thread over a joinable one calls std::terminate().
    disconnect();
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        openState = cancelEpoch == epoch ? OpenState::Pending : OpenState::Cancelled;
        openStatus = kIOReturnSuccess;
    }
    // convert mac address to nsstring
    NSString *addressNSString = [NSString stringWithCString:addrStr.c_str() encoding:[NSString defaultCStringEncoding]];
    // get device based on mac address
    IOBluetoothDevice *device = [IOBluetoothDevice deviceWithAddressString:addressNSString];
    if (device == nil) {
        throw RecoverableException("Could not resolve the selected Bluetooth device.", false);
    }
    std::promise<void> connectPromise;
    std::future<void> connectFuture = connectPromise.get_future();

    // store the device in a variable
    rfcommDevice = (void*)CFRetain((__bridge CFTypeRef)device);
    uthread = std::thread(MacOSBluetoothConnector::connectToMac, this, std::move(connectPromise));
    
    // wait till the device is connected
    try {
        connectFuture.get();
    } catch (...) {
        disconnect();
        throw;
    }
}

int MacOSBluetoothConnector::recv(char* buf, size_t length)
{
    // wait for newly received data, but time out so an unanswered inquiry (probing an unsupported
    // feature) or a dropped link doesn't block the caller forever.
    std::unique_lock<std::mutex> g(receiveDataMutex);
    bool gotData = receiveDataConditionVariable.wait_for(g, std::chrono::milliseconds(2500),
        [this]{ return !running || !receivedBytes.empty(); });
    if (receivedBytes.empty()) {
        if (!running) {
            throw RecoverableException("Bluetooth disconnected", true);
        }
        if (!gotData) {
            throw RecoverableException("recv timed out", false);
        }
    }

    // fill the buf with the new data
    std::vector<unsigned char> receivedVector = std::move(receivedBytes.front());
    receivedBytes.pop_front();
    
    size_t lengthCopied = std::min(length, receivedVector.size());

    // copy the first amount of bytes
    std::memcpy(buf, receivedVector.data(), lengthCopied);
    
    // too much data, save it for next time
    if (receivedVector.size() > length){
        receivedVector.erase(receivedVector.begin(), receivedVector.begin() + lengthCopied);
        receivedBytes.push_front(std::move(receivedVector));
    }

#ifdef SHC_DEBUG_PROTOCOL
    _debugHexDump("recv", buf, lengthCopied);
#endif

    return (int)lengthCopied;
}

SonyProtocolVersion MacOSBluetoothConnector::getProtocolVersion() noexcept
{
    return protocolVersion;
}

std::vector<BluetoothDevice> MacOSBluetoothConnector::getConnectedDevices()
{
    // create the output vector
    std::vector<BluetoothDevice> res;
    // List every paired device, not just ones macOS reports as connected: [isConnected] returns NO for
    // headsets connected only for audio/BLE (e.g. Sony ULT WEAR), which hid them from the picker. connect()
    // opens the RFCOMM link on demand, so a paired-but-"disconnected" device still works.
    // The list includes phones, mice, and other headphones. SonyDeviceDiscovery in sony-transport
    // keeps only the Sony devices.
    for (IOBluetoothDevice* device in [IOBluetoothDevice pairedDevices]) {
        if (![device addressString]) continue;
        BluetoothDevice dev;
        dev.mac = [[device addressString] UTF8String];
        dev.name = [device name] ? [[device name] UTF8String] : "Unknown Device";
        dev.paired = true;
        dev.connected = [device isConnected] == YES;
        res.push_back(dev);
    }
    
    return res;
}

void MacOSBluetoothConnector::wakeWorkerLocked() noexcept
{
    stateConditionVariable.notify_all();
    if (workerRunLoop) {
        CFRunLoopStop((CFRunLoopRef)workerRunLoop);
    }
}

void MacOSBluetoothConnector::channelOpenComplete(IOReturn status) noexcept
{
    std::lock_guard<std::mutex> lock(stateMutex);
    if (openState == OpenState::Pending) {
        openStatus = status;
        openState = status == kIOReturnSuccess ? OpenState::Opened : OpenState::Failed;
    }
    wakeWorkerLocked();
}

void MacOSBluetoothConnector::linkOpenComplete(IOReturn status) noexcept
{
    std::lock_guard<std::mutex> lock(stateMutex);
    linkPending = false;
    linkStatus = status;
    wakeWorkerLocked();
}

void MacOSBluetoothConnector::abort() noexcept
{
    ++cancelEpoch;
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (openState == OpenState::Pending) {
            openState = OpenState::Cancelled;
        }
        // An open connection stops too, so a recv() blocked on another thread (the
        // session's reader) returns now instead of after its timeout.
        running = false;
        wakeWorkerLocked();
    }
    receiveDataConditionVariable.notify_all();
}

void MacOSBluetoothConnector::channelData(const void* data, size_t length)
{
    std::lock_guard<std::mutex> g(receiveDataMutex);
    const unsigned char* buffer = static_cast<const unsigned char*>(data);
    receivedBytes.emplace_back(buffer, buffer + length);
    receiveDataConditionVariable.notify_one();
}

void MacOSBluetoothConnector::channelClosed() noexcept
{
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (openState == OpenState::Pending) {
            openState = OpenState::Closed;
        }
        running = false;
        wakeWorkerLocked();
    }
    receiveDataConditionVariable.notify_all();
}

void MacOSBluetoothConnector::disconnect() noexcept
{
    // Stop the worker (cancelling an open still in progress) and wake readers.
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (openState == OpenState::Pending) {
            openState = OpenState::Cancelled;
        }
        running = false;
        wakeWorkerLocked();
    }
    receiveDataConditionVariable.notify_all();
    // Teardown runs on the owning thread, never on the worker or in a delegate callback.
    assert(!uthread.joinable() || uthread.get_id() != std::this_thread::get_id());
    if (uthread.joinable()) {
        uthread.join();
    }
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (workerRunLoop) {
            CFRelease((CFTypeRef)workerRunLoop);
            workerRunLoop = nullptr;
        }
    }
    AsyncCommDelegate* delegate = (__bridge AsyncCommDelegate*)rfcommDelegate;
    rfcommDelegate = nullptr;
    // Waits for a callback still running elsewhere; none reaches this connector afterwards.
    [delegate detachOwner];
    closeConnection();
    if (delegate) {
        // Callbacks arrive on the main run loop. Releasing there means one that was
        // already dispatched finishes before the delegate is freed.
        dispatch_async(dispatch_get_main_queue(), ^{
            [delegate release];
        });
    }
    if (rfcommDevice) {
        CFRelease((CFTypeRef)rfcommDevice);
        rfcommDevice = nullptr;
    }
    std::lock_guard<std::mutex> lock(receiveDataMutex);
    receivedBytes.clear();
}
void MacOSBluetoothConnector::retainChannel(void* channel) {
    std::lock_guard<std::mutex> lock(channelMutex);
    rfcommchannel = channel ? (void*)CFRetain((CFTypeRef)channel) : nullptr;
}
void MacOSBluetoothConnector::closeConnection() {
    std::lock_guard<std::mutex> lock(channelMutex);
    // take the channel so a repeated close is a no-op
    IOBluetoothRFCOMMChannel *chan = (__bridge IOBluetoothRFCOMMChannel*) rfcommchannel;
    rfcommchannel = nullptr;
    if (chan == nil) {
        return;
    }
    // Detach the delegate first so a local close does not re-enter through rfcommChannelClosed.
    [chan setDelegate: nil];
    // close the channel
    [chan closeChannel];
    CFRelease((__bridge CFTypeRef)chan);
}


bool MacOSBluetoothConnector::isConnected() noexcept
{
    if (!running)
        return false;
    std::lock_guard<std::mutex> lock(channelMutex);
    IOBluetoothRFCOMMChannel *chan = (__bridge IOBluetoothRFCOMMChannel*) rfcommchannel;
    return chan != nil && chan.isOpen;
}
