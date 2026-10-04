#include <catch2/catch_test_macros.hpp>
#include "sony/core/DeviceService.h"
#include "sony/core/JsonProtocol.h"
#include "sony/core/IpcServer.h"
#include "sony/core/IpcClient.h"
#include "../support/PrivateSocket.h"
#include "../support/ReplyTransport.h"
#include <condition_variable>
#include <future>
#include <mutex>
#include <thread>
#include <fstream>
#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <cstring>
#endif
using namespace sony;
using namespace sony::core;
using namespace sony::transport;
using Json = JsonProtocol::Json;

TEST_CASE("Lifecycle retries with backoff and rediscovers without a restart", "[core][recovery]") {
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    DeviceService service(transport, discovery, [&] { return now; });
    service.startAutoConnect(); service.tick();
    CHECK(service.connectionState() == "retrying");
    discovery->addDevice({"WH-1000XM3", DeviceAddress("11:22:33:44:55:66"), true, true});
    service.tick(); CHECK(transport->attempts.empty());
    now += std::chrono::seconds(1); service.tick();
    REQUIRE(service.isConnected());
    CHECK(transport->attempts.size() == 1);
    transport->simulateDisconnect(); service.tick();
    REQUIRE(service.isConnected());
    CHECK(transport->attempts.size() == 2);
    service.disconnect(); now += std::chrono::hours(1); service.tick();
    CHECK_FALSE(service.isConnected()); CHECK(transport->attempts.size() == 2);
    CHECK(service.connectionState() == "manually_disconnected");
    for (const auto& raw : transport->sentFrames()) {
        const auto frame = protocol::FrameCodec::decode(raw);
        if (!frame.payload.empty()) CHECK(frame.payload[0] != 0x22);
    }
}
TEST_CASE("Lifecycle prefers connected candidates and honors explicit selection", "[core][recovery]") {
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->setDevices({{"WH-1000XM3",DeviceAddress("11:22:33:44:55:66"),true,false},
        {"WH-1000XM3",DeviceAddress("22:22:33:44:55:66"),true,true}});
    DeviceService service(transport, discovery);
    SECTION("Connected candidate first") {
        service.startAutoConnect(); service.tick();
        CHECK(service.selectedAddress() == "22:22:33:44:55:66");
    }
    SECTION("Failed candidate falls through") {
        transport->failAddress = "22:22:33:44:55:66";
        service.startAutoConnect(); service.tick();
        CHECK(service.selectedAddress() == "11:22:33:44:55:66");
        CHECK(transport->attempts.size() == 2);
    }
    SECTION("Explicit address is the only candidate") {
        service.startAutoConnect("11:22:33:44:55:66"); service.tick();
        CHECK(service.selectedAddress() == "11:22:33:44:55:66");
        CHECK(transport->attempts.size() == 1);
    }
}
namespace {
// macOS discovery lists "14-3f-a6-a3-da-e0"; people type "14:3F:A6:A3:DA:E0".
struct TypedAddressFixture {
    std::shared_ptr<ReplyTransport> transport = std::make_shared<ReplyTransport>();
    std::shared_ptr<FakeDeviceDiscovery> discovery = [] {
        auto d = std::make_shared<FakeDeviceDiscovery>();
        d->setDevices({{"WH-1000XM4", DeviceAddress("14-3f-a6-a3-da-e0"), true, true}});
        return d;
    }();
    DeviceService service{transport, discovery};

    void expectResolved() {
        REQUIRE(service.isConnected());
        // The discovered name selects the XM4 profile instead of the empty fallback.
        CHECK(service.activeDevice()->name() == "WH-1000XM4");
        CHECK(JsonProtocol::snapshot(service)["capabilities"]["anc"] == true);
        // The discovered form, so the GUI's device list still matches the active device.
        CHECK(service.selectedAddress() == "14-3f-a6-a3-da-e0");
        CHECK(transport->attempts == std::vector<std::string>{"14-3f-a6-a3-da-e0"});
    }
};
} // namespace
TEST_CASE("A typed address resolves the discovered device whatever its format", "[core][recovery]") {
    TypedAddressFixture f;
    SECTION("sonyd -d starts auto-connect with a typed address") {
        f.service.startAutoConnect("14:3F:A6:A3:DA:E0"); f.service.tick();
        f.expectResolved();
    }
    SECTION("connect without a name") {
        f.service.connect(DeviceAddress("14:3F:A6:A3:DA:E0"), "");
        f.expectResolved();
    }
}
TEST_CASE("JSON connect accepts a listed address and rejects malformed ones", "[core][json]") {
    TypedAddressFixture f;
    SECTION("the address the devices list returned") {
        const auto devices = JsonProtocol::execute({{"version",1},{"id",1},{"method","devices"}}, f.service);
        const auto listed = devices["data"][0]["address"].get<std::string>();
        const auto reply = JsonProtocol::execute({{"version",1},{"id",2},{"method","connect"},
            {"params",{{"address",listed},{"name",""}}}}, f.service);
        CHECK(reply["ok"] == true);
        f.expectResolved();
    }
    SECTION("a malformed address") {
        const auto reply = JsonProtocol::execute({{"version",1},{"id",3},{"method","connect"},
            {"params",{{"address","14.3f.a6.a3.da.e0"}}}}, f.service);
        CHECK(reply["error"]["code"] == "InvalidRequest");
        CHECK(f.transport->attempts.empty());
    }
}
TEST_CASE("An explicit connect name is kept", "[core][recovery]") {
    TypedAddressFixture f;
    f.service.connect(DeviceAddress("14:3F:A6:A3:DA:E0"), "WH-1000XM4 (desk)");
    REQUIRE(f.service.isConnected());
    CHECK(f.service.activeDevice()->name() == "WH-1000XM4 (desk)");
}
namespace {
// connect() hangs like paging a headset that is off, until abort().
class HangingTransport : public FakeTransport {
public:
    std::atomic<int> attempts{0};
    void connect(const DeviceAddress&) override {
        std::unique_lock lock(_mutex);
        ++attempts; _entered = true; _cv.notify_all();
        _cv.wait_for(lock, std::chrono::seconds(10), [this] { return _cancelled; });
        throw SonyException(SonyErrorCode::TransportFailure, "The connection attempt was cancelled.");
    }
    void abort() noexcept override {
        std::lock_guard lock(_mutex); _cancelled = true; _cv.notify_all();
    }
    void waitUntilConnecting() {
        std::unique_lock lock(_mutex);
        _cv.wait_for(lock, std::chrono::seconds(5), [this] { return _entered; });
    }
private:
    std::mutex _mutex;
    std::condition_variable _cv;
    bool _entered{false}, _cancelled{false};
};
} // namespace
TEST_CASE("Shutdown ends a connection attempt in progress and starts no other", "[core][recovery]") {
    auto transport = std::make_shared<HangingTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->setDevices({{"WH-1000XM4",DeviceAddress("11:22:33:44:55:66"),true,true},
        {"WH-1000XM4",DeviceAddress("22:22:33:44:55:66"),true,true}});
    DeviceService service(transport, discovery);
    service.startAutoConnect();
    std::thread worker([&] { service.tick(); });
    transport->waitUntilConnecting();
    // From another thread, while tick() holds the service lock.
    const auto start = std::chrono::steady_clock::now();
    service.requestShutdown();
    worker.join();
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
    CHECK(transport->attempts == 1); // the second candidate is not tried
    service.tick();
    CHECK(transport->attempts == 1); // and no later tick starts another
}
TEST_CASE("Shutdown ends a headset request that is waiting for an answer", "[core][recovery]") {
    // Cmd+Q right after switching the headset off, before the link loss is noticed.
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    DeviceService service(transport, discovery, [&] { return now; });
    service.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM4");
    REQUIRE(service.isConnected());
    transport->silent = true;
    now += std::chrono::seconds(10); // due for a settings refresh
    const auto sentBefore = transport->sentCount();
    std::thread worker([&] { service.tick(); });
    // Only once the refresh request is out is there something to abort.
    for (int i = 0; i < 400 && transport->sentCount() == sentBefore; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    CHECK(transport->sentCount() > sentBefore);
    const auto start = std::chrono::steady_clock::now();
    service.requestShutdown();
    worker.join();
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(500));
}
TEST_CASE("Connect after a shutdown request fails without an attempt", "[core][recovery]") {
    auto transport = std::make_shared<ReplyTransport>();
    DeviceService service(transport);
    service.requestShutdown();
    CHECK_THROWS(service.connect(DeviceAddress("11:22:33:44:55:66"), "WH-1000XM4"));
    CHECK(transport->attempts.empty());
}
TEST_CASE("Retry delay doubles and is capped at thirty seconds", "[core][recovery]") {
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    transport->failAddress = "11:22:33:44:55:66";
    DeviceService service(transport, {}, [&] { return now; });
    service.startAutoConnect(transport->failAddress); service.tick();
    size_t count = 1;
    for (int delay : {1,2,4,8,16,30,30}) {
        now += std::chrono::milliseconds(delay * 1000 - 1); service.tick();
        CHECK(transport->attempts.size() == count);
        now += std::chrono::milliseconds(1); service.tick();
        CHECK(transport->attempts.size() == ++count);
    }
}
TEST_CASE("A wake-up cuts the retry backoff short", "[core][recovery]") {
    auto now = DeviceService::Clock::time_point{};
    auto transport = std::make_shared<ReplyTransport>();
    transport->failAddress = "11:22:33:44:55:66";
    DeviceService service(transport, {}, [&] { return now; });
    // Before auto-connect a wake-up is meaningless and must not connect.
    service.wake(); service.tick();
    CHECK(transport->attempts.empty());
    service.startAutoConnect(transport->failAddress); service.tick();
    now += std::chrono::seconds(1); service.tick();
    now += std::chrono::seconds(2); service.tick();
    REQUIRE(transport->attempts.size() == 3);
    // Backoff is now 4 s; the OS says the headphones just showed up.
    now += std::chrono::seconds(1); service.tick();
    CHECK(transport->attempts.size() == 3);
    service.wake(); service.tick();
    CHECK(transport->attempts.size() == 4);
    // ...and the backoff restarts from one second, not from where it was.
    now += std::chrono::milliseconds(999); service.tick();
    CHECK(transport->attempts.size() == 4);
    now += std::chrono::milliseconds(1); service.tick();
    CHECK(transport->attempts.size() == 5);
    // Once connected, wake-ups do nothing.
    transport->failAddress.clear();
    now += std::chrono::seconds(2); service.tick();
    REQUIRE(service.isConnected());
    const auto attempts = transport->attempts.size();
    service.wake(); service.tick();
    CHECK(transport->attempts.size() == attempts);
}
TEST_CASE("Structured snapshots are escaped versioned and truthful", "[core][json]") {
    auto transport = std::make_shared<ReplyTransport>();
    auto discovery = std::make_shared<FakeDeviceDiscovery>();
    discovery->addDevice({"name\n\"|<img> 日本",DeviceAddress("11:22:33:44:55:66")});
    DeviceService service(transport, discovery);
    auto request = Json{{"version",1},{"id","test"},{"method","devices"}};
    auto response = Json::parse(JsonProtocol::executeLine(request.dump(), service));
    CHECK(response["id"] == "test");
    CHECK(response["data"][0]["name"] == "name\n\"|<img> 日本");
    request["version"] = 9;
    CHECK(JsonProtocol::execute(request,service)["error"]["code"] == "VersionMismatch");
    CHECK(Json::parse(JsonProtocol::executeLine("{broken",service))["ok"] == false);
    const auto deep = std::string(40, '[') + "0" + std::string(40, ']');
    CHECK(Json::parse(JsonProtocol::executeLine(deep,service))["error"]["code"] == "InvalidRequest");
    service.connect(DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
    const auto snapshot = JsonProtocol::snapshot(service);
    CHECK(snapshot["connected"] == true);
    CHECK(snapshot["equalizer"]["bands"] == Json::array({1,2,3,4,5}));
    CHECK(snapshot["features"]["noiseControl"]["availability"] == "valid");
    CHECK(snapshot["codec"] == "Unknown");
    CHECK(JsonProtocol::execute({{"version",1},{"id",1},{"method","ambient"},{"params",{{"level",21}}}},service)["error"]["code"] == "InvalidRequest");
    service.disconnect();
    CHECK(JsonProtocol::snapshot(service)["features"]["noiseControl"]["availability"] == "stale");
    auto status = IpcProtocol::execute(IpcProtocol::parseCommand("status"),service);
    CHECK_FALSE(status.success); CHECK(status.message == "Device disconnected");
}
TEST_CASE("Notification callbacks may read state from another thread", "[core][events]") {
    auto transport = std::make_shared<ReplyTransport>();
    SonyDevice device(transport); device.connect(DeviceAddress("11:22:33:44:55:66"),"WH-1000XM5");
    std::promise<bool> completed;
    auto result = completed.get_future();
    auto id = device.events().onStateChanged([&](const auto&) {
        auto read = std::async(std::launch::async, [&] { return device.snapshot()->noiseControl.ambientLevel; });
        completed.set_value(read.get() == 12);
    });
    transport->notify({0x69,0x17,1,1,1,0,12});
    REQUIRE(result.wait_for(std::chrono::seconds(2)) == std::future_status::ready);
    CHECK(result.get()); device.events().removeListener(id);
}
#ifndef _WIN32
namespace {
struct ClientFd {
    int fd{-1};
    explicit ClientFd(const std::string& path) {
        fd = ::socket(AF_UNIX,SOCK_STREAM,0);
        sockaddr_un address{}; address.sun_family=AF_UNIX;
        std::strcpy(address.sun_path,path.c_str());
        REQUIRE(::connect(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0);
    }
    ~ClientFd() { if (fd >= 0) ::close(fd); }
};
}
TEST_CASE("IPC isolates idle oversized and disappearing clients", "[core][ipc][security]") {
    PrivateSocket socket;
    auto service = std::make_shared<DeviceService>(std::make_shared<FakeTransport>());
    IpcServer server(service,socket.path); server.start();
    IpcClient client(socket.path);
    SECTION("Idle client does not delay other clients") {
        ClientFd idle(socket.path);
        auto result = client.sendCommand("devices",std::chrono::milliseconds(300));
        CHECK(result.success);
    }
    SECTION("Oversized request is disconnected") {
        ClientFd attacker(socket.path);
        std::string bytes(17*1024,'x');
#ifdef MSG_NOSIGNAL
        ::send(attacker.fd,bytes.data(),bytes.size(),MSG_NOSIGNAL);
#else
        ::write(attacker.fd,bytes.data(),bytes.size());
#endif
        pollfd pfd{attacker.fd,POLLIN,0}; REQUIRE(::poll(&pfd,1,2000) > 0);
        char b; CHECK(::read(attacker.fd,&b,1) <= 0);
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("Client disappears before response") {
        { ClientFd vanished(socket.path); ::write(vanished.fd,"devices\n",8); }
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("JSON and legacy clients coexist") {
        auto response = Json::parse(client.request(R"({"version":1,"id":7,"method":"snapshot"})"));
        CHECK(response["ok"] == true); CHECK(response["id"] == 7);
        CHECK(client.sendCommand("devices").success);
    }
    SECTION("Second daemon cannot evict the first") {
        IpcServer second(service,socket.path); CHECK_THROWS(second.start());
        second.stop(); CHECK(client.sendCommand("devices").success);
    }
    struct stat st{}; REQUIRE(::lstat(socket.path.c_str(),&st) == 0);
    CHECK((st.st_mode & 0777) == 0600);
}
TEST_CASE("IPC refuses insecure paths and recovers an owned stale socket", "[core][ipc][security]") {
    PrivateSocket socket;
    auto service = std::make_shared<DeviceService>(std::make_shared<FakeTransport>());
    SECTION("Private parent required") {
        ::chmod(socket.directory.c_str(),0755);
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
    }
    SECTION("Never replaces a regular file") {
        std::ofstream(socket.path) << "keep";
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
        CHECK(std::filesystem::file_size(socket.path) == 4);
    }
    SECTION("Never follows a symlink") {
        std::filesystem::create_symlink(socket.directory / "missing",socket.path);
        IpcServer server(service,socket.path); CHECK_THROWS(server.start());
        CHECK(std::filesystem::is_symlink(socket.path));
    }
    SECTION("Stale socket can be reclaimed") {
        int fd = ::socket(AF_UNIX,SOCK_STREAM,0);
        sockaddr_un address{}; address.sun_family=AF_UNIX; std::strcpy(address.sun_path,socket.path.c_str());
        REQUIRE(::bind(fd,reinterpret_cast<sockaddr*>(&address),sizeof(address)) == 0); ::close(fd);
        IpcServer server(service,socket.path); REQUIRE_NOTHROW(server.start());
        CHECK(IpcClient(socket.path).sendCommand("devices").success);
    }
}
TEST_CASE("An aborted IPC request returns at once, and later ones fail", "[core][ipc]") {
    // A daemon stand-in that accepts connections and never answers, like sonyd
    // busy in a connection attempt.
    PrivateSocket socket;
    const int listener = ::socket(AF_UNIX, SOCK_STREAM, 0);
    sockaddr_un address{}; address.sun_family = AF_UNIX;
    std::strcpy(address.sun_path, socket.path.c_str());
    REQUIRE(::bind(listener, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    REQUIRE(::listen(listener, 4) == 0);
    IpcClient client(socket.path);
    std::thread aborter([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        client.abort();
    });
    const auto start = std::chrono::steady_clock::now();
    CHECK_THROWS(client.request(R"({"version":1,"id":1,"method":"snapshot"})", std::chrono::seconds(10)));
    CHECK(std::chrono::steady_clock::now() - start < std::chrono::seconds(2));
    aborter.join();
    const auto again = std::chrono::steady_clock::now();
    CHECK_THROWS(client.request(R"({"version":1,"id":2,"method":"snapshot"})", std::chrono::seconds(10)));
    CHECK(std::chrono::steady_clock::now() - again < std::chrono::milliseconds(500));
    ::close(listener);
}
#endif
