//
// Created by divmone on 4/24/2026.
//

#include "purchase_methods_service.hpp"

#include <userver/components/component_context.hpp>

shop::services::PurchaseMethodsService::PurchaseMethodsService(const userver::components::ComponentConfig& config,
                                                               const userver::components::ComponentContext& context)
    : ComponentBase(config, context), repository_(context.FindComponent<repositories::PurchaseMethodsRepository>()) {}

std::vector<shop::PurchaseMethod> shop::services::PurchaseMethodsService::getByUserId(int user_id) {
    return repository_.getAll(user_id);
}

shop::PurchaseMethod shop::services::PurchaseMethodsService::create(int user_id, const std::string& type, const std::string& details) {
    return repository_.create(user_id, type, details);
}

void shop::services::PurchaseMethodsService::deleteById(int id, int user_id) {
    repository_.deleteById(id, user_id);
}
