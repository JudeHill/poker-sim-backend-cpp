#pragma once
#include <pistache/router.h>
#include <pistache/http.h>
#include <optional>
#include <string>
#include "external/json.hpp"

namespace HttpHelpers {

// JSON = nlohmann::json
using json = nlohmann::json;

// Parse request body as JSON. Returns empty object if body is empty,
// or std::nullopt on parse error.
std::optional<json> parseBody(const Pistache::Rest::Request& req);

// Send a JSON response with correct Content-Type.
void sendJson(Pistache::Http::ResponseWriter res,
              Pistache::Http::Code code,
              const json& body);

// Quick query-param getter with default.
std::string qp(const Pistache::Rest::Request& req,
               const std::string& key,
               const std::string& def = "");

// Add permissive CORS headers (adjust for production).
void cors(Pistache::Http::ResponseWriter& res);

// Convenience helpers (optional)
inline void badRequest(Pistache::Http::ResponseWriter res, const std::string& msg) {
    sendJson(std::move(res), Pistache::Http::Code::Bad_Request, {{"error", msg}});
}
inline void unauthorized(Pistache::Http::ResponseWriter res) {
    sendJson(std::move(res), Pistache::Http::Code::Unauthorized, {{"error","unauthorized"}});
}
inline void notFound(Pistache::Http::ResponseWriter res) {
    sendJson(std::move(res), Pistache::Http::Code::Not_Found, {{"error","not_found"}});
}

} // namespace HttpHelpers
