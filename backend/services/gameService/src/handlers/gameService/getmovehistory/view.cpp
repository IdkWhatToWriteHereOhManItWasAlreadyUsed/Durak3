#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::getmovehistory {
namespace common = ::handlers::gameService::common;

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    durak::match::MatchError error = durak::match::MatchError::kOk;
    const auto history = match_manager.GetMatchHistory(request.gameId, &error);
    if (!history) {
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }

    games_gameid_moves_history::get::Response200BodyApplicationJson body;
    body.total = static_cast<int>(history->size());
    body.items.emplace();
    body.items->reserve(history->size());
    for (const auto& move : *history) {
        body.items->push_back(common::MakeAppliedMove(move));
    }

    Response200 ok;
    ok.body = std::move(body);
    return ok;
}

}  // namespace handlers::gameService::getmovehistory
