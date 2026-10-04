#pragma once
#include <stdio.h>
#include "../IBluetoothConnector.h"
#include "IOBluetooth/IOBluetooth.h"
#include "Constants.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <deque>
#include <future>

class MacOSBluetoothConnector final : public IBluetoothConnector
{
public:
    MacOSBluetoothConnector();
    ~MacOSBluetoothConnector();
    static void connectToMac(MacOSBluetoothConnector* MacOSBluetoothConnector, std::promise<void> connectPromise) noexcept(false);
    virtual void connect(const std::string& addrStr) noexcept(false);
    virtual int send(char* buf, size_t length) noexcept(false);
    virtual int recv(char* buf, size_t length) noexcept(false);
    virtual void disconnect() noexcept;
    virtual bool isConnected() noexcept;
    virtual void closeConnection();
    virtual SonyProtocolVersion getProtocolVersion() noexcept;

    // Called by the RFCOMM delegate, usually on the main thread's run loop. They only
    // record state and wake waiters; they never close the channel or join the worker.
    void channelOpenComplete(IOReturn status) noexcept;
    void channelData(const void* data, size_t length);
    void channelClosed() noexcept;

    virtual std::vector<BluetoothDevice> getConnectedDevices() noexcept(false);
    std::deque<std::vector<unsigned char>> receivedBytes;
    std::mutex receiveDataMutex;
    std::condition_variable receiveDataConditionVariable;
    std::atomic<bool> running = false;
    //Set on the connectToMac background thread before the connect promise resolves; safe to read from
    //any thread afterwards (the future's get() synchronizes-with the promise's set_value()).
    SonyProtocolVersion protocolVersion = SonyProtocolVersion::V1;

private:
    friend struct MacOSBluetoothConnectorTestAccess;

    // Outcome of the asynchronous RFCOMM open; only Opened makes the channel usable.
    enum class OpenState { Idle, Pending, Opened, Failed, Closed, Cancelled };
    // Wakes the worker while it waits for the open, or parks it until disconnect().
    void wakeWorkerLocked() noexcept;

    std::mutex stateMutex;
    std::condition_variable stateConditionVariable;
    OpenState openState = OpenState::Idle;
    IOReturn openStatus = kIOReturnSuccess;
    std::chrono::milliseconds openTimeout{std::chrono::seconds(10)};
    void *workerRunLoop = nullptr;

    // This file is built without ARC. The connector owns one reference to each native
    // object: IOBluetooth can release its own channel reference on a remote or failed
    // open before disconnect() runs, so a borrowed pointer would dangle.
    void retainChannel(void* channel);
    std::mutex channelMutex;
    void *rfcommDevice = nullptr;
    void *rfcommchannel = nullptr;
    void *rfcommDelegate = nullptr;
    std::thread uthread;
};
