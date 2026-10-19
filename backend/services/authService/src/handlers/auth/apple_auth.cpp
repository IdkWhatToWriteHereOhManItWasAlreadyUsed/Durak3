//
// Created by divmone on 3/11/2026.
//

#include "apple_auth.hpp"

#include <userver/components/component_context.hpp>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/server/http/http_error.hpp>

namespace shop::handlers {
AppleAuthHandler::AppleAuthHandler(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      auth_service_(context.FindComponent<shop::services::AuthService>()) {}

std::string AppleAuthHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context) const {
  const auto& code = request.GetArg("code");

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
