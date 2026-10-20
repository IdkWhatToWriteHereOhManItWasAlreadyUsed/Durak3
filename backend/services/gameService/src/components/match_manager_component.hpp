#pragma once

#include <match_manager/match_manager.hpp>
#include <userver/chaotic/openapi/server/dependencies.hpp>
#include <userver/components/component_base.hpp>
#include <userver/components/component_context.hpp>

class MatchManagerComponent final : public userver::components::ComponentBase {
public:
    static constexpr std::string_view kName = "match-manager-component";

    MatchManagerComponent(const userver::components::ComponentConfig& config, const userver::components::ComponentContext& context)
        : ComponentBase(config, context) {
        using namespace userver;
        auto& factories =
            context.FindComponent<components::Container<chaotic::openapi::server::dependencies::Factories>>("chaotic-openapi-factories")
                .Get();

        factories.Register([this]() -> MatchManagerComponent& { return *this; });
    }

    durak::match::MatchManager& GetManager() noexcept { return match_manager_; }
private:
    durak::match::MatchManager match_manager_;
};

inline constexpr userver::chaotic::openapi::server::dependencies::FactoryTag<MatchManagerComponent&> kMatchManagerComponent{};
