#include <pistache/endpoint.h>
#include <pistache/router.h>
#include <pistache/http.h>
#include <pistache/net.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <vector>
#include <mutex>
#include <random>
#include <chrono>
#include <optional>
#include <algorithm>

using json = nlohmann::json;
using namespace Pistache;

struct Player {
    std::string id;
    std::string name;
    std::string token;
};

struct Table {
    std::string id;
    std::string name;
    int maxPlayers{9};
    int smallBlind{1};
    int bigBlind{2};
    std::vector<std::string> players;
    std::unordered_map<std::string,int> seats;
    int stateVersion{0};
    json state;
};

static std::string nowIso() {
    using namespace std::chrono;
    auto tp = time_point_cast<milliseconds>(system_clock::now());
    auto t = system_clock::to_time_t(tp);
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &t);
#else
    gmtime_r(&t, &tm);
#endif
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    auto ms = duration_cast<milliseconds>(tp.time_since_epoch()).count() % 1000;
    char out[40];
    snprintf(out, sizeof(out), "%s.%03lldZ", buf, static_cast<long long>(ms));
    return out;
}

static std::string randId(size_t n=24) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    static const char* chars = "0123456789abcdef";
    std::uniform_int_distribution<int> dist(0,15);
    std::string s; s.reserve(n);
    for (size_t i=0;i<n;++i) s.push_back(chars[dist(rng)]);
    return s;
}

class PokerApiServer {
public:
    explicit PokerApiServer(Address addr) : httpEndpoint(std::make_shared<Http::Endpoint>(addr)) {}
    void init(size_t threads) {
        auto opts = Http::Endpoint::options().threads(static_cast<int>(threads)).flags(Tcp::Options::InstallSignalHandler);
        httpEndpoint->init(opts);
        setupRoutes();
    }
    void start() { httpEndpoint->setHandler(router.handler()); httpEndpoint->serve(); }
    void shutdown() { httpEndpoint->shutdown(); }
private:
    std::shared_ptr<Http::Endpoint> httpEndpoint;
    Rest::Router router;
    std::mutex m;
    std::unordered_map<std::string, Player> players;
    std::unordered_map<std::string, Table> tables;
    std::unordered_map<std::string, std::string> sessions;

    static void sendJson(const Rest::Request&, Http::ResponseWriter response, Http::Code code, const json& body) {
        auto s = body.dump();
        response.headers().add<Http::Header::ContentType>(MIME(Application, Json));
        response.send(code, s);
    }

    static std::optional<json> parseBody(const Rest::Request& req) {
        try {
            if (req.body().empty()) return json::object();
            auto j = json::parse(req.body(), nullptr, false);
            if (j.is_discarded()) return std::nullopt;
            return j;
        } catch (...) { return std::nullopt; }
    }

    static std::string qp(const Rest::Request& req, const std::string& key, const std::string& def="") {
        auto v = req.query().get(key);
        if (v) return v.get();
        return def;
    }

    bool auth(const std::string& playerId, const std::string& token) {
        auto it = players.find(playerId);
        if (it==players.end()) return false;
        return it->second.token==token;
    }

    void setupRoutes() {
        Rest::Routes::Options(router, "/",
            Rest::Routes::bind(&PokerApiServer::handleOptions, this));
        Rest::Routes::Options(router, "/:any",
            Rest::Routes::bind(&PokerApiServer::handleOptions, this));
        Rest::Routes::Options(router, "/v1/:any+",
            Rest::Routes::bind(&PokerApiServer::handleOptions, this));

        Rest::Routes::Get(router, "/health", Rest::Routes::bind(&PokerApiServer::health, this));

        Rest::Routes::Post(router, "/v1/players/register", Rest::Routes::bind(&PokerApiServer::registerPlayer, this));
        Rest::Routes::Post(router, "/v1/sessions/create", Rest::Routes::bind(&PokerApiServer::createSession, this));

        Rest::Routes::Get(router, "/v1/tables", Rest::Routes::bind(&PokerApiServer::listTables, this));
        Rest::Routes::Post(router, "/v1/tables", Rest::Routes::bind(&PokerApiServer::createTable, this));
        Rest::Routes::Get(router, "/v1/tables/:tableId", Rest::Routes::bind(&PokerApiServer::getTable, this));

        Rest::Routes::Post(router, "/v1/tables/:tableId/join", Rest::Routes::bind(&PokerApiServer::joinTable, this));
        Rest::Routes::Post(router, "/v1/tables/:tableId/leave", Rest::Routes::bind(&PokerApiServer::leaveTable, this));
        Rest::Routes::Post(router, "/v1/tables/:tableId/heartbeat", Rest::Routes::bind(&PokerApiServer::heartbeat, this));

        Rest::Routes::Post(router, "/v1/tables/:tableId/state/sync", Rest::Routes::bind(&PokerApiServer::syncState, this));
        Rest::Routes::Get(router, "/v1/tables/:tableId/state", Rest::Routes::bind(&PokerApiServer::getStateSince, this));

        Rest::Routes::Post(router, "/v1/tables/:tableId/events", Rest::Routes::bind(&PokerApiServer::postEvents, this));
        Rest::Routes::Post(router, "/v1/tables/:tableId/action", Rest::Routes::bind(&PokerApiServer::postAction, this));
        Rest::Routes::Post(router, "/v1/tables/:tableId/resync", Rest::Routes::bind(&PokerApiServer::forceResync, this));
        Rest::Routes::Post(router, "/v1/tables/:tableId/chat", Rest::Routes::bind(&PokerApiServer::chat, this));
    }

    void handleOptions(const Rest::Request& req, Http::ResponseWriter res) {
        res.headers().add<Http::Header::AccessControlAllowOrigin>("*");
        res.headers().add<Http::Header::AccessControlAllowMethods>("GET, POST, OPTIONS");
        res.headers().add<Http::Header::AccessControlAllowHeaders>("Content-Type, Authorization");
        res.send(Http::Code::Ok);
    }

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
        sendJson(req, std::move(res), Http::Code::Ok, {{"tables",arr}});
    }

    void createTable(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        std::string name = (*j).value("name","Table");
        int maxPlayers = (*j).value("maxPlayers",9);
        int sb = (*j).value("smallBlind",1);
        int bb = (*j).value("bigBlind",2);
        Table t; t.id = randId(12); t.name=name; t.maxPlayers=maxPlayers; t.smallBlind=sb; t.bigBlind=bb; t.stateVersion=0; t.state=json::object();
        {
            std::lock_guard<std::mutex> lock(m);
            tables[t.id]=t;
        }
        json out{{"tableId",t.id}};
        sendJson(req, std::move(res), Http::Code::Created, out);
    }

    void getTable(const Rest::Request& req, Http::ResponseWriter res) {
        auto tableId = req.param("tableId").as<std::string>();
        Table t;
        bool ok=false;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) { t = it->second; ok=true; }
        }
        if (!ok) { sendJson(req, std::move(res), Http::Code::Not_Found, {{"error","not_found"}}); return; }
        json out{{"tableId",t.id},{"name",t.name},{"maxPlayers",t.maxPlayers},{"smallBlind",t.smallBlind},{"bigBlind",t.bigBlind},{"players",t.players},{"seats",t.seats},{"stateVersion",t.stateVersion}};
        sendJson(req, std::move(res), Http::Code::Ok, out);
    }

    void joinTable(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        std::optional<int> seat;
        if ((*j).contains("seat")) seat = (*j)["seat"].get<int>();
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        int assignedSeat=-1;
        bool ok=false; bool full=false;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it==tables.end()) { sendJson(req, std::move(res), Http::Code::Not_Found, {{"error","not_found"}}); return; }
            auto& t = it->second;
            if (std::find(t.players.begin(), t.players.end(), playerId)==t.players.end()) {
                if ((int)t.players.size()>=t.maxPlayers) { full=true; }
                else {
                    t.players.push_back(playerId);
                    if (seat && *seat>=0 && *seat<t.maxPlayers && !seatTaken(t,*seat)) { t.seats[playerId]=*seat; assignedSeat=*seat; }
                    else { assignedSeat = firstFreeSeat(t); if (assignedSeat>=0) t.seats[playerId]=assignedSeat; }
                    ok=true;
                }
            } else { ok=true; if (t.seats.count(playerId)) assignedSeat=t.seats[playerId]; }
        }
        if (!ok) { if (full) sendJson(req, std::move(res), Http::Code::Conflict, {{"error","table_full"}}); else sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","cannot_join"}}); return; }
        json out{{"tableId",tableId},{"playerId",playerId},{"seat",assignedSeat}};
        sendJson(req, std::move(res), Http::Code::Ok, out);
    }

    void leaveTable(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        bool ok=false;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) {
                auto& t = it->second;
                auto itp = std::find(t.players.begin(), t.players.end(), playerId);
                if (itp!=t.players.end()) { t.players.erase(itp); t.seats.erase(playerId); ok=true; }
            }
        }
        if (!ok) { sendJson(req, std::move(res), Http::Code::Not_Found, {{"error","not_found"}}); return; }
        json out{{"tableId",tableId},{"playerId",playerId}};
        sendJson(req, std::move(res), Http::Code::Ok, out);
    }

    void heartbeat(const Rest::Request& req, Http::ResponseWriter res) {
        auto tableId = req.param("tableId").as<std::string>();
        int v=-1; size_t playerCount=0;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) { v=it->second.stateVersion; playerCount=it->second.players.size(); }
        }
        json out{{"time",nowIso()},{"tableId",tableId},{"stateVersion",v},{"players",playerCount}};
        sendJson(req, std::move(res), Http::Code::Ok, out);
    }

    void syncState(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        int version = (*j).value("version",-1);
        json state = (*j).value("state", json::object());
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        bool ok=false; int appliedVersion=-1;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) {
                auto& t = it->second;
                if (version>=t.stateVersion) { t.state = state; t.stateVersion = version; ok=true; appliedVersion=t.stateVersion; }
            }
        }
        if (!ok) { sendJson(req, std::move(res), Http::Code::Conflict, {{"error","stale_version"}}); return; }
        json out{{"tableId",tableId},{"appliedVersion",appliedVersion}};
        sendJson(req, std::move(res), Http::Code::Ok, out);
    }

    void getStateSince(const Rest::Request& req, Http::ResponseWriter res) {
        auto tableId = req.param("tableId").as<std::string>();
        int since = 0;
        try { since = std::stoi(qp(req, "since", "0")); } catch(...) { since=0; }
        Table t; bool ok=false;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) { t=it->second; ok=true; }
        }
        if (!ok) { sendJson(req, std::move(res), Http::Code::Not_Found, {{"error","not_found"}}); return; }
        if (t.stateVersion>since) {
            json out{{"tableId",t.id},{"version",t.stateVersion},{"state",t.state}};
            sendJson(req, std::move(res), Http::Code::Ok, out);
        } else {
            json out{{"tableId",t.id},{"version",t.stateVersion},{"state","unchanged"}};
            sendJson(req, std::move(res), Http::Code::Not_Modified, out);
        }
    }

    void postEvents(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        json events = (*j).value("events", json::array());
        int ack = (int)events.size();
        json out{{"tableId",tableId},{"acknowledged",ack}};
        sendJson(req, std::move(res), Http::Code::Accepted, out);
    }

    void postAction(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        json action = (*j).value("action", json::object());
        int appliedVersion=-1;
        {
            std::lock_guard<std::mutex> lock(m);
            auto it = tables.find(tableId);
            if (it!=tables.end()) { appliedVersion = it->second.stateVersion; }
        }
        json out{{"tableId",tableId},{"action",action},{"appliedVersion",appliedVersion}};
        sendJson(req, std::move(res), Http::Code::Accepted, out);
    }

    void forceResync(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        json out{{"tableId",tableId},{"request","resync"}};
        sendJson(req, std::move(res), Http::Code::Accepted, out);
    }

    void chat(const Rest::Request& req, Http::ResponseWriter res) {
        auto j = parseBody(req);
        if (!j) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","invalid_json"}}); return; }
        auto tableId = req.param("tableId").as<std::string>();
        std::string playerId = (*j).value("playerId","");
        std::string token = (*j).value("token","");
        std::string message = (*j).value("message","");
        if (!auth(playerId, token)) { sendJson(req, std::move(res), Http::Code::Unauthorized, {{"error","unauthorized"}}); return; }
        if (message.empty()) { sendJson(req, std::move(res), Http::Code::Bad_Request, {{"error","message_required"}}); return; }
        json out{{"tableId",tableId},{"playerId",playerId},{"message",message},{"time",nowIso()}};
        sendJson(req, std::move(res), Http::Code::Accepted, out);
    }

    static bool seatTaken(const Table& t, int s) {
        for (auto& kv : t.seats) if (kv.second==s) return true;
        return false;
    }

    static int firstFreeSeat(const Table& t) {
        for (int i=0;i<t.maxPlayers;++i) if (!seatTaken(t,i)) return i;
        return -1;
    }
};

int main(int argc, char* argv[]) {
    uint16_t port = 9080;
    if (argc>1) port = static_cast<uint16_t>(std::stoi(argv[1]));
    Address addr(Ipv4::any(), Port(port));
    PokerApiServer server(addr);
    server.init(std::thread::hardware_concurrency());
    server.start();
    server.shutdown();
    return 0;
}
