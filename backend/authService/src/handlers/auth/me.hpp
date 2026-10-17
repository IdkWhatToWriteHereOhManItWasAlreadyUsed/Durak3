//
// Created by divmone on 4/10/2026.
//

#pragma once

#include <services/auth_service.hpp>
#include <userver/server/handlers/http_handler_base.hpp>

#include "repositories/user_repository.hpp"


namespace shop::handlers {

    class MeHandler final : public userver::server::handlers::HttpHandlerBase {
    public:
        static constexpr std::string_view kName = "handler-me";

        MeHandler(const userver::components::ComponentConfig &config,
                const userver::components::ComponentContext &context);

        std::string HandleRequestThrow(const userver::server::http::HttpRequest &request, userver::server::request::RequestContext &context) const override;

    private:
        shop::services::AuthService& auth_service_;
    };

}
