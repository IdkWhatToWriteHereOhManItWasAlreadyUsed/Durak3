#include "apiGateway_LobbyController.h"

#include <json/json.h>

using namespace apiGateway;

namespace {
void appendQuery(std::string& query, const std::string& key, const std::string& value) {
    query += (query.empty() ? "?" : "&") + key + "=" + drogon::utils::urlEncodeComponent(value);
}
}  // namespace

LobbyController::LobbyController() {
    client_ = HttpClient::newHttpClient(app().getCustomConfig()["lobby"].asString());
}

// Проксирует lobby-service POST /presence: открывает присутствие в лобби.
// Тело клиента пересылается как есть, id подставляется из middleware.
void LobbyController::OpenPresence(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    const auto json = req->getJsonObject();
    Json::Value body = json ? *json : Json::Value(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");
    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/presence");
    Forward(downstream, callback);
}

// Проксирует lobby-service DELETE /presence/{sessionId}: закрывает присутствие.
// sessionId клиент передаёт в теле (он получил его при открытии присутствия).
void LobbyController::ClosePresence(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    const auto json = req->getJsonObject();
    if (!json || !json->isMember("sessionId")) {
        BadRequest(callback, "sessionId is required");
        return;
    }

    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Delete);
    downstream->setPath("/presence/" + (*json)["sessionId"].asString());
    Forward(downstream, callback);
}

// Проксирует lobby-service POST /presence/{sessionId}/heartbeat: продлевает
// присутствие.
void LobbyController::Heartbeat(const HttpRequestPtr&, std::function<void(const HttpResponsePtr&)>&& callback, std::string sessionId) {
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Post);
    downstream->setPath("/presence/" + sessionId + "/heartbeat");
    Forward(downstream, callback);
}

// Проксирует lobby-service GET /presence: список онлайн-игроков.
// id (для excludeSelf) подставляется из middleware, остальные query
// параметры пересылаются как есть.
void LobbyController::OnlinePlayers(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string query;
    const auto excludeSelf = req->getParameter("excludeSelf");
    appendQuery(query, "excludeSelf", excludeSelf.empty() ? "true" : excludeSelf);
    appendQuery(query, "id", std::to_string(req->getAttributes()->get<int64_t>("userId")));

    const auto limit = req->getParameter("limit");
    if (!limit.empty()) {
        appendQuery(query, "limit", limit);
    }

    const auto offset = req->getParameter("offset");
    if (!offset.empty()) {
        appendQuery(query, "offset", offset);
    }

    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/presence" + query);
    Forward(downstream, callback);
}

// Отправляет downstream-запрос в lobby-service и пробрасывает ответ клиенту.
void LobbyController::Forward(const HttpRequestPtr& downstream, const std::function<void(const HttpResponsePtr&)>& callback) {
    auto client = client_;
    client->sendRequest(
        downstream,
        [callback](ReqResult result, const HttpResponsePtr& response) {
            if (result == ReqResult::Ok && response) {
                // Drogon сам добавит Content-Length при отправке тела;
                // входящий от downstream-сервиса удаляем, иначе дубль.
                response->removeHeader("Content-Length");
                callback(response);
                return;
            }
            Unreachable(callback);
        },
        5);
}

void LobbyController::BadRequest(const std::function<void(const HttpResponsePtr&)>& callback, const std::string& message) {
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k400BadRequest);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(R"({"code":"BAD_REQUEST","message":")" + message + R"("})");
    callback(response);
}

void LobbyController::Unreachable(const std::function<void(const HttpResponsePtr&)>& callback) {
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k503ServiceUnavailable);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(R"({"code":"SERVICE_UNAVAILABLE","message":"lobby-service is unreachable"})");
    callback(response);
}
