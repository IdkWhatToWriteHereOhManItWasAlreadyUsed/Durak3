#pragma once

#include <drogon/HttpClient.h>
#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <drogon/nosql/RedisClient.h>
#include <drogon/nosql/RedisSubscriber.h>

#include <services/SessionManager.h>

#include <mutex>

using namespace drogon;

namespace realtimeGateway {
class RealtimeController : public drogon::WebSocketController<RealtimeController, false> {
public:
    explicit RealtimeController();
    ~RealtimeController() override = default;

    WS_PATH_LIST_BEGIN
    WS_PATH_ADD("/ws", {drogon::Get, "realtimeGateway::WSAuthFilter"});
    WS_PATH_LIST_END

private:
    void handleNewMessage(const WebSocketConnectionPtr& wsPtr,
                          std::string&& message,
                          const WebSocketMessageType& type) override;
    void handleNewConnection(const HttpRequestPtr& req,
                             const WebSocketConnectionPtr& wsPtr) override;
    void handleConnectionClosed(const WebSocketConnectionPtr& wsPtr) override;

    void ProcessCommand(const WSSession::Ptr& session, Json::Value command);
    void OnPresenceHeartbeat(const WSSession::Ptr& session, Json::Value payload);
    void OnInviteCreate(const WSSession::Ptr& session, Json::Value payload);
    void OnInviteAccept(const WSSession::Ptr& session, Json::Value payload, const std::string& action);
    void OnGameMove(const WSSession::Ptr& session, Json::Value payload);

    void DispatchEvent(const std::string& channel, const std::string& message);
    void OnInviteEvent(const std::string& type, Json::Value payload);
    void OnGameStarted(Json::Value payload);
    void OnGameEvent(const std::string& type, Json::Value payload);
    void OnPresenceEvent(const std::string& type, Json::Value payload);
    void OnAuthLoggedOut(Json::Value payload);

    void SendState(const std::string& gameId, int64_t playerId);

    // Redis-клиент готов только после app().run() — берём и подписываемся
    // при первом входящем WS-соединении (оно возможно не раньше, чем
    // слушатель стартует, т.е. после createRedisClients).
    void EnsureSubscriber();

    HttpClientPtr m_authClient;
    HttpClientPtr m_lobbyClient;
    HttpClientPtr m_matchmakingClient;
    HttpClientPtr m_gameClient;
    nosql::RedisClientPtr m_redisClient;
    std::shared_ptr<nosql::RedisSubscriber> m_subscriber;
    std::once_flag m_subscriberOnce;
};
}  // namespace realtimeGateway
