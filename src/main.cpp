#include "http/server.h"

#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <thread>

#ifdef _WIN32
  #include <windows.h>
#endif

// Global stop flag flipped by signal/console handlers
static std::atomic<bool> g_stop{false};

#ifdef _WIN32
// Windows console control handler (Ctrl+C, close, logoff, shutdown)
BOOL WINAPI ConsoleCtrlHandler(DWORD ctrlType) {
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            g_stop.store(true, std::memory_order_relaxed);
            return TRUE; // handled
        default:
            return FALSE;
    }
}
#endif

// POSIX-style signal handler (also works on Windows for SIGINT/SIGTERM)
void signalHandler(int) {
    g_stop.store(true, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    uint16_t port = 9080;
    if (argc > 1) {
        try {
            port = static_cast<uint16_t>(std::stoi(argv[1]));
        } catch (...) {
            std::cerr << "Invalid port argument, using default 9080\n";
        }
    }

    // Install handlers
#ifdef _WIN32
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);
#endif
    std::signal(SIGINT,  signalHandler);  // Ctrl+C
#ifdef SIGTERM
    std::signal(SIGTERM, signalHandler);  // kill <pid>
#endif

    Pistache::Address addr(Pistache::Ipv4::any(), Pistache::Port(port));
    PokerApiServer server(addr);

    // Init without InstallSignalHandler flag
    server.init(std::thread::hardware_concurrency());

    std::cout << "Poker API server listening on port " << port << " (press Ctrl+C to stop)...\n";

    // Run server in background so main thread can watch for signals
    std::thread srv([&]{
        server.start();   // blocking
    });

    // Wait until a signal flips g_stop
    while (!g_stop.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nShutting down...\n";
    server.shutdown();
    if (srv.joinable()) srv.join();

    std::cout << "Bye.\n";
    return 0;
}
