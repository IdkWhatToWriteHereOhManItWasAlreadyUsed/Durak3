#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::getgamestate {
namespace common = ::handlers::gameService::common;

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    // `id` — запрашивающий игрок (гейтвей подставляет его из токена).
    durak::match::MatchError error = durak::match::MatchError::kOk;
    const auto state = match_manager.GetMatchState(request.gameId, request.id, &error);
    if (!state) {
        if (error == durak::match::MatchError::kUnknownPlayer) {
            common::ThrowApiError<Response404>(common::kErrPlayerUnknown,
                                               "player " + std::to_string(request.id) + " is not a member of game " + request.gameId);
        }
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }

    Response200 ok;
    ok.body = common::MakeGameState(*state);
    return ok;
}

}  // namespace handlers::gameService::getgamestate
