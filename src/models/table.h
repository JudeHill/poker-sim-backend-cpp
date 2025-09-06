#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include "external/json.hpp"

struct Table {
    std::string id;
    std::string name;
    int maxPlayers{9};
    int smallBlind{1};
    int bigBlind{2};
    std::vector<std::string> players;
    std::unordered_map<std::string,int> seats;
    int stateVersion{0};
    nlohmann::json state;
};
