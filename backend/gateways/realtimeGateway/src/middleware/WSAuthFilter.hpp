#pragma once

#include <drogon/HttpAppFramework.h>
#include <drogon/HttpClient.h>
#include <drogon/HttpFilter.h>
#include <json/json.h>

using namespace drogon;

namespace realtimeGateway {
class WSAuthFilter : public HttpFilter<WSAuthFilter, false> {
private:
    HttpClientPtr m_authClient = nullptr;

public:
    WSAuthFilter() {
        m_authClient = HttpClient::newHttpClient(app().getCustomConfig()["auth"].asString());
    }

    void doFilter(const HttpRequestPtr& req,
                  FilterCallback&& fcb,
                  FilterChainCallback&& fccb) override {
        std::string token = req->getParameter("token");
        if (token.empty()) {
            const auto& authHeader = req->getHeader("Authorization");
            if (authHeader.size() > 7 && authHeader.substr(0, 7) == "Bearer ") {
                token = authHeader.substr(7);
            }
        }

        if (token.empty()) {
            fcb(Unauthorized());
            return;
        }

        auto authRequest = HttpRequest::newHttpRequest();
        authRequest->setMethod(drogon::Get);
        authRequest->setPath("/auth/me");
        authRequest->addHeader("Authorization", "Bearer " + token);

        auto client = m_authClient;
        client->sendRequest(
            authRequest,
            [req = req, fcb = std::move(fcb), fccb = std::move(fccb),
             token](ReqResult result, const HttpResponsePtr& response) mutable {
                if (result == ReqResult::Ok && response &&
                    response->getStatusCode() == k200OK) {
                    auto json = response->getJsonObject();
                    if (json && json->isMember("id")) {
                        req->getAttributes()->insert("userId", (*json)["id"].asInt64());
                        req->getAttributes()->insert("token", token);
                        fccb();
                        return;
                    }
                }
                fcb(Unauthorized());
            },
            5);
    }

private:
    static HttpResponsePtr Unauthorized() {
        auto response = HttpResponse::newHttpResponse();
        response->setStatusCode(k401Unauthorized);
        response->setContentTypeCode(ContentType::CT_APPLICATION_JSON);
        response->setBody(R"({"code":"SESSION_EXPIRED","message":"Invalid or expired token"})");
        return response;
    }
};
}  // namespace realtimeGateway
