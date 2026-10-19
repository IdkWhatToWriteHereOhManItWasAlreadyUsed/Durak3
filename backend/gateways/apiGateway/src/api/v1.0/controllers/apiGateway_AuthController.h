#pragma once

#include <drogon/HttpClient.h>
#include <drogon/HttpController.h>

using namespace drogon;

namespace apiGateway {
class AuthController : public HttpController<AuthController, false> {
    HttpClientPtr client_ = nullptr;
public:
    explicit AuthController();
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(AuthController::GoogleLogin, "/api/auth/google", drogon::Post);
    ADD_METHOD_TO(AuthController::Me,
                  "/api/auth/me",
                  drogon::Get,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(AuthController::Logout,
                  "/api/auth/logout",
                  drogon::Delete,
                  "apiGateway::AuthCheckMiddleware");
    ADD_METHOD_TO(AuthController::UserById, "/api/auth/users/{id}", drogon::Get);
    METHOD_LIST_END
private:
    void GoogleLogin(const HttpRequestPtr&                         req,
                     std::function<void(const HttpResponsePtr&)>&& callback);
    void Me(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback);
    void Logout(const HttpRequestPtr& req, std::function<void(const HttpResponsePtr&)>&& callback);
    void UserById(const HttpRequestPtr&                         req,
                  std::function<void(const HttpResponsePtr&)>&& callback,
                  uint64_t                                      id);

    void        Forward(const HttpRequestPtr&                              req,
                        const HttpRequestPtr&                              downstream,
                        const std::function<void(const HttpResponsePtr&)>& callback);
    static void BadRequest(const std::function<void(const HttpResponsePtr&)>& callback,
                           const std::string&                                 message);
    static void Unreachable(const std::function<void(const HttpResponsePtr&)>& callback);
};
}  // namespace apiGateway
