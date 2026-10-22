#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <components/redis_client_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::resign {
namespace common = ::handlers::gameService::common;

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    const auto player_id = static_cast<std::int64_t>(request.body.id);

    const auto error = match_manager.Resign(request.gameId, player_id);
    if (error == durak::match::MatchError::kMatchNotFound) {
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }
    if (error == durak::match::MatchError::kUnknownPlayer) {
        common::ThrowApiError<Response404>(common::kErrPlayerUnknown,
                                           "player " + std::to_string(player_id) + " is not a member of game " + request.gameId);
    }

    const auto info = match_manager.GetMatch(request.gameId);
    if (!info) {
        common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + request.gameId + " is not found");
    }

    if (error == durak::match::MatchError::kOk) {
        const auto game_id = common::ParseGameId(request.gameId);
        if (const auto redis_client = deps[kRedisClientComponent].GetRedisClient()) {
            durak::match::MatchError result_error = durak::match::MatchError::kOk;
            const auto result = match_manager.GetMatchResult(request.gameId, &result_error);
            if (result) {
                common::TryPublish(redis_client, common::kChannelGameFinished, common::BuildFinishedEvent(game_id, *result));
            }
        }
    }

    Response200 ok;
    ok.body = common::MakeGame(*info);
    return ok;
}

}  // namespace handlers::gameService::resign
