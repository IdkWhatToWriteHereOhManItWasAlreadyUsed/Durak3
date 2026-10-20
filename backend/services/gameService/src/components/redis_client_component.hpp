#pragma once

#include <userver/chaotic/openapi/server/dependencies.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/storages/redis/component.hpp>

class RedisClientComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "redis-client-component";
    static constexpr std::string_view kRedisDatabaseName = "s";

    RedisClientComponent(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : ComponentBase(config, context),
          redisClient_(context.FindComponent<userver::components::Redis>(kRedisDatabaseName).GetClient("")) {
        using userver::chaotic::openapi::server::dependencies::Factories;
        using userver::components::Container;

        context.FindComponent<Container<Factories>>("chaotic-openapi-factories").Get().Register([this]() -> RedisClientComponent& {
            return *this;
        });
    }

    userver::storages::redis::ClientPtr GetRedisClient() noexcept { return redisClient_; }
private:
    userver::storages::redis::ClientPtr redisClient_;
};

inline constexpr userver::chaotic::openapi::server::dependencies::FactoryTag<RedisClientComponent&> kRedisClientComponent{};
