//
// Created by divmone on 3/26/2026.
//

#pragma once

#include <userver/server/handlers/http_handler_base.hpp>

#include "repositories/user_repository.hpp"

namespace shop::handlers {
class UserHandler final : public userver::server::handlers::HttpHandlerBase {
public:
    constexpr static std::string_view kName = "handler-user";

    UserHandler(const userver::components::ComponentConfig&, const userver::components::ComponentContext&);
    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                   userver::server::request::RequestContext& context) const override;

private:
    shop::repositories::UserRepository& user_repository_;
};

}  // namespace shop::handlers