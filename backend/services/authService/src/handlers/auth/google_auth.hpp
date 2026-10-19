//
// Created by divmone on 3/11/2026.
//

#pragma once

#include <userver/server/handlers/http_handler_base.hpp>

#include "services/auth_service.hpp"

namespace shop::handlers {
class GoogleAuthHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handler-auth-google";

    GoogleAuthHandler(const userver::components::ComponentConfig&  config,
                      const userver::components::ComponentContext& context);

    std::string HandleRequestThrow(
        const userver::server::http::HttpRequest& request,
        userver::server::request::RequestContext& context) const override;

private:
    shop::services::AuthService& auth_service_;
};
}  // namespace shop::handlers
