#include "http/server.h"
#include <thread>
#include <iostream>

int main(int argc, char* argv[]) {
    uint16_t port = 9080;
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port argument, using default 9080\n";
        }
    }

    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(port));

    PokerApiServer server(addr);
    server.init(std::thread::hardware_concurrency());

    std::cout << "Poker API server listening on port " << port << "...\n";

    server.start();
    server.shutdown();

    return 0;
}
