#include "controllers/chat_controller.h"
#include <pistache/http.h>
#include "http/routes.h"
#include "external/json.hpp"
#include "util/time.h"

using namespace Pistache;
using HttpHelpers::json;

void ChatController::registerRoutes(Rest::Router& r) {
    Rest::Routes::Post(r, "/v1/tables/:tableId/chat",
        Rest::Routes::bind(&ChatController::chat, this));
}

void ChatController::chat(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId", "");
    std::string token    = (*j).value("token", "");
    std::string message  = (*j).value("message", "");

    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }
    if (message.empty()) { HttpHelpers::badRequest(std::move(res), "message_required"); return; }

    HttpHelpers::sendJson(std::move(res), Http::Code::Accepted,
        {{"tableId", tableId}, {"playerId", playerId}, {"message", message}, {"time", nowIso()}});
}
