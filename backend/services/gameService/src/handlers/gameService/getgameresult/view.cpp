#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::getgameresult {
namespace common = ::handlers::gameService::common;

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    durak::match::MatchError error = durak::match::MatchError::kOk;
    const auto result = match_manager.GetMatchResult(request.gameId, &error);
    if (!result) {
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }

    Response200 ok;
    ok.body = common::MakeGameResult(*result);
    return ok;
}

}  // namespace handlers::gameService::getgameresult
