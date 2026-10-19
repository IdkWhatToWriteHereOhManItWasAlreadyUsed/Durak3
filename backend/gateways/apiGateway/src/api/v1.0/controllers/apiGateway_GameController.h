#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>
#include <json/json.h>

using namespace drogon;

namespace apiGateway {
class GameController : public drogon::HttpController<GameController, false> {
    HttpClientPtr client_ = nullptr;
public:
    explicit GameController();
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(GameController::GetGame, "/api/games/{gameId}", drogon::Get);
    ADD_METHOD_TO(GameController::GetGameState,
                  "/api/games/{gameId}/state",
                  drogon::Get,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(GameController::MakeMove,
                  "/api/games/{gameId}/moves",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(GameController::GetMoveHistory,
                  "/api/games/{gameId}/moves/history",
                  drogon::Get);
    ADD_METHOD_TO(GameController::Resign,
                  "/api/games/{gameId}/resign",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(GameController::GetGameResult,
                  "/api/games/{gameId}/result",
                  drogon::Get);
    METHOD_LIST_END
private:
    void GetGame(const HttpRequestPtr&                         req,
                 std::function<void(const HttpResponsePtr&)>&& callback,
                 std::string                                   gameId);
    void GetGameState(const HttpRequestPtr&                         req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      std::string                                   gameId);
    void MakeMove(const HttpRequestPtr&                         req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string                                   gameId);
    void GetMoveHistory(const HttpRequestPtr&                         req,
                        std::function<void(const HttpResponsePtr&)>&& callback,
                        std::string                                   gameId);
    void Resign(const HttpRequestPtr&                         req,
                std::function<void(const HttpResponsePtr&)>&& callback,
                std::string                                   gameId);
    void GetGameResult(const HttpRequestPtr&                         req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       std::string                                   gameId);

    void Forward(const HttpRequestPtr&                              downstream,
                 const std::function<void(const HttpResponsePtr&)>& callback);
    static void Unreachable(
        const std::function<void(const HttpResponsePtr&)>& callback);
};
}  // namespace apiGateway
