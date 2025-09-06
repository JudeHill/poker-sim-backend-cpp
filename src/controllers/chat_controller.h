#pragma once
#include <pistache/router.h>
#include "store/store.h"

class ChatController {
public:
    explicit ChatController(Store& store) : store_(store) {}
    void registerRoutes(Pistache::Rest::Router& r);

private:
    void chat(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    Store& store_;
};
