#include "store/store.h"
#include <algorithm>

// ---- Player methods ----
bool Store::hasPlayer(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_);
    return players_.count(id) > 0;
}

bool Store::auth(const std::string& playerId, const std::string& token) const {
    std::lock_guard<std::mutex> lock(m_);
    auto it = players_.find(playerId);
    if (it == players_.end()) return false;
    return it->second.token == token;
}

Player Store::getPlayer(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_);
    auto it = players_.find(id);
    if (it != players_.end()) {
        return it->second; // copy
    }
    return {}; // default Player
}

void Store::upsertPlayer(const Player& p) {
    std::lock_guard<std::mutex> lock(m_);
    players_[p.id] = p;
}

// ---- Table methods ----
bool Store::getTable(const std::string& id, Table& out) const {
    std::lock_guard<std::mutex> lock(m_);
    auto it = tables_.find(id);
    if (it == tables_.end()) return false;
    out = it->second; // copy
    return true;
}

void Store::upsertTable(const Table& t) {
    std::lock_guard<std::mutex> lock(m_);
    tables_[t.id] = t;
}

std::unordered_map<std::string, Table> Store::listTables() const {
    std::lock_guard<std::mutex> lock(m_);
    return tables_; // copy
}

// ---- Session methods ----
void Store::setSession(const std::string& sessionId, const std::string& playerId) {
    std::lock_guard<std::mutex> lock(m_);
    sessions_[sessionId] = playerId;
}

bool Store::getSession(const std::string& sessionId, std::string& playerId) const {
    std::lock_guard<std::mutex> lock(m_);
    auto it = sessions_.find(sessionId);
    if (it == sessions_.end()) return false;
    playerId = it->second;
    return true;
}
