//
// Created by divmone on 3/11/2026.
//

#include "user_repository.hpp"

#include <userver/components/component_context.hpp>
#include <userver/storages/postgres/component.hpp>

namespace shop::repositories {
UserRepository::UserRepository(const userver::components::ComponentConfig&  config,
                               const userver::components::ComponentContext& context)
    : ComponentBase(config, context),
      pg_cluster(
          context.FindComponent<userver::components::Postgres>("postgres-db-1").GetCluster()) {}

User UserRepository::create(const std::string& email,
                            const std::string& username,
                            const std::string& avatar_url) {
    const auto result =
        pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "INSERT INTO users(email, username, avatar_url) VALUES($1, $2, $3) "
                            "RETURNING id, email, username, avatar_url",
                            email, username, avatar_url);

    const auto row = result.Front();
    return User{row["id"].As<int>(), row["email"].As<std::string>(),
                row["username"].As<std::string>(), row["avatar_url"].As<std::string>()};
}

void UserRepository::linkOAuthProvider(int                user_id,
                                       const std::string& provider,
                                       const std::string& provider_id) {
    const auto result =
        pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "INSERT INTO user_oauth_providers(user_id, provider, provider_id) "
                            "VALUES($1, $2, $3) ON CONFLICT (provider, provider_id) DO NOTHING",
                            user_id, provider, provider_id);
}

std::optional<User> UserRepository::findByOauthId(const std::string& provider,
                                                  const std::string& provider_id) const {
    const auto result = pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                                            "SELECT u.id, u.email, u.username, u.avatar_url "
                                            "FROM users u "
                                            "JOIN user_oauth_providers p ON p.user_id = u.id "
                                            "WHERE p.provider = $1 AND p.provider_id = $2",
                                            provider, provider_id);

    if (result.IsEmpty()) {
        return std::nullopt;
    }

    const auto row = result.Front();
    return User{row["id"].As<int>(), row["email"].As<std::string>(),
                row["username"].As<std::string>(), row["avatar_url"].As<std::string>()};
}

std::optional<User> UserRepository::findById(int64_t id) const {
    const auto res =
        pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                            "SELECT id, email, username, avatar_url FROM users WHERE id = $1", id);

    if (res.IsEmpty()) {
        return std::nullopt;
    }

    const auto row = res.Front();
    return User(row["id"].As<int>(), row["email"].As<std::string>(),
                row["username"].As<std::string>(), row["avatar_url"].As<std::string>());
}

std::string UserRepository::createSession(int user_id) {
    const auto result = pg_cluster->Execute(
        userver::storages::postgres::ClusterHostType::kMaster,
        "INSERT INTO sessions(user_id) VALUES($1) RETURNING token::text", user_id);

    return result[0][0].As<std::string>();
}

void UserRepository::deleteSession(const std::string& token) {
    pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                        "DELETE FROM sessions WHERE token = $1::uuid", token);
}

std::optional<User> UserRepository::findByToken(const std::string& token) const {
    const auto result = pg_cluster->Execute(userver::storages::postgres::ClusterHostType::kMaster,
                                            "SELECT u.id, u.email, u.username, u.avatar_url "
                                            "FROM sessions s JOIN users u ON u.id = s.user_id "
                                            "WHERE s.token = $1::uuid AND s.expires_at > now()",
                                            token);

    if (result.IsEmpty()) {
        return std::nullopt;
    }

    const auto row = result.Front();
    return User{row["id"].As<int>(), row["email"].As<std::string>(),
                row["username"].As<std::string>(), row["avatar_url"].As<std::string>()};
}
}  // namespace shop::repositories
