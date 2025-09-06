#pragma once
#include <memory>
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

    // Initialize server with thread count, wire routes.
    void init(std::size_t threads);

    // Start serving (blocking call); call shutdown() from another thread to stop.
    void start();

    // Gracefully stop the HTTP endpoint.
    void shutdown();

private:
    void setupRoutes();

    std::shared_ptr<Pistache::Http::Endpoint> httpEndpoint_;
    Pistache::Rest::Router router_;

    // Shared in-memory state for controllers.
    Store store_;
};
