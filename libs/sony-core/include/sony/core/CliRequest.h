#pragma once

#include "JsonProtocol.h"

#include <string>
#include <vector>

namespace sony::core {

// sonyctl's command words turned into a structured JsonProtocol request, so
// the CLI can talk to sonyd (or a direct session) in the same typed form the
// GUI uses and print the reply as JSON. `select` names the part of the
// response data worth printing for read-only commands: "" for everything,
// "battery" / "equalizer" for that object, "status" for the connection
// summary.
struct CliRequest {
    JsonProtocol::Json request;
    std::string select;
};

// Throws std::invalid_argument for an unknown or malformed command; the
// message is what the user should read.
[[nodiscard]] CliRequest cliRequestFor(const std::vector<std::string>& tokens);

// Applies CliRequest::select to a JsonProtocol response. Error responses and
// an empty select pass through untouched.
[[nodiscard]] JsonProtocol::Json cliSelect(const JsonProtocol::Json& response, const std::string& select);

} // namespace sony::core
