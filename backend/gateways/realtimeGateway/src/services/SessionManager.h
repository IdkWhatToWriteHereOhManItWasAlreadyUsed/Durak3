#pragma once

#include <drogon/WebSocketConnection.h>
#include <mutex>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace realtimeGateway {
struct WSSession {
    using Ptr = std::shared_ptr<WSSession>;

    int64_t playerId = 0;
    std::string token;
    std::string sessionId;
    drogon::WebSocketConnectionPtr connection;
};

class SessionManager {
public:
    void Add(const WSSession::Ptr& session) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_byPlayer[session->playerId].insert(session->sessionId);
        m_sessions[session->sessionId] = session;
        m_byToken[session->token].insert(session->sessionId);
    }

    void Remove(const std::string& sessionId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_sessions.find(sessionId);
        if (it == m_sessions.end()) return;
        auto& player = m_byPlayer[it->second->playerId];
        player.erase(sessionId);
        if (player.empty()) m_byPlayer.erase(it->second->playerId);
        auto& token = m_byToken[it->second->token];
        token.erase(sessionId);
        if (token.empty()) m_byToken.erase(it->second->token);
        m_sessions.erase(it);
    }

    std::vector<WSSession::Ptr> SessionsOfPlayer(int64_t playerId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<WSSession::Ptr> out;
        auto player = m_byPlayer.find(playerId);
        if (player == m_byPlayer.end()) return out;
        for (const auto& sessionId : player->second) {
            auto it = m_sessions.find(sessionId);
            if (it != m_sessions.end()) out.push_back(it->second);
        }
        return out;
    }

    std::vector<int64_t> PlayerIds() {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<int64_t> out;
        out.reserve(m_byPlayer.size());
        for (const auto& [id, sessions] : m_byPlayer) {
            out.push_back(id);
        }
        return out;
    }

    std::vector<WSSession::Ptr> SessionsByToken(const std::string& token) {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<WSSession::Ptr> out;
        auto tok = m_byToken.find(token);
        if (tok == m_byToken.end()) return out;
        for (const auto& sessionId : tok->second) {
            auto it = m_sessions.find(sessionId);
            if (it != m_sessions.end()) out.push_back(it->second);
        }
        return out;
    }

    void RememberGame(int64_t playerId, const std::string& gameId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_lastGame[playerId] = gameId;
    }

    std::string LastGameOf(int64_t playerId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_lastGame.find(playerId);
        return it == m_lastGame.end() ? std::string{} : it->second;
    }

    void SetGamePlayers(const std::string& gameId, const std::vector<int64_t>& players) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_gamePlayers[gameId] = players;
    }

    std::vector<int64_t> GamePlayers(const std::string& gameId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_gamePlayers.find(gameId);
        return it == m_gamePlayers.end() ? std::vector<int64_t>{} : it->second;
    }

private:
    std::mutex m_mutex;
    std::unordered_map<int64_t, std::unordered_set<std::string>> m_byPlayer;
    std::unordered_map<std::string, std::unordered_set<std::string>> m_byToken;
    std::unordered_map<std::string, WSSession::Ptr> m_sessions;
    std::unordered_map<int64_t, std::string> m_lastGame;
    std::unordered_map<std::string, std::vector<int64_t>> m_gamePlayers;
};

SessionManager& Sessions();
}  // namespace realtimeGateway
