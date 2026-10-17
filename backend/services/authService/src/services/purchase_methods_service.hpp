//
// Created by divmone on 4/24/2026.
//

#pragma once

#include <userver/components/component_base.hpp>
#include "repositories/purchase_methods_repository.hpp"
#include "models/user.hpp"

namespace shop::services {
    class PurchaseMethodsService final : public userver::components::ComponentBase {
    public:
        static constexpr std::string_view kName = "purchase-methods-service";

        PurchaseMethodsService(
            const userver::components::ComponentConfig& config,
            const userver::components::ComponentContext& context);

        std::vector<PurchaseMethod> getByUserId(int user_id);
        PurchaseMethod create(int user_id, const std::string& type, const std::string& details);
        void deleteById(int id, int user_id);

    private:
        repositories::PurchaseMethodsRepository& repository_;
    };
}
