//
// Created by divmone on 3/11/2026.
//

#include "auth_service.hpp"

#include <userver/components/component_config.hpp>
#include <userver/components/component_context.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/server/handlers/exceptions.hpp>
#include <userver/utils/exception.hpp>

namespace shop::services {
    userver::yaml_config::Schema AuthService::GetStaticConfigSchema() {
        return userver::yaml_config::MergeSchemas<userver::components::ComponentBase>(R"(
type: object
description: AuthService config
additionalProperties: false
properties:
    google-client-id:
        type: string
        description: Google OAuth2 client ID
    google-client-secret:
        type: string
        description: Google OAuth2 client secret
)");
    }

    std::string AuthService::createSession(int user_id) const {
        return user_repository_.createSession(user_id);
    }

    void AuthService::deleteSession(const std::string &token) const {
        user_repository_.deleteSession(token);
    }

    AuthService::AuthService(const userver::components::ComponentConfig &config,
                             const userver::components::ComponentContext &context)
        : ComponentBase(config, context)
          , user_repository_(context.FindComponent<repositories::UserRepository>("user-repository"))
          , client_(context.FindComponent<userver::components::HttpClient>("http-client").GetHttpClient())
          , google_client_id_(config["google-client-id"].As<std::string>())
          , google_client_secret_(config["google-client-secret"].As<std::string>()) {
    }

    User AuthService::getUserByToken(const std::string& token) const {
        const auto user = user_repository_.findByToken(token);
        if (!user) {
            throw userver::server::handlers::Unauthorized(
                userver::server::handlers::ExternalBody{"Invalid or expired token"});
        }

        return *user;
    }

    User AuthService::loginWithGoogle(const std::string &code, const std::string &redirect_uri) {
        const auto token_response = client_.CreateRequest()
            .post(
                "https://oauth2.googleapis.com/token",
                "code=" + code +
                "&client_id=" + google_client_id_ +
                "&client_secret=" + google_client_secret_ +
                "&redirect_uri=" + redirect_uri +
                "&grant_type=authorization_code"
            )
            .headers({{"Content-Type", "application/x-www-form-urlencoded"}})
            .retry(2)
            .timeout(std::chrono::seconds(10))
            .perform();

        if (token_response->status_code() != 200) {
            throw userver::server::handlers::ClientError(
                userver::server::handlers::ExternalBody{
                    "Google token exchange failed: " + token_response->body()
                });
        }

        const auto token_json = userver::formats::json::FromString(token_response->body());
        const auto id_token = token_json["id_token"].As<std::string>("");

        if (id_token.empty()) {
            throw userver::server::handlers::ClientError(
                userver::server::handlers::ExternalBody{"Google did not return id_token"});
        }

        const auto userinfo_response = client_.CreateRequest()
            .get("https://oauth2.googleapis.com/tokeninfo?id_token=" + id_token)
            .retry(2)
            .timeout(std::chrono::seconds(10))
            .perform();

        if (userinfo_response->status_code() != 200) {
            throw userver::server::handlers::ClientError(
                userver::server::handlers::ExternalBody{
                    "Google tokeninfo failed: " + userinfo_response->body()
                });
        }

        const auto userinfo = userver::formats::json::FromString(userinfo_response->body());
        const auto google_sub   = userinfo["sub"].As<std::string>();
        const auto email        = userinfo["email"].As<std::string>();
        const auto name         = userinfo["name"].As<std::string>("");
        const auto avatar_url   = userinfo["picture"].As<std::string>("");

        const auto existing = user_repository_.findByOauthId("google", google_sub);
        if (existing.has_value()) {
            return *existing;
        }

        const auto& user = user_repository_.create(email, name, avatar_url);
        user_repository_.linkOAuthProvider(user.id, "google", google_sub);

        return user;
    }
}
