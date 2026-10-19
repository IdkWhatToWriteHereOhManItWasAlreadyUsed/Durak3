//
// Created by dmitry on 25.04.2026.
//

#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>

#include <memory>

namespace apiGateway {
using namespace drogon;

class AuthCheckMiddleware : public HttpMiddleware<AuthCheckMiddleware> {
private:
    static constexpr std::size_t TOKEN_HEADER_OFFSET = 7;
    HttpClientPtr                m_authServiceClient = nullptr;
    bool                         m_isEnabled;
public:
    AuthCheckMiddleware() {
        auto envValue = std::getenv("OAUTH_TOKEN_CHECK_ENABLED");

        if (std::string env = envValue ? envValue : ""; !env.empty() && env == "false") {
            m_isEnabled = false;
            LOG_INFO << "Auth middleware was not enabled!";
        } else {
            m_isEnabled         = true;
            m_authServiceClient = HttpClient::newHttpClient(app().getCustomConfig()["auth"].asString());
            LOG_INFO << "Auth middleware launched";
        }
    }

    void invoke(const drogon::HttpRequestPtr& req, MiddlewareNextCallback&& nextCb, MiddlewareCallback&& mcb) override {
        LOG_INFO << "AuthCheckMiddleware invoked for path: " << req->path();

        if (!m_isEnabled) {
            LOG_INFO << "Auth middleware disabled, allowing request";
            nextCb([mcb = std::move(mcb)](const HttpResponsePtr& resp) { mcb(resp); });
            return;
        }

        const auto& authHeader = req->getHeader("Authorization");
        LOG_INFO << "Auth header present: " << (!authHeader.empty() ? "yes" : "no");

        if (authHeader.size() > TOKEN_HEADER_OFFSET && authHeader.substr(0, 7) == "Bearer ") {
            const auto token = authHeader.substr(TOKEN_HEADER_OFFSET);
            LOG_INFO << "Valid Bearer token detected, validating with auth-service";

            auto authRequest = HttpRequest::newHttpRequest();
            authRequest->setMethod(drogon::Get);
            authRequest->setPath("/auth/me");
            authRequest->addHeader("Authorization", "Bearer " + token);

            m_authServiceClient->sendRequest(
                authRequest,
                [req, nextCb = std::move(nextCb), mcb = std::move(mcb)](ReqResult result, const HttpResponsePtr& authResponse) mutable {
                    if (result == ReqResult::Ok && authResponse && authResponse->getStatusCode() == k200OK) {
                        auto json = authResponse->getJsonObject();
                        if (json && json->isMember("id")) {
                            int64_t userId = (*json)["id"].asInt64();
                            LOG_INFO << "Request from user: " << userId;

                            // Модифицируем запрос: контроллерам opaque-токен не
                            // нужен, дальше гуляет только внутренний id игрока.
                            req->getAttributes()->insert("userId", userId);

                            nextCb([mcb = std::move(mcb)](const HttpResponsePtr& resp) { mcb(resp); });
                            return;
                        }
                        LOG_ERROR << "Auth response missing 'id' field";
                    } else if (authResponse && authResponse->getStatusCode() == k401Unauthorized) {
                        LOG_INFO << "Invalid or expired token";
                    } else {
                        LOG_ERROR << "Auth service error: " << static_cast<int>(result);
                    }

                    auto response = HttpResponse::newHttpResponse();
                    response->setStatusCode(k401Unauthorized);
                    response->setContentTypeCode(CT_APPLICATION_JSON);
                    response->setBody(R"({"error":"Unauthorized"})");
                    mcb(response);
                },
                5);
        } else {
            LOG_INFO << "Missing or invalid Authorization header";
            auto response = HttpResponse::newHttpResponse();
            response->setStatusCode(k401Unauthorized);
            response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
            response->setBody(R"({"error":"Missing or invalid Authorization header"})");
            mcb(response);
        }

        fflush(stdout);
    }
};
}  // namespace apiGateway
