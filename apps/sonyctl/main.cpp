#include "sony/core/CliRequest.h"
#include "sony/core/DeviceService.h"
#include "sony/core/IpcClient.h"
#include "sony/core/IpcProtocol.h"
#include "sony/core/JsonProtocol.h"
#include "sony/protocol/FrameCodec.h"
#include "sony/transport/FakeTransport.h"
#include "sony/transport/PlatformTransport.h"
#include "sony/transport/Logger.h"


#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace sony;
using namespace sony::core;
using namespace sony::protocol;
using namespace sony::transport;

namespace {

void printHelp() {
    std::cout << "sonyctl — CLI diagnostic and control tool for Sony audio devices\n\n"
              << "Usage: sonyctl [options] <command> [args...]\n\n"
              << "Commands:\n"
              << "  devices                    List discovered paired Sony devices\n"
              << "  info                       Display connected device information & capabilities\n"
              << "  battery                    Display battery percentage and charging state\n"
              << "  anc on|off                 Enable or disable Active Noise Cancelling\n"
              << "  ambient <1-20>|off         Set Ambient Sound level or turn ambient off\n"
              << "  eq get                     Display active Equalizer preset and band levels\n"
              << "  eq preset <name>           Set Equalizer preset (bright, bass-boost, vocal, etc.)\n"
              << "  eq custom <cb> <b1..b5>    Apply custom Clear Bass (-10..10) and 5 EQ bands\n"
              << "  eq <preset>                Shorthand for eq preset <preset>\n"
              << "  dsee on|off|auto           Toggle DSEE sound enhancement\n"
              << "  stc on|off                 Toggle Speak-to-Chat\n"
              << "  adaptive on|off            Toggle Adaptive Volume\n"
              << "  apo get|<0-5>              Read or set Auto-Power-Off preset index\n"
              << "  power off                  Turn the headphones off\n"
              << "  connect <address> [name]   Connect to a specific paired device\n"
              << "  disconnect                 Drop the current connection\n"
              << "  status                     Display connection status\n\n"
              << "Options:\n"
              << "  -j, --json                 Print the structured reply as one line of JSON\n"
              << "                             ({\"version\":1,\"ok\":true,\"data\":...} or an \"error\" object)\n"
              << "  -s, --socket <path>        Custom Unix domain socket path for sonyd\n"
              << "  --direct                   Direct execution bypassing daemon\n"
              << "  -v, --verbose              Enable verbose diagnostic logging\n"
              << "  -h, --help                 Display this help menu\n\n"
              << "Examples:\n"
              << "  sonyctl anc on\n"
              << "  sonyctl ambient 10\n"
              << "  sonyctl eq bass-boost\n"
              << "  sonyctl dsee on\n"
              << "  sonyctl --json battery     # {\"version\":1,\"id\":1,\"ok\":true,\"data\":{\"main\":85,...}}\n";
}

// Prints a structured reply as one line and turns it into an exit code.
int emitJson(const JsonProtocol::Json& response) {
    std::cout << response.dump() << "\n";
    return response.value("ok", false) ? 0 : 1;
}

JsonProtocol::Json errorReply(const char* code, const std::string& message) {
    return {{"version", JsonProtocol::Version}, {"id", nullptr}, {"ok", false},
            {"error", {{"code", code}, {"message", message}}}};
}

// A structured reply rendered for a human, for the commands the legacy
// line parser does not know.
int emitText(const JsonProtocol::Json& response) {
    if (response.value("ok", false)) {
        std::cout << "OK\n";
        return 0;
    }
    std::cerr << "Error: " << response.value("error", JsonProtocol::Json::object()).value("message", "unknown error") << "\n";
    return 1;
}

int emitLegacy(const IpcResponse& resp) {
    if (resp.success) {
        if (!resp.data.empty()) std::cout << resp.data << "\n";
        else if (!resp.message.empty()) std::cout << resp.message << "\n";
        return 0;
    }
    std::cerr << "Error: " << resp.message << "\n";
    return 1;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string socketPath = defaultSocketPath();
    bool direct = false;
    bool verbose = false;
    bool json = false;
    std::vector<std::string> commandTokens;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printHelp();
            return 0;
        } else if ((arg == "-s" || arg == "--socket") && i + 1 < argc) {
            socketPath = argv[++i];
        } else if (arg == "--direct") {
            direct = true;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else if (arg == "-j" || arg == "--json") {
            json = true;
        } else {
            commandTokens.push_back(std::move(arg));
        }
    }

    if (commandTokens.empty()) {
        printHelp();
        return 0;
    }

    if (verbose) {
        Logger::setLogLevel(LogLevel::Debug);
        Logger::setDeveloperMode(true);
    }

    // Build command line string
    std::ostringstream oss;
    for (size_t i = 0; i < commandTokens.size(); ++i) {
        oss << commandTokens[i];
        if (i + 1 < commandTokens.size()) oss << " ";
    }
    std::string commandLine = oss.str();

    // The legacy line parser keeps the original commands and their prose
    // replies. Everything else, and all of --json, goes through the typed
    // request the GUI uses; the daemon accepts both on the same socket.
    const auto legacyCommand = IpcProtocol::parseCommand(commandLine);
    const bool legacy = !json && legacyCommand.type != IpcCommandType::Unknown;
    CliRequest typed;
    if (!legacy) {
        try {
            typed = cliRequestFor(commandTokens);
        } catch (const std::invalid_argument& ex) {
            if (json) return emitJson(errorReply("InvalidRequest", ex.what()));
            std::cerr << "Error: " << ex.what() << "\n";
            return 1;
        }
    }
    auto emitTyped = [&](const JsonProtocol::Json& response) {
        return json ? emitJson(cliSelect(response, typed.select)) : emitText(response);
    };

    IpcClient daemon(socketPath);
    if (direct && daemon.isDaemonRunning()) {
        const std::string message = "sonyd is running and may own the Bluetooth session. Stop sonyd before using --direct.";
        if (json) return emitJson(errorReply("DaemonRunning", message));
        std::cerr << "Error: " << message << "\n";
        return 1;
    }
    // If direct execution requested or daemon not running, connect via local DeviceService
    if (!direct) {
        IpcClient client(socketPath);
        if (client.isDaemonRunning()) {
            if (legacy) return emitLegacy(client.sendCommand(commandLine));
            const auto raw = client.request(typed.request.dump());
            const auto parsed = JsonProtocol::Json::parse(raw, nullptr, false);
            if (parsed.is_discarded()) return emitTyped(errorReply("ServiceError", "Unreadable reply from sonyd: " + raw));
            return emitTyped(parsed);
        }
    }

    // Fallback or Direct mode
    if (direct) std::cerr << "Using a direct Bluetooth session (--direct).\n";
    else std::cerr << "Notice: sonyd daemon is not running at " << socketPath << "\n"
              << "Starting direct session...\n";

    std::shared_ptr<ITransport> transport = transport::createPlatformTransport();
    std::shared_ptr<IDeviceDiscovery> discovery = transport::createPlatformDiscovery();
    DeviceService service(transport, discovery);

    const auto method = legacy ? std::string{} : typed.request.value("method", std::string{});
    const bool needsDevice = legacy ? legacyCommand.type != IpcCommandType::Devices
                                    : method != "devices" && method != "connect";
    if (needsDevice) {
        auto devs = service.discoverDevices();
        if (!devs.empty()) {
            try {
                service.connect(DeviceAddress(devs.front().address), devs.front().name);
            } catch (const std::exception& ex) {
                std::cerr << "Notice: initial connection to " << devs.front().name << " deferred: " << ex.what() << "\n";
            }
        }
    }

    if (legacy) return emitLegacy(IpcProtocol::execute(legacyCommand, service));
    return emitTyped(JsonProtocol::execute(typed.request, service));
}
