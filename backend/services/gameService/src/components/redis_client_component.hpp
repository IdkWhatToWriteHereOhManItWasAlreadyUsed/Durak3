#pragma once
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/storages/redis/component.hpp>


class RedisClientComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "redis-client-component";

    RedisClientComponent(const userver::components::ComponentConfig& config,
                    const userver::components::ComponentContext& context)
      : ComponentBase(config, context),
        redisClient_(context.FindComponent<userver::components::Redis>("redis-component-1")
                        .GetClient("")) { }

    userver::storages::redis::ClientPtr GetRedisClient() noexcept {  return redisClient_; }
private:
    userver::storages::redis::ClientPtr redisClient_;
};
