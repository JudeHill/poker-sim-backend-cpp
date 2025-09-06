#include "http/server.h"
#include "http/routes.h"
using namespace Pistache;

PokerApiServer::PokerApiServer(Address addr)
 : httpEndpoint_(std::make_shared<Http::Endpoint>(addr)) {}

void PokerApiServer::init(size_t threads) {
    auto opts = Http::Endpoint::options()
        .threads(static_cast<int>(threads));
    httpEndpoint_->init(opts);
    setupRoutes();
}

void PokerApiServer::setupRoutes() {
    // CORS preflight
    Rest::Routes::Options(router_, "/:any",
        [](const Rest::Request&, Http::ResponseWriter res){ HttpHelpers::cors(res); res.send(Http::Code::Ok); return Rest::Route::Result::Ok; });
    Rest::Routes::Options(router_, "/v1/:any+",
        [](const Rest::Request&, Http::ResponseWriter res){ HttpHelpers::cors(res); res.send(Http::Code::Ok); return Rest::Route::Result::Ok; });

    // Controllers
    PlayersController players(store_); players.registerRoutes(router_);
    TablesController tables(store_); tables.registerRoutes(router_);
    StateController  state(store_);  state.registerRoutes(router_);
    ChatController   chat(store_);   chat.registerRoutes(router_);

    // Health
    Rest::Routes::Get(router_, "/health",
        [](const Rest::Request&, Http::ResponseWriter res){
            HttpHelpers::sendJson(std::move(res), Http::Code::Ok, {{"status","ok"}});
            return Rest::Route::Result::Ok;
        });
}

void PokerApiServer::start() { httpEndpoint_->setHandler(router_.handler()); httpEndpoint_->serve(); }
void PokerApiServer::shutdown() { httpEndpoint_->shutdown(); }
