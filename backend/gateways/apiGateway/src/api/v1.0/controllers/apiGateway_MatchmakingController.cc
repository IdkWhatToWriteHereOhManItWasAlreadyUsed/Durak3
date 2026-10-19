#include "apiGateway_MatchmakingController.h"

#include <json/json.h>

using namespace apiGateway;

namespace {
void appendQuery(std::string&       query,
                 const std::string& key,
                 const std::string& value) {
    query += (query.empty() ? "?" : "&") + key + "=" +
             drogon::utils::urlEncodeComponent(value);
}
}  // namespace

MatchmakingController::MatchmakingController() {
    client_ = HttpClient::newHttpClient(
        app().getCustomConfig()["matchmaking"].asString());
}

// Проксирует matchmaking-service POST /invites: приглашает игрока (`GO?`).
// Тело клиента (toId, ttlSeconds) пересылается как есть, fromId
// подставляется из middleware (клиентское значение игнорируется).
void MatchmakingController::CreateInvite(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
    const auto  json = req->getJsonObject();
    Json::Value body = json ? *json : Json::Value(Json::objectValue);
    body["fromId"]   = req->getAttributes()->get<int64_t>("userId");
    auto downstream  = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites");
    Forward(downstream, callback);
}

// Проксирует matchmaking-service GET /invites: список приглашений игрока.
// id (чьи приглашения) подставляется из middleware.
void MatchmakingController::ListInvites(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string query;
    appendQuery(query, "id",
                std::to_string(req->getAttributes()->get<int64_t>("userId")));

    const auto direction = req->getParameter("direction");
    if (!direction.empty()) {
        appendQuery(query, "direction", direction);
    }

    const auto status = req->getParameter("status");
    if (!status.empty()) {
        appendQuery(query, "status", status);
    }

    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/invites" + query);
    Forward(downstream, callback);
}

// Проксирует matchmaking-service GET /invites/{inviteId}: получить приглашение.
void MatchmakingController::GetInvite(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback,
    std::string                                   inviteId) {
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/invites/" + inviteId);
    Forward(downstream, callback);
}

// Проксирует matchmaking-service POST /invites/{inviteId}/accept: принять
// приглашение (`GO!`, создает матч и партию). Тело формируется гейтвеем:
// id принимающего подставляется из middleware.
void MatchmakingController::AcceptInvite(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    std::string                                   inviteId) {
    Json::Value body(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites/" + inviteId + "/accept");
    Forward(downstream, callback);
}

// Проксирует matchmaking-service POST /invites/{inviteId}/decline:
// отклонить приглашение (`NO!`). id отклоняющего подставляется из middleware.
void MatchmakingController::DeclineInvite(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    std::string                                   inviteId) {
    Json::Value body(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites/" + inviteId + "/decline");
    Forward(downstream, callback);
}

// Проксирует matchmaking-service POST /invites/{inviteId}/cancel:
// отменить своё приглашение. id отменяющего подставляется из middleware.
void MatchmakingController::CancelInvite(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback,
    std::string                                   inviteId) {
    Json::Value body(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/invites/" + inviteId + "/cancel");
    Forward(downstream, callback);
}

// Проксирует matchmaking-service GET /matches: список матчей игрока.
// id (чьи матчи) подставляется из middleware.
void MatchmakingController::ListMatches(
    const HttpRequestPtr&                         req,
    std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string query;
    appendQuery(query, "id",
                std::to_string(req->getAttributes()->get<int64_t>("userId")));

    const auto status = req->getParameter("status");
    if (!status.empty()) {
        appendQuery(query, "status", status);
    }

    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/matches" + query);
    Forward(downstream, callback);
}

// Проксирует matchmaking-service GET /matches/{matchId}: получить матч.
void MatchmakingController::GetMatch(
    const HttpRequestPtr&,
    std::function<void(const HttpResponsePtr&)>&& callback,
    std::string                                   matchId) {
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/matches/" + matchId);
    Forward(downstream, callback);
}

// Отправляет downstream-запрос в matchmaking-service и пробрасывает ответ
// клиенту.
void MatchmakingController::Forward(
    const HttpRequestPtr&                              downstream,
    const std::function<void(const HttpResponsePtr&)>& callback) {
    auto client = client_;
    client->sendRequest(
        downstream,
        [callback](ReqResult result, const HttpResponsePtr& response) {
            if (result == ReqResult::Ok && response) {
                callback(response);
                return;
            }
            Unreachable(callback);
        },
        5);
}

void MatchmakingController::Unreachable(
    const std::function<void(const HttpResponsePtr&)>& callback) {
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k503ServiceUnavailable);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(
        R"({"code":"SERVICE_UNAVAILABLE","message":"matchmaking-service is unreachable"})");
    callback(response);
}
