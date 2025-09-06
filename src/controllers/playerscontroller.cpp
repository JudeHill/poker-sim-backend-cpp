#include "controllers/players_controller.h"
#include <pistache/http.h>
#include "http/routes.h"
#include "external/json.hpp"
#include "util/id.h"

using namespace Pistache;
using HttpHelpers::json;

void PlayersController::registerRoutes(Rest::Router& r) {
    Rest::Routes::Post(r, "/v1/players/register",
        Rest::Routes::bind(&PlayersController::registerPlayer, this));
    Rest::Routes::Post(r, "/v1/sessions/create",
        Rest::Routes::bind(&PlayersController::createSession, this));
}

void PlayersController::registerPlayer(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }

    std::string name = (*j).value("name", "");
    if (name.empty()) { HttpHelpers::badRequest(std::move(res), "name_required"); return; }

    Player p;
    p.id = randId(16);
    p.name = name;
    p.token = randId(32);

    store_.upsertPlayer(p);

    HttpHelpers::sendJson(std::move(res), Http::Code::Created,
        {{"playerId", p.id}, {"token", p.token}, {"name", p.name}});
}

void PlayersController::createSession(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }

    std::string playerId = (*j).value("playerId", "");
    std::string token    = (*j).value("token", "");
    if (playerId.empty() || token.empty()) {
        HttpHelpers::badRequest(std::move(res), "auth_required"); return;
    }
    if (!store_.auth(playerId, token)) {
        HttpHelpers::unauthorized(std::move(res)); return;
    }

    std::string sessionId = randId(20);
    store_.setSession(sessionId, playerId);

    HttpHelpers::sendJson(std::move(res), Http::Code::Created,
        {{"sessionId", sessionId}, {"playerId", playerId}});
}
