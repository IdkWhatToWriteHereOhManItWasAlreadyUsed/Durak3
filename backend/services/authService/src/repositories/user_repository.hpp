//
// Created by divmone on 3/11/2026.
//

#pragma once

#include <userver/components/component_base.hpp>
#include <userver/storages/postgres/cluster.hpp>

#include "models/user.hpp"

namespace shop::repositories {
class UserRepository final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "user-repository";

    UserRepository(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);

    User create(const std::string& email, const std::string& username, const std::string& avatar_url);

    void                linkOAuthProvider(int user_id, const std::string& provider, const std::string& provider_id);
    std::optional<User> findByOauthId(const std::string&, const std::string&) const;
    std::optional<User> findById(int64_t id) const;

    std::string createSession(int user_id);
    void        deleteSession(const std::string& token);

    std::optional<User> findByToken(const std::string& token) const;

private:
    userver::storages::postgres::ClusterPtr pg_cluster;
};
}  // namespace shop::repositories
