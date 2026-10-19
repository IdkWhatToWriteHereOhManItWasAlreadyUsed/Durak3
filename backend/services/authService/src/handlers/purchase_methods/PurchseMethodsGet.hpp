//
// Created by divmone on 4/24/2026.
//
#pragma once

#include <userver/server/handlers/http_handler_base.hpp>

#include "services/purchase_methods_service.hpp"

namespace shop::handlers {

class PurchaseMethodsGet final : public userver::server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "handlers-purchase-methods-get";

    PurchaseMethodsGet(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);

    std::string HandleRequestThrow(const userver::server::http::HttpRequest& request,
                                   userver::server::request::RequestContext& context) const override;

private:
    shop::services::PurchaseMethodsService& service_;
};

}  // namespace shop::handlers