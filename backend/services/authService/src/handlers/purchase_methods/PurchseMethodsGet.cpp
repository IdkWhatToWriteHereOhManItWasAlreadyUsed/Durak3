//
// Created by divmone on 4/24/2026.
//

#include "PurchseMethodsGet.hpp"
#include <userver/components/component_context.hpp>

namespace shop::handlers {
PurchaseMethodsGet::PurchaseMethodsGet(
    const userver::components::ComponentConfig& config,
    const userver::components::ComponentContext& context)
    : HttpHandlerBase(config, context),
      service_(context.FindComponent<services::PurchaseMethodsService>()) {}

std::string PurchaseMethodsGet::HandleRequestThrow(
    const userver::server::http::HttpRequest& request,
    userver::server::request::RequestContext& context) const {
  return HttpHandlerBase::HandleRequestThrow(request, context);
}
}  // namespace shop::handlers
