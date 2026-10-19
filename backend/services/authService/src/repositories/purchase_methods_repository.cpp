//
// Created by divmone on 4/24/2026.
//

#include "purchase_methods_repository.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace shop::repositories {
PurchaseMethodsRepository::PurchaseMethodsRepository(
    const userver::components::ComponentConfig&  config,
    const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_cluster(
          context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()) {}

PurchaseMethod PurchaseMethodsRepository::create(int                userId,
                                                 const std::string& type,
                                                 const std::string& details) {
    const auto result =
        pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "INSERT INTO purchase_methods(user_id, type, details) VALUES($1, $2, "
                            "$3::jsonb) RETURNING id, user_id, type, details::text",
                            userId, type, details);

    const auto row = result.Front();
    return {row["id"].As<int>(), row["user_id"].As<int>(), row["type"].As<std::string>(),
            row["details"].As<std::string>()};
}

void PurchaseMethodsRepository::deleteById(int id, int user_id) {
    pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        "DELETE FROM purchase_methods WHERE id = $1 AND user_id = $2", id, user_id);
}

std::vector<PurchaseMethod> PurchaseMethodsRepository::getAll(int userId) {
    const auto result = pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                                            "SELECT id, user_id, type, details::text FROM "
                                            "purchase_methods WHERE user_id = $1",
                                            userId);

    std::vector<PurchaseMethod> methods;
    for (const auto& row : result) {
        methods.push_back({row["id"].As<int>(), row["user_id"].As<int>(),
                           row["type"].As<std::string>(), row["details"].As<std::string>()});
    }
    return methods;
}
}  // namespace shop::repositories
