#pragma once

#include <userver/chaotic/openapi/server/dependencies.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/storages/redis/component.hpp>

class RedisClientComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "redis-client-component";

    RedisClientComponent(const userver::components::ComponentConfig& config,
                         const userver::components::ComponentContext& context)
                      :  ComponentBase(config, context)
                      ,  redisClient_(context.FindComponent<userver::components::Redis>("redis-component-1")
                      .GetClient("redis-client-1")) {
        using userver::chaotic::openapi::server::dependencies::Factories;
        using userver::components::Container;
        /*
         *надо придумать имя клиенту редиса
         *вот у нас есть компонент наш редис клаент компонент вот этот класс
         *в нем есть компонент редиса который components::Redis
         *эта фиговина внутри фреймворка, ее можно назвать и конфигурировать
         *потоки там ей выделять, ну их может несокько быть
         *а в каждой такой фигне может быть много редисных клиентов
         *GetClient(имя клиента)
         *и у клиента свой конфиг, в него имя бд пишется и так далее
         */
        context.FindComponent<Container<Factories>>("chaotic-openapi-factories").Get().Register(
            [this]() -> RedisClientComponent& {
                return *this;
            }
        );
    }

    userver::storages::redis::ClientPtr GetRedisClient() noexcept { return redisClient_; }
private:
    userver::storages::redis::ClientPtr redisClient_;
};

inline constexpr userver::chaotic::openapi::server::dependencies::FactoryTag<RedisClientComponent&> kRedisClientComponent{};
