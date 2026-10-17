//
// Created by divmone on 3/26/2026.
//

#include "users.hpp"

#include <userver/components/component_context.hpp>

shop::handlers::UserHandler::UserHandler(
    const userver::components::ComponentConfig &config,
    const userver::components::ComponentContext &context)
        : HttpHandlerBase(config, context)
        , user_repository_(context.FindComponent<shop::repositories::UserRepository>()){
}

std::string shop::handlers::UserHandler::HandleRequestThrow(
    const userver::server::http::HttpRequest &request,
    userver::server::request::RequestContext &context) const {

    const auto id = std::stoll(request.GetPathArg("id"));
    const auto response = user_repository_.findById(id);
    if (!response.has_value()) {
        request.SetResponseStatus(userver::server::http::HttpStatus::kNotFound);
        return {"not found"};
    }

    userver::formats::json::ValueBuilder resp;
    resp["id"] = response->id;
    resp["email"] = response->email;
    resp["username"] = response->username;
    resp["avatar_url"] = response->avatar_url;
    return userver::formats::json::ToString(resp.ExtractValue());
}
