#include "apiGateway_ReplayController.h"

using namespace apiGateway;

namespace {
void appendQuery(std::string& query, const std::string& key, const std::string& value) {
    query += (query.empty() ? "?" : "&") + key + "=" + drogon::utils::urlEncodeComponent(value);
}
}  // namespace

ReplayController::ReplayController() {
    client_ = HttpClient::newHttpClient(app().getCustomConfig()["replay"].asString());
}

// Проксирует replay-service GET /replays: список повторов.
// Query-параметры (id, limit, offset) пересылаются как есть.
void ReplayController::ListReplays(const HttpRequestPtr&                         req,
                                   std::function<void(const HttpResponsePtr&)>&& callback) {
    std::string query;
    const auto  id = req->getParameter("id");
    if (!id.empty()) {
        appendQuery(query, "id", id);
    }

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
    downstream->setPath("/replays" + query);
    Forward(downstream, callback);
}

// Проксирует replay-service GET /replays/{replayId}: получить повтор.
void ReplayController::GetReplay(const HttpRequestPtr&,
                                 std::function<void(const HttpResponsePtr&)>&& callback,
                                 std::string                                   replayId) {
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/replays/" + replayId);
    Forward(downstream, callback);
}

// Проксирует replay-service GET /replays/{replayId}/frames: кадры повтора.
// Query-параметры (from, to) пересылаются как есть.
void ReplayController::GetReplayFrames(const HttpRequestPtr&                         req,
                                       std::function<void(const HttpResponsePtr&)>&& callback,
                                       std::string                                   replayId) {
    std::string query;
    const auto  from = req->getParameter("from");
    if (!from.empty()) {
        appendQuery(query, "from", from);
    }

    const auto to = req->getParameter("to");
    if (!to.empty()) {
        appendQuery(query, "to", to);
    }

    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/replays/" + replayId + "/frames" + query);
    Forward(downstream, callback);
}

// Отправляет downstream-запрос в replay-service и пробрасывает ответ клиенту.
void ReplayController::Forward(const HttpRequestPtr&                              downstream,
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

void ReplayController::Unreachable(const std::function<void(const HttpResponsePtr&)>& callback) {
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k503ServiceUnavailable);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(
        R"({"code":"SERVICE_UNAVAILABLE","message":"replay-service is unreachable"})");
    callback(response);
}
