//
// Created by divmone on 3/11/2026.
//

#include "google_auth.hpp"

#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_error.hpp>

namespace shop::handlers {
GoogleAuthHandler::GoogleAuthHandler(const userver::components::ComponentConfig& config,
                                     const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), auth_service_(context.FindComponent<shop::services::AuthService>()) {}

std::string GoogleAuthHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                                  userver::server::request::RequestContext& context) const {
    const auto body = userver::formats::json::FromString(request.RequestBody());
    const auto token = body["code"].As<std::string>("");
    const auto redirect_uri = body["redirect_uri"].As<std::string>("");

    if (token.empty()) {
        throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{"google_token is required"});
    }

    if (redirect_uri.empty()) {
        throw userver::server::handlers::ClientError(userver::server::handlers::ExternalBody{"redirect_uri is required"});
    }

    const auto& response = auth_service_.loginWithGoogle(token, redirect_uri);

    const auto authToken = auth_service_.createSession(response.id);

    userver::formats::json::ValueBuilder resp;
    resp["id"] = response.id;
    resp["email"] = response.email;
    resp["username"] = response.username;
    resp["avatar_url"] = response.avatar_url;
    resp["accessToken"] = authToken;
    return userver::formats::json::ToString(resp.ExtractValue());
}
}  // namespace shop::handlers
