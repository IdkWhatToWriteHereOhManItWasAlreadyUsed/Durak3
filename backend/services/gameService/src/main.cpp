#include <handlers/gameService/chaotic_handlers_list.hpp>
#include <userver/clients/dns/component.hpp>
#include <userver/clients/http/component_list.hpp>
#include <userver/components/component.hpp>
#include <userver/components/component_list.hpp>
#include <userver/components/container.hpp>
#include <userver/components/minimal_server_component_list.hpp>
#include <userver/congestion_control/component.hpp>
#include <userver/server/handlers/ping.hpp>
#include <userver/storages/redis/component.hpp>
#include <userver/storages/secdist/component.hpp>
#include <userver/storages/secdist/provider_component.hpp>
#include <userver/utils/daemon_run.hpp>

#include "components/match_manager_component.hpp"
#include "components/redis_client_component.hpp"

int main(int argc, char* argv[]) {
    using Dependencies = userver::chaotic::openapi::server::dependencies::Factories;

    auto component_list = userver::components::MinimalServerComponentList()
                              .Append<userver::server::handlers::Ping>()
                              .AppendComponentList(userver::clients::http::ComponentList())
                              .Append<userver::clients::dns::Component>()
                              .Append<userver::congestion_control::Component>()
                              .Append<userver::components::Container<Dependencies>>()
                              .AppendComponentList(::handlers::gameService::ChaoticHandlersList())
                              .Append<MatchManagerComponent>()
                              .Append<userver::components::Secdist>()
                              .Append<userver::components::DefaultSecdistProvider>()
                              .Append<userver::components::Redis>("key-value-database")
                              .Append<RedisClientComponent>();

    return userver::utils::DaemonMain(argc, argv, component_list);
}
