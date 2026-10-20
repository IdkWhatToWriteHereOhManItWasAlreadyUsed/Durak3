#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::getgame {
namespace common = ::handlers::gameService::common;

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    const auto info = match_manager.GetMatch(request.gameId);
    if (!info) {
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }

    Response200 ok;
    ok.body = common::MakeGame(*info);
    return ok;
}

}  // namespace handlers::gameService::getgame
