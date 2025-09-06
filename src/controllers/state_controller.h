#pragma once
#include <pistache/router.h>
#include "store/store.h"

class StateController {
public:
    explicit StateController(Store& store) : store_(store) {}
    void registerRoutes(Pistache::Rest::Router& r);

private:
    void syncState(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void getStateSince(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void postEvents(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void postAction(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void forceResync(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);

    Store& store_;
};
