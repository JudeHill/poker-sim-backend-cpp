#pragma once
#include <pistache/router.h>
#include "store/store.h"

class TablesController {
public:
    explicit TablesController(Store& store) : store_(store) {}
    void registerRoutes(Pistache::Rest::Router& r);

private:
    void listTables(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void createTable(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void getTable(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);

    void joinTable(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void leaveTable(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);
    void heartbeat(const Pistache::Rest::Request& req, Pistache::Http::ResponseWriter res);

    static bool seatTaken(const Table& t, int s);
    static int firstFreeSeat(const Table& t);

    Store& store_;
};
