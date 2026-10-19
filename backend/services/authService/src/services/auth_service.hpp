//
// Created by divmone on 3/11/2026.
//

#pragma once

#include <userver/clients/http/client.hpp>
#include <userver/yaml_config/merge_schemas.hpp>

#include "models/user.hpp"
#include "repositories/user_repository.hpp"

namespace shop::services {
class AuthService final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "auth-service";

    explicit AuthService(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context);

    User loginWithGoogle(const std::string& code, const std::string& redirect_uri);
    User getUserByToken(const std::string& token) const;

    static userver::yaml_config::Schema GetStaticConfigSchema();

    std::string createSession(int user_id) const;
    void        deleteSession(const std::string& token) const;

private:
    repositories::UserRepository&   user_repository_;
    userver::clients::http::Client& client_;
    std::string                     google_client_id_;
    std::string                     google_client_secret_;
};
}  // namespace shop::services
