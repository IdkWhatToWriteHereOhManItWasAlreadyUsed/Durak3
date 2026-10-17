//
// Created by divmone on 4/24/2026.
//

#ifndef DB_TEST_PURCHASE_METHODS_REPOSITORY_HPP
#define DB_TEST_PURCHASE_METHODS_REPOSITORY_HPP

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>
#include "models/user.hpp"

namespace shop::repositories {
    class PurchaseMethodsRepository final : public
            userver::components::ComponentBase {
    public:
        static constexpr std::string_view kName = "purchaseMethods-repository";

        PurchaseMethodsRepository(
            const userver::components::ComponentConfig &config,
            const userver::components::ComponentContext &context
        );

        PurchaseMethod create(int userId,
                              const std::string &type,
                              const std::string &details);

        void deleteById(int id, int user_id);

        std::vector<PurchaseMethod> getAll(int user_id);

    private:
        userver::storages::postgres::ClusterPtr pg_cluster;
    };
}

#endif //DB_TEST_PURCHASE_METHODS_REPOSITORY_HPP
