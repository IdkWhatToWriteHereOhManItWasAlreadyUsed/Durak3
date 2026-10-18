#include "apiGateway_AuthController.h"

#include <json/json.h>

using namespace apiGateway;

AuthController::AuthController() {
    client_ = HttpClient::newHttpClient(app().getCustomConfig()["auth"].asString());
}

// Проксирует auth-service POST /auth/google: обменивает authorization code
// на opaque accessToken. Тело запроса пересылается как есть.
void AuthController::GoogleLogin(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    const auto json = req->getJsonObject();
    if (!json)
    {
        BadRequest(callback, "Request body is required");
        return;
    }

    auto downstream = HttpRequest::newHttpJsonRequest(*json);
    downstream->setMethod(drogon::Post);
    downstream->setPath("/auth/google");
    Forward(req, downstream, callback);
}

// Проксирует auth-service GET /auth/me: профиль текущего пользователя.
// Заголовок Authorization пересылается как есть.
void AuthController::Me(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/auth/me");
    Forward(req, downstream, callback);
}

// Проксирует auth-service DELETE /auth/logout: удаляет сессию.
// Заголовок Authorization пересылается как есть.
void AuthController::Logout(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Delete);
    downstream->setPath("/auth/logout");
    Forward(req, downstream, callback);
}

// Проксирует auth-service GET /auth/users/{id}: публичный профиль по id.
// id из маршрута подставляется в путь downstream-запроса.
void AuthController::UserById(const HttpRequestPtr& req, std::function<void (const HttpResponsePtr &)>&& callback, uint64_t id)
{
    auto downstream = HttpRequest::newHttpRequest();
    downstream->setMethod(drogon::Get);
    downstream->setPath("/auth/users/" + std::to_string(id));
    Forward(req, downstream, callback);
}

// Отправляет downstream-запрос в auth-service и пробрасывает ответ клиенту.
// В отличие от domain-сервисов, заголовок Authorization здесь нужен
// (auth-service сам валидирует opaque-токен).
void AuthController::Forward(const HttpRequestPtr& req, const HttpRequestPtr& downstream,
                             const std::function<void (const HttpResponsePtr &)>& callback)
{
    const auto& authHeader = req->getHeader("Authorization");
    if (!authHeader.empty())
        downstream->addHeader("Authorization", authHeader);

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

void AuthController::BadRequest(const std::function<void (const HttpResponsePtr &)>& callback, const std::string& message)
{
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k400BadRequest);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(R"({"code":"BAD_REQUEST","message":")" + message + R"("})");
    callback(response);
}

void AuthController::Unreachable(const std::function<void (const HttpResponsePtr &)>& callback)
{
    auto response = HttpResponse::newHttpResponse();
    response->setStatusCode(k503ServiceUnavailable);
    response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
    response->setBody(R"({"code":"SERVICE_UNAVAILABLE","message":"auth-service is unreachable"})");
    callback(response);
}
