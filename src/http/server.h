#pragma once
#include <pistache/endpoint.h>
#include <pistache/router.h>
#include <pistache/net.h>
#include "store/store.h"
#include "controllers/players_controller.h"
#include "controllers/tables_controller.h"
#include "controllers/state_controller.h"
#include "controllers/chat_controller.h"

class PokerApiServer {
public:
    explicit PokerApiServer(Pistache::Address addr);
    void init(size_t threads);
    void start();
    void shutdown();
private:
    void setupRoutes();
    std::shared_ptr<Pistache::Http::Endpoint> httpEndpoint_;
    Pistache::Rest::Router router_;
    Store store_;
};
