#include "controllers/tables_controller.h"
#include <pistache/http.h>
#include <algorithm>
#include "http/routes.h"
#include "external/json.hpp"
#include "util/id.h"
#include "util/time.h"

using namespace Pistache;
using HttpHelpers::json;

void TablesController::registerRoutes(Rest::Router& r) {
    Rest::Routes::Get (r, "/v1/tables",          Rest::Routes::bind(&TablesController::listTables, this));
    Rest::Routes::Post(r, "/v1/tables",          Rest::Routes::bind(&TablesController::createTable, this));
    Rest::Routes::Get (r, "/v1/tables/:tableId", Rest::Routes::bind(&TablesController::getTable, this));

    Rest::Routes::Post(r, "/v1/tables/:tableId/join",      Rest::Routes::bind(&TablesController::joinTable, this));
    Rest::Routes::Post(r, "/v1/tables/:tableId/leave",     Rest::Routes::bind(&TablesController::leaveTable, this));
    Rest::Routes::Post(r, "/v1/tables/:tableId/heartbeat", Rest::Routes::bind(&TablesController::heartbeat, this));
}

void TablesController::listTables(const Rest::Request&, Http::ResponseWriter res) {
    auto all = store_.listTables();
    json arr = json::array();
    for (auto& kv : all) {
        const auto& t = kv.second;
        arr.push_back({
            {"tableId", t.id},
            {"name", t.name},
            {"maxPlayers", t.maxPlayers},
            {"smallBlind", t.smallBlind},
            {"bigBlind", t.bigBlind},
            {"players", static_cast<int>(t.players.size())},
            {"stateVersion", t.stateVersion}
        });
    }
    HttpHelpers::sendJson(std::move(res), Http::Code::Ok, {{"tables", arr}});
}

void TablesController::createTable(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }

    Table t;
    t.id = randId(12);
    t.name = (*j).value("name", std::string("Table"));
    t.maxPlayers = (*j).value("maxPlayers", 9);
    t.smallBlind = (*j).value("smallBlind", 1);
    t.bigBlind   = (*j).value("bigBlind", 2);
    t.stateVersion = 0;
    t.state = json::object();

    store_.upsertTable(t);
    HttpHelpers::sendJson(std::move(res), Http::Code::Created, {{"tableId", t.id}});
}

void TablesController::getTable(const Rest::Request& req, Http::ResponseWriter res) {
    auto tableId = req.param("tableId").as<std::string>();
    Table t;
    if (!store_.getTable(tableId, t)) {
        HttpHelpers::notFound(std::move(res)); return;
    }
    HttpHelpers::sendJson(std::move(res), Http::Code::Ok, {
        {"tableId", t.id},
        {"name", t.name},
        {"maxPlayers", t.maxPlayers},
        {"smallBlind", t.smallBlind},
        {"bigBlind", t.bigBlind},
        {"players", t.players},
        {"seats", t.seats},
        {"stateVersion", t.stateVersion}
    });
}

void TablesController::joinTable(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId", "");
    std::string token    = (*j).value("token", "");
    std::optional<int> seat;
    if (j->contains("seat")) seat = (*j)["seat"].get<int>();

    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    Table t;
    if (!store_.getTable(tableId, t)) { HttpHelpers::notFound(std::move(res)); return; }

    int assignedSeat = -1;
    bool full = false;
    if (std::find(t.players.begin(), t.players.end(), playerId) == t.players.end()) {
        if (static_cast<int>(t.players.size()) >= t.maxPlayers) {
            full = true;
        } else {
            t.players.push_back(playerId);
            if (seat && *seat >= 0 && *seat < t.maxPlayers && !seatTaken(t, *seat)) {
                t.seats[playerId] = *seat;
                assignedSeat = *seat;
            } else {
                assignedSeat = firstFreeSeat(t);
                if (assignedSeat >= 0) t.seats[playerId] = assignedSeat;
            }
        }
    } else {
        if (t.seats.count(playerId)) assignedSeat = t.seats[playerId];
    }

    if (full || assignedSeat < 0) {
        HttpHelpers::sendJson(std::move(res),
            full ? Http::Code::Conflict : Http::Code::Bad_Request,
            {{"error", full ? "table_full" : "cannot_join"}});
        return;
    }

    store_.upsertTable(t);
    HttpHelpers::sendJson(std::move(res), Http::Code::Ok,
        {{"tableId", tableId}, {"playerId", playerId}, {"seat", assignedSeat}});
}

void TablesController::leaveTable(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = HttpHelpers::parseBody(req);
    if (!j) { HttpHelpers::badRequest(std::move(res), "invalid_json"); return; }
    auto tableId = req.param("tableId").as<std::string>();

    std::string playerId = (*j).value("playerId", "");
    std::string token    = (*j).value("token", "");
    if (!store_.auth(playerId, token)) { HttpHelpers::unauthorized(std::move(res)); return; }

    Table t;
    if (!store_.getTable(tableId, t)) { HttpHelpers::notFound(std::move(res)); return; }

    bool changed = false;
    auto itp = std::find(t.players.begin(), t.players.end(), playerId);
    if (itp != t.players.end()) {
        t.players.erase(itp);
        t.seats.erase(playerId);
        changed = true;
    }

    if (!changed) { HttpHelpers::notFound(std::move(res)); return; }

    store_.upsertTable(t);
    HttpHelpers::sendJson(std::move(res), Http::Code::Ok,
        {{"tableId", tableId}, {"playerId", playerId}});
}

void TablesController::heartbeat(const Rest::Request& req, Http::ResponseWriter res) {
    auto tableId = req.param("tableId").as<std::string>();
    Table t;
    int v = -1;
    size_t playerCount = 0;
    if (store_.getTable(tableId, t)) {
        v = t.stateVersion;
        playerCount = t.players.size();
    }
    HttpHelpers::sendJson(std::move(res), Http::Code::Ok,
        {{"time", nowIso()}, {"tableId", tableId}, {"stateVersion", v}, {"players", playerCount}});
}

bool TablesController::seatTaken(const Table& t, int s) {
    for (auto& kv : t.seats) if (kv.second == s) return true;
    return false;
}
int TablesController::firstFreeSeat(const Table& t) {
    for (int i = 0; i < t.maxPlayers; ++i) if (!seatTaken(t, i)) return i;
    return -1;
}
