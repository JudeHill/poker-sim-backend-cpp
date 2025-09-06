#include "controllers/state_controller.h"
#include <pistache/http.h>
#include "http/routes.h"
#include "external/json.hpp"

using namespace Pistache;
using HttpHelpers::json;

void StateController::registerRoutes(Rest::Router& r) {
    Rest::Routes::Post(r, "/v1/tables/:tableId/state/sync", Rest::Routes::bind(&StateController::syncState, this));
    Rest::Routes::Get (r, "/v1/tables/:tableId/state",      Rest::Routes::bind(&StateController::getStateSince, this));
    Rest::Routes::Post(r, "/v1/tables/:tableId/events",     Rest::Routes::bind(&StateController::postEvents, this));
    Rest::Routes::Post(r, "/v1/tables/:tableId/action",     Rest::Routes::bind(&StateController::postAction, this));
    Rest::Routes::Post(r, "/v1/tables/:tableId/resync",     Rest::Routes::bind(&StateController::forceResync, this));
}

void StateController::syncState(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId","");
    std::string token    = (*j).value("token","");
    int version          = (*j).value("version",-1);
    json state           = (*j).value("state", json::object());

    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    Table t;
    if (!store_.getTable(tableId, t)) { HttpHelpers::notFound(std::move(res)); return; }

    if (version >= t.stateVersion) {
        t.state = state;
        t.stateVersion = version;
        store_.upsertTable(t);

        HttpHelpers::sendJson(std::move(res), Http::Code::Ok,
            {{"tableId", tableId}, {"appliedVersion", t.stateVersion}});
    } else {
        HttpHelpers::sendJson(std::move(res), Http::Code::Conflict, {{"error","stale_version"}});
    }
}

void StateController::getStateSince(const Rest::Request& req, Http::ResponseWriter res) {
    auto tableId = req.param("tableId").as<std::string>();
    int since = 0;
    try { since = std::stoi(HttpHelpers::qp(req, "since", "0")); } catch (...) { since = 0; }

    Table t;
    if (!store_.getTable(tableId, t)) { HttpHelpers::notFound(std::move(res)); return; }

    if (t.stateVersion > since) {
        HttpHelpers::sendJson(std::move(res), Http::Code::Ok,
            {{"tableId", t.id}, {"version", t.stateVersion}, {"state", t.state}});
    } else {
        HttpHelpers::sendJson(std::move(res), Http::Code::Not_Modified,
            {{"tableId", t.id}, {"version", t.stateVersion}, {"state", "unchanged"}});
    }
}

void StateController::postEvents(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId","");
    std::string token    = (*j).value("token","");
    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    json events = (*j).value("events", json::array());
    int ack = static_cast<int>(events.size());
    HttpHelpers::sendJson(std::move(res), Http::Code::Accepted,
        {{"tableId", tableId}, {"acknowledged", ack}});
}

void StateController::postAction(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId","");
    std::string token    = (*j).value("token","");
    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    json action = (*j).value("action", json::object());

    Table t;
    int appliedVersion = -1;
    if (store_.getTable(tableId, t)) appliedVersion = t.stateVersion;

    HttpHelpers::sendJson(std::move(res), Http::Code::Accepted,
        {{"tableId", tableId}, {"action", action}, {"appliedVersion", appliedVersion}});
}

void StateController::forceResync(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId","");
    std::string token    = (*j).value("token","");
    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    HttpHelpers::sendJson(std::move(res), Http::Code::Accepted,
        {{"tableId", tableId}, {"request", "resync"}});
}
