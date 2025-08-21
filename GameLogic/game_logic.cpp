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