//
// Created by divmone on 3/22/2026.
//

#include "logout.hpp"

#include <services/auth_service.hpp>
#include <userver/components/component_context.hpp>

namespace shop::handlers {
LogoutHandler::LogoutHandler(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context), auth_service_(context.FindComponent<shop::services::AuthService>()) {}

std::string LogoutHandler::HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                              userver::server::request::RequestContext& context) const {
    const auto auth  = request.GetHeader("Authorization");
    const auto token = auth.substr(7);

    auth_service_.deleteSession(token);

    request.GetHttpResponse().SetStatus(userver::server::http::HttpStatus::kOk);
    return "{}";
}

}  // namespace shop::handlers
