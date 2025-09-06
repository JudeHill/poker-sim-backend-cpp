#pragma once
#include "player.h"
#include "table.h"
#include "external/json.hpp"

inline nlohmann::json tableSummaryJson(const Table& t) {
    return {{"tableId",t.id},{"name",t.name},{"maxPlayers",t.maxPlayers},
            {"smallBlind",t.smallBlind},{"bigBlind",t.bigBlind},
            {"players",(int)t.players.size()},{"stateVersion",t.stateVersion}};
}

inline nlohmann::json tableDetailJson(const Table& t) {
    return {{"tableId",t.id},{"name",t.name},{"maxPlayers",t.maxPlayers},
            {"smallBlind",t.smallBlind},{"bigBlind",t.bigBlind},
            {"players",t.players},{"seats",t.seats},{"stateVersion",t.stateVersion}};
}
