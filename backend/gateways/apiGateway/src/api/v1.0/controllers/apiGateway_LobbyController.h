#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>
#include <json/json.h>

using namespace drogon;

namespace apiGateway {
class LobbyController : public drogon::HttpController<LobbyController, false> {
    HttpClientPtr client_ = nullptr;
public:
    explicit LobbyController();
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(LobbyController::OpenPresence,
                  "/api/lobby/presence",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(LobbyController::ClosePresence,
                  "/api/lobby/presence",
                  drogon::Delete,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(LobbyController::Heartbeat,
                  "/api/lobby/presence/{sessionId}/heartbeat",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(LobbyController::OnlinePlayers,
                  "/api/lobby/players",
                  drogon::Get,
                  "apiGateway::AuthCheckMiddleware");
    METHOD_LIST_END
private:
    void OpenPresence(const HttpRequestPtr&                         req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void ClosePresence(const HttpRequestPtr&                         req,
                       std::function<void(const HttpResponsePtr&)>&& callback);
    void Heartbeat(const HttpRequestPtr&                         req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::string                                   sessionId);
    void OnlinePlayers(const HttpRequestPtr&                         req,
                       std::function<void(const HttpResponsePtr&)>&& callback);

    void Forward(const HttpRequestPtr&                              downstream,
                 const std::function<void(const HttpResponsePtr&)>& callback);
    static void BadRequest(
        const std::function<void(const HttpResponsePtr&)>& callback,
        const std::string&                                 message);
    static void Unreachable(
        const std::function<void(const HttpResponsePtr&)>& callback);
};
}  // namespace apiGateway
