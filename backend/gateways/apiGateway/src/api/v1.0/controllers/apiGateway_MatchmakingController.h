#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>
#include <json/json.h>

using namespace drogon;

namespace apiGateway {
class MatchmakingController : public drogon::HttpController<MatchmakingController, false> {
    HttpClientPtr client_ = nullptr;
public:
    explicit MatchmakingController();
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MatchmakingController::CreateInvite,
                  "/api/invites",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::ListInvites,
                  "/api/invites",
                  drogon::Get,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::GetInvite, "/api/invites/{inviteId}", drogon::Get);
    ADD_METHOD_TO(MatchmakingController::AcceptInvite,
                  "/api/invites/{inviteId}/accept",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::DeclineInvite,
                  "/api/invites/{inviteId}/decline",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::CancelInvite,
                  "/api/invites/{inviteId}/cancel",
                  drogon::Post,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::ListMatches,
                  "/api/matches",
                  drogon::Get,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(MatchmakingController::GetMatch, "/api/matches/{matchId}", drogon::Get);
    METHOD_LIST_END
private:
    void CreateInvite(const HttpRequestPtr&                         req,
                      std::function<void(const HttpResponsePtr&)>&& callback);
    void ListInvites(const HttpRequestPtr&                         req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void GetInvite(const HttpRequestPtr&                         req,
                   std::function<void(const HttpResponsePtr&)>&& callback,
                   std::string                                   inviteId);
    void AcceptInvite(const HttpRequestPtr&                         req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      std::string                                   inviteId);
    void DeclineInvite(const HttpRequestPtr&                         req,
                       std::function<void(const HttpResponsePtr&)>&& callback,
                       std::string                                   inviteId);
    void CancelInvite(const HttpRequestPtr&                         req,
                      std::function<void(const HttpResponsePtr&)>&& callback,
                      std::string                                   inviteId);
    void ListMatches(const HttpRequestPtr&                         req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void GetMatch(const HttpRequestPtr&                         req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  std::string                                   matchId);

    void        Forward(const HttpRequestPtr&                              downstream,
                        const std::function<void(const HttpResponsePtr&)>& callback);
    static void Unreachable(const std::function<void(const HttpResponsePtr&)>& callback);
};
}  // namespace apiGateway
