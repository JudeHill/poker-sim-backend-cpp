#include <pistache/endpoint.h>
#include <pistache/router.h>
#include <pistache/http.h>
#include <pistache/net.h>
#include "json.hpp"
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <optional>
#include <algorithm>




void health(const Rest::Request& req, Http::ResponseWriter res) {
    json out{{"status","ok"},{"time",nowIso()}};
    sendJson(req, std::move(res), Http::Code::Ok, out);
    }
    
    
    void registerPlayer(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = parseBody(req);
    if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
    std::string name = (*j).value("name", "");
    if (name.empty()) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","name_required"}}); return; }
    Player p; p.id = randId(16); p.name = name; p.token = randId(32);
    {
    std::lock_guard<std::mutex> lock(m);
    players[p.id] = p;
    }
    json out{{"playerId",p.id},{"token",p.token},{"name",p.name}};
    sendJson(req, std::move(res), Http::Code::Created, out);
    }
    
    
    void createSession(const Rest::Request& req, Http::ResponseWriter res) {
    auto j = parseBody(req);
    if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
    std::string playerId = (*j).value("playerId","");
    std::string token = (*j).value("token","");
    if (playerId.empty()||token.empty()) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","auth_required"}}); return; }
    if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
    std::string sessionId = randId(20);
    {
    std::lock_guard<std::mutex> lock(m);
    sessions[sessionId]=playerId;
    }
    json out{{"sessionId",sessionId},{"playerId",playerId}};
    sendJson(req, std::move(res), Http::Code::Created, out);
    }
    
    
    void listTables(const Rest::Request& req, Http::ResponseWriter res) {
    json arr = json::array();
    {
    std::lock_guard<std::mutex> lock(m);
    for (auto& kv : tables) {
    auto& t = kv.second;
    arr.push_back({{"tableId",t.id},{"name",t.name},{"maxPlayers",t.maxPlayers},{"smallBlind",t.smallBlind},{"bigBlind",t.bigBlind},{"players",t.players.size()},{"stateVersion",t.stateVersion}});
    }
    }