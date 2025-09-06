#pragma once
#include <unordered_map>
#include <string>
#include <mutex>
#include "models/player.h"
#include "models/table.h"

class Store {
public:
    bool hasPlayer(const std::string& id) const;
    bool auth(const std::string& playerId, const std::string& token) const;
    Player getPlayer(const std::string& id) const;
    void upsertPlayer(const Player& p);

    bool getTable(const std::string& id, Table& out) const;
    void upsertTable(const Table& t);
    std::unordered_map<std::string, Table> listTables() const;

    void setSession(const std::string& sessionId, const std::string& playerId);
    bool getSession(const std::string& sessionId, std::string& playerId) const;
private:
    mutable std::mutex m_;
    std::unordered_map<std::string, Player> players_;
    std::unordered_map<std::string, Table> tables_;
    std::unordered_map<std::string, std::string> sessions_;
};
