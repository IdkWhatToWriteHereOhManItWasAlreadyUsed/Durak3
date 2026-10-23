#include "realtimeGateway_RealtimeController.h"

#include <drogon/HttpAppFramework.h>
#include <trantor/net/InetAddress.h>

#include <string>
#include <vector>

using namespace drogon;
using namespace drogon::nosql;

namespace realtimeGateway {

SessionManager& Sessions() {
    static SessionManager manager;
    return manager;
}

static Json::FastWriter kFastWriter;

static std::string str(const Json::Value& value, const char* fallback = "") {
    return value.isString() ? value.asString()
           : value.isInt64() ? std::to_string(value.asInt64())
                             : fallback;
}

static Json::Value NewError(const std::string& code, const std::string& message) {
    Json::Value payload(Json::objectValue);
    payload["code"] = code;
    payload["message"] = message;
    return payload;
}

RealtimeController::RealtimeController() {
    const auto& custom = app().getCustomConfig();
    m_authClient = HttpClient::newHttpClient(custom["auth"].asString());
    m_lobbyClient = HttpClient::newHttpClient(custom["lobby"].asString());
    m_matchmakingClient = HttpClient::newHttpClient(custom["matchmaking"].asString());
    m_gameClient = HttpClient::newHttpClient(custom["game"].asString());
    m_redisClient = RedisClient::newRedisClient(
        trantor::InetAddress(custom["redis_host"].asString(),
                             static_cast<uint16_t>(custom["redis_port"].asUInt())),
        1,
        custom["redis_password"].asString(),
        custom["redis_db"].asUInt());

    m_subscriber = m_redisClient->newSubscriber();
    for (const std::string channel :
         {"invite.created", "invite.accepted", "invite.declined",
          "game.started", "game.move", "game.finished",
          "presence.online", "presence.offline", "auth.logged_out"}) {
        m_subscriber->subscribe(channel, [this](const std::string& ch,
                                                const std::string& message) {
            DispatchEvent(ch, message);
        });
    }
}

void RealtimeController::handleNewConnection(const HttpRequestPtr& req,
                                             const WebSocketConnectionPtr& wsPtr) {
    auto session = std::make_shared<WSSession>();
    session->playerId = req->getAttributes()->get<int64_t>("userId");
    session->token = req->getAttributes()->get<std::string>("token");
    session->sessionId = std::to_string(reinterpret_cast<uint64_t>(wsPtr.get())) +
                         "-" + std::to_string(trantor::Date::date().secondsSinceEpoch());
    session->connection = wsPtr;
    wsPtr->setContext(session);
    Sessions().Add(session);

    Json::Value body(Json::objectValue);
    body["type"] = "hello";
    Json::Value payload(Json::objectValue);
    payload["sessionId"] = session->sessionId;
    body["payload"] = payload;
    wsPtr->send(kFastWriter.write(body));
}

void RealtimeController::handleConnectionClosed(const WebSocketConnectionPtr& wsPtr) {
    auto session = wsPtr->getContext<WSSession>();
    if (session) {
        Sessions().Remove(session->sessionId);
    }
}

void RealtimeController::handleNewMessage(const WebSocketConnectionPtr& wsPtr,
                                          std::string&& message,
                                          const WebSocketMessageType& type) {
    auto session = wsPtr->getContext<WSSession>();
    if (!session) return;

    if (type == WebSocketMessageType::Close) {
        return;
    }
    if (type != WebSocketMessageType::Text && type != WebSocketMessageType::Binary) {
        return;
    }

    Json::Reader reader;
    Json::Value command;
    if (!reader.parse(message, command) || !command.isObject() ||
        !command.isMember("type") || !command["type"].isString()) {
        Json::Value out(Json::objectValue);
        out["type"] = "error";
        out["payload"] = NewError("BAD_REQUEST", "message is not valid command");
        wsPtr->send(kFastWriter.write(out));
        return;
    }
    ProcessCommand(session, command);
}

void RealtimeController::ProcessCommand(const WSSession::Ptr& session,
                                        Json::Value command) {
    const std::string type = command["type"].asString();
    const Json::Value payload = command.get("payload", Json::Value(Json::objectValue));

    if (type == "ping") {
        Json::Value out(Json::objectValue);
        out["type"] = "pong";
        Json::Value body(Json::objectValue);
        if (payload.isMember("t")) body["t"] = payload["t"];
        if (payload.isMember("requestId")) body["requestId"] = payload["requestId"];
        out["payload"] = body;
        session->connection->send(kFastWriter.write(out));
        return;
    }
    if (type == "presence.heartbeat") {
        OnPresenceHeartbeat(session, payload);
        return;
    }
    if (type == "invite.create") {
        OnInviteCreate(session, payload);
        return;
    }
    if (type == "invite.accept" || type == "invite.decline") {
        OnInviteAccept(session, payload, type == "invite.accept" ? "accept" : "decline");
        return;
    }
    if (type == "game.move") {
        OnGameMove(session, payload);
        return;
    }
    Json::Value out(Json::objectValue);
    out["type"] = "error";
    out["payload"] = NewError("UNSUPPORTED", "unknown command " + type);
    session->connection->send(kFastWriter.write(out));
}

void RealtimeController::OnPresenceHeartbeat(const WSSession::Ptr& session,
                                             Json::Value payload) {
    if (!payload.isMember("sessionId")) {
        Json::Value out(Json::objectValue);
        out["type"] = "error";
        out["payload"] = NewError("NO_SESSION",
                                  "gateway stub knows no lobby session; pass payload.sessionId");
        session->connection->send(kFastWriter.write(out));
        return;
    }
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Post);
    downstream->setPath("/presence/" + payload["sessionId"].asString() + "/heartbeat");
    auto client = m_lobbyClient;
    client->sendRequest(
        downstream,
        [session](ReqResult result, const HttpResponsePtr& response) {
            if (result == ReqResult::Ok && response &&
                response->getStatusCode() == k200OK) {
                return;
            }
            Json::Value out(Json::objectValue);
            out["type"] = "error";
            out["payload"] = response ? NewError("LOBBY_SERVICE",
                                                 std::string(response->body()))
                                      : NewError("SERVICE_UNAVAILABLE",
                                                 "lobby-service is unreachable");
            session->connection->send(kFastWriter.write(out));
        },
        5);
}

void RealtimeController::OnInviteCreate(const WSSession::Ptr& session,
                                        Json::Value payload) {
    Json::Value body(Json::objectValue);
    body["fromId"] = Json::Int64(session->playerId);
    body["toId"] = payload["toId"];
    body["ttlSeconds"] = payload.get("ttlSeconds", 30);
    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites");
    auto client = m_matchmakingClient;
    client->sendRequest(
        downstream,
        [session](ReqResult result, const HttpResponsePtr& response) {
            if (result == ReqResult::Ok && response &&
                response->getStatusCode() == k201Created) {
                return;
            }
            Json::Value out(Json::objectValue);
            out["type"] = "error";
            out["payload"] = response ? NewError("MM_SERVICE", std::string(response->body()))
                                      : NewError("SERVICE_UNAVAILABLE",
                                                 "matchmaking-service is unreachable");
            session->connection->send(kFastWriter.write(out));
        },
        5);
}

void RealtimeController::OnInviteAccept(const WSSession::Ptr& session,
                                        Json::Value payload,
                                        const std::string& action) {
    if (!payload.isMember("inviteId")) {
        Json::Value out(Json::objectValue);
        out["type"] = "error";
        out["payload"] = NewError("BAD_REQUEST", "inviteId required");
        session->connection->send(kFastWriter.write(out));
        return;
    }
    Json::Value body(Json::objectValue);
    body["id"] = Json::Int64(session->playerId);
    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites/" + payload["inviteId"].asString() + "/" + action);
    auto client = m_matchmakingClient;
    client->sendRequest(
        downstream,
        [session](ReqResult result, const HttpResponsePtr& response) {
            if (result == ReqResult::Ok && response &&
                response->getStatusCode() < k400BadRequest) {
                return;
            }
            Json::Value out(Json::objectValue);
            out["type"] = "error";
            out["payload"] = response ? NewError("MM_SERVICE", std::string(response->body()))
                                      : NewError("SERVICE_UNAVAILABLE",
                                                 "matchmaking-service is unreachable");
            session->connection->send(kFastWriter.write(out));
        },
        5);
}

void RealtimeController::OnGameMove(const WSSession::Ptr& session,
                                    Json::Value payload) {
    std::string gameId;
    if (payload.isMember("gameId")) {
        gameId = str(payload["gameId"]);
    } else {
        gameId = Sessions().LastGameOf(session->playerId);
    }
    if (gameId.empty()) {
        Json::Value out(Json::objectValue);
        out["type"] = "error";
        out["payload"] = NewError("GAME_NOT_FOUND", "no active game for player");
        session->connection->send(kFastWriter.write(out));
        return;
    }

    Json::Value body(Json::objectValue);
    body["id"] = Json::Int64(session->playerId);
    body["type"] = payload["type"];
    if (payload.isMember("card")) body["card"] = payload["card"];
    if (payload.isMember("stack")) body["stack"] = payload["stack"];

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/games/" + gameId + "/moves");
    auto client = m_gameClient;
    client->sendRequest(
        downstream,
        [this, session, gameId](ReqResult result, const HttpResponsePtr& response) {
            if (result != ReqResult::Ok || !response) {
                Json::Value out(Json::objectValue);
                out["type"] = "error";
                out["payload"] = NewError("SERVICE_UNAVAILABLE",
                                          "game-service is unreachable");
                session->connection->send(kFastWriter.write(out));
                return;
            }
            if (response->getStatusCode() == k200OK) {
                auto json = response->getJsonObject();
                Json::Value out(Json::objectValue);
                out["type"] = "game.state";
                out["payload"] = json ? *json : Json::Value(Json::objectValue);
                session->connection->send(kFastWriter.write(out));
                for (int64_t opponent : Sessions().GamePlayers(gameId)) {
                    if (opponent == session->playerId) continue;
                    if (Sessions().SessionsOfPlayer(opponent).empty()) continue;
                    SendState(gameId, opponent);
                }
                return;
            }
            Json::Value out(Json::objectValue);
            out["type"] = "error";
            out["payload"] = NewError("GAME_SERVICE", std::string(response->body()));
            session->connection->send(kFastWriter.write(out));
        },
        5);
}

void RealtimeController::SendState(const std::string& gameId, int64_t playerId) {
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/games/" + gameId + "/state");
    downstream->setParameter("id", std::to_string(playerId));
    auto client = m_gameClient;
    client->sendRequest(
        downstream,
        [playerId](ReqResult result, const HttpResponsePtr& response) {
            if (result != ReqResult::Ok || !response ||
                response->getStatusCode() != k200OK) {
                return;
            }
            auto json = response->getJsonObject();
            if (!json) return;
            Json::Value out(Json::objectValue);
            out["type"] = "game.state";
            out["payload"] = *json;
            std::string text = kFastWriter.write(out);
            for (const auto& session : Sessions().SessionsOfPlayer(playerId)) {
                session->connection->send(text);
            }
        },
        5);
}

void RealtimeController::DispatchEvent(const std::string& channel,
                                       const std::string& message) {
    Json::Reader reader;
    Json::Value payload;
    if (!reader.parse(message, payload)) return;

    if (channel == "invite.created" || channel == "invite.accepted" ||
        channel == "invite.declined") {
        OnInviteEvent(channel, payload);
        return;
    }
    if (channel == "game.started") {
        OnGameStarted(payload);
        return;
    }
    if (channel == "game.move" || channel == "game.finished") {
        OnGameEvent(channel, payload);
        return;
    }
    if (channel == "presence.online" || channel == "presence.offline") {
        OnPresenceEvent(channel, payload);
        return;
    }
    if (channel == "auth.logged_out") {
        OnAuthLoggedOut(payload);
        return;
    }
}

void RealtimeController::OnInviteEvent(const std::string& type, Json::Value payload) {
    std::vector<int64_t> targets;
    if (payload.isMember("fromId")) targets.push_back(payload["fromId"].asInt64());
    if (payload.isMember("toId")) targets.push_back(payload["toId"].asInt64());
    for (int64_t id : targets) {
        for (const auto& session : Sessions().SessionsOfPlayer(id)) {
            Json::Value out(Json::objectValue);
            out["type"] = type;
            out["payload"] = payload;
            session->connection->send(kFastWriter.write(out));
        }
    }
}

void RealtimeController::OnGameStarted(Json::Value payload) {
    const std::string gameId = str(payload["gameId"]);
    std::vector<int64_t> players;
    if (payload.isMember("players") && payload["players"].isArray()) {
        for (const auto& player : payload["players"]) {
            if (player.isMember("id")) players.push_back(player["id"].asInt64());
        }
    }
    Sessions().SetGamePlayers(gameId, players);
    for (int64_t id : players) {
        Sessions().RememberGame(id, gameId);
        Json::Value out(Json::objectValue);
        out["type"] = "game.started";
        out["payload"] = payload;
        for (const auto& session : Sessions().SessionsOfPlayer(id)) {
            session->connection->send(kFastWriter.write(out));
        }
    }
    for (int64_t id : players) {
        if (!Sessions().SessionsOfPlayer(id).empty()) {
            SendState(gameId, id);
        }
    }
}

void RealtimeController::OnGameEvent(const std::string& type, Json::Value payload) {
    const std::string gameId = str(payload["gameId"]);
    auto targets = Sessions().GamePlayers(gameId);
    if (targets.empty()) targets = Sessions().PlayerIds();
    for (int64_t id : targets) {
        for (const auto& session : Sessions().SessionsOfPlayer(id)) {
            Json::Value out(Json::objectValue);
            out["type"] = type;
            out["payload"] = payload;
            session->connection->send(kFastWriter.write(out));
        }
    }
}

void RealtimeController::OnPresenceEvent(const std::string& type,
                                         Json::Value payload) {
    for (int64_t id : Sessions().PlayerIds()) {
        for (const auto& session : Sessions().SessionsOfPlayer(id)) {
            Json::Value out(Json::objectValue);
            out["type"] = type;
            out["payload"] = payload;
            session->connection->send(kFastWriter.write(out));
        }
    }
}

void RealtimeController::OnAuthLoggedOut(Json::Value payload) {
    if (!payload.isMember("token")) return;
    for (const auto& session : Sessions().SessionsByToken(payload["token"].asString())) {
        session->connection->shutdown(static_cast<CloseCode>(4401), "session expired");
        Sessions().Remove(session->sessionId);
    }
}

}  // namespace realtimeGateway
