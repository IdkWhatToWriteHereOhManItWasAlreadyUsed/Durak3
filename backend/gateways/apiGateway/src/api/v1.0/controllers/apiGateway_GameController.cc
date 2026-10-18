#include "apiGateway_GameController.h"

#include <json/json.h>

using namespace apiGateway;

GameController::GameController() {
    client_ = HttpClient::newHttpClient(app().getCustomConfig()["game"].asString());
}

// Проксирует game-service GET /games/{gameId}: метаданные партии.
void GameController::GetGame(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/games/" + gameId);
    Forward(downstream, callback);
}

// Проксирует game-service GET /games/{gameId}/state: видимое состояние
// партии. id запрашивающего игрока подставляется из middleware.
void GameController::GetGameState(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/games/" + gameId + "/state?id=" + std::to_string(req->getAttributes()->get<int64_t>("userId")));
    Forward(downstream, callback);
}

// Проксирует game-service POST /games/{gameId}/moves: сделать ход.
// Тело клиента (type, card, stack, clientMoveId) пересылается как есть,
// id делающего ход перезаписывается значением из middleware.
void GameController::MakeMove(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    const auto json = req->getJsonObject();
    Json::Value body = json ? *json : Json::Value(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/games/" + gameId + "/moves");
    Forward(downstream, callback);
}

// Проксирует game-service GET /games/{gameId}/moves/history: история ходов.
void GameController::GetMoveHistory(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/games/" + gameId + "/moves/history");
    Forward(downstream, callback);
}

// Проксирует game-service POST /games/{gameId}/resign: сдаться.
// Тело формируется гейтвеем: id сдающегося подставляется из middleware.
void GameController::Resign(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    Json::Value body(Json::objectValue);
    body["id"] = req->getAttributes()->get<int64_t>("userId");

    auto downstream = HttpRequest::newHttpJsonRequest(body);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/games/" + gameId + "/resign");
    Forward(downstream, callback);
}

// Проксирует game-service GET /games/{gameId}/result: результат партии.
void GameController::GetGameResult(const HttpRequestPtr&, std::function<void (const HttpResponsePtr &)>&& callback, std::string gameId)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/games/" + gameId + "/result");
    Forward(downstream, callback);
}

// Отправляет downstream-запрос в game-service и пробрасывает ответ клиенту.
void GameController::Forward(const HttpRequestPtr& downstream, const std::function<void (const HttpResponsePtr &)>& callback)
{
    auto client = client_;
    client->sendRequest(downstream, [callback](ReqResult result, const HttpResponsePtr& response)
    {
        if (result == ReqResult::Ok && response)
        {
            callback(response);
            return;
        }
        Unreachable(callback);
    }, 5);
}

void GameController::Unreachable(const std::function<void (const HttpResponsePtr &)>& callback)
{
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k503ServiceUnavailable);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(R"({"code":"SERVICE_UNAVAILABLE","message":"game-service is unreachable"})");
    callback(response);
}
