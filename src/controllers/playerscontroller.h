#pragma once
#include <pistache/router.h>
#include "store/store.h"

class PlayersController {
public:
    explicit PlayersController(Store& store) : store_(store) {}
    void registerRoutes(Pistache::Rest::Router& r);
private:
    void registerPlayer(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void createSession(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    Store& store_;
};
