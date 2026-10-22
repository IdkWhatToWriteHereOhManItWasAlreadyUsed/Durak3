#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <components/redis_client_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::makemove {
namespace common = ::handlers::gameService::common;

namespace {

Response409 ThrowMoveError(durak::match::MatchError error, const std::string& game_id, std::int64_t player_id) {
    switch (error) {
        case durak::match::MatchError::kUnknownPlayer:
            common::ThrowApiError<Response409>(common::kErrPlayerUnknown,
                                               "player " + std::to_string(player_id) + " is not a member of game " + game_id);
        case durak::match::MatchError::kNotThisPlayersTurn:
            common::ThrowApiError<Response409>(common::kErrNotYourTurn, "it is not the turn of player " + std::to_string(player_id));
        case durak::match::MatchError::kMatchAlreadyFinished:
            common::ThrowApiError<Response409>(common::kErrGameFinished, "game " + game_id + " is already finished");
        case durak::match::MatchError::kIllegalMove:
        case durak::match::MatchError::kMatchNotFound:
        case durak::match::MatchError::kOk:
            break;
    }
    common::ThrowApiError<Response409>(common::kErrIllegalMove, "illegal move in game " + game_id);
}

Response404 ThrowGameNotFound(const std::string& game_id) {
    common::ThrowApiError<Response404>(common::kErrGameNotFound, "game " + game_id + " is not found");
}

}  // namespace

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    const auto player_id = static_cast<std::int64_t>(request.body.id);

    // Для CARD обязательны `card` и `stack` (схема не валидирует это, т.к.
    // обязательность полей зависит от `type`).
    if (request.body.type == MoveRequest::Type::kCard && !request.body.card.has_value()) {
        common::ThrowApiError<Response409>(common::kErrIllegalMove, "CARD move requires 'card'");
    }
    if (request.body.type == MoveRequest::Type::kCard && !request.body.stack.has_value()) {
        common::ThrowApiError<Response409>(common::kErrIllegalMove, "CARD move requires 'stack'");
    }

    durak::match::MoveRequest engine_move = common::ToEngineMove(request.body);

    // Движок проверяет очерёдность и правила.
    durak::match::MatchMove applied;
    const auto error = match_manager.ApplyMove(request.gameId, player_id, engine_move, &applied);
    if (error == durak::match::MatchError::kMatchNotFound) {
        ThrowGameNotFound(request.gameId);
    }
    if (error != durak::match::MatchError::kOk) {
        ThrowMoveError(error, request.gameId, player_id);
    }

    const auto game_id = common::ParseGameId(request.gameId);

    // Статус после хода: партия могла завершиться.
    bool match_finished = false;
    if (const auto info = match_manager.GetMatch(request.gameId)) {
        match_finished = info->status == durak::match::MatchStatus::kFinished;
    }

    // События для realtime-gateway (опциональная часть схемы).
    if (const auto redis_client = deps[kRedisClientComponent].GetRedisClient()) {
        common::TryPublish(redis_client, common::kChannelGameMove, common::BuildMoveEvent(game_id, applied, match_finished));

        if (match_finished) {
            auto result_error = durak::match::MatchError::kOk;
            if (const auto result = match_manager.GetMatchResult(request.gameId, &result_error)) {
                common::TryPublish(redis_client, common::kChannelGameFinished, common::BuildFinishedEvent(game_id, *result));
            }
        }
    }

    auto state_error = durak::match::MatchError::kOk;
    const auto state = match_manager.GetMatchState(request.gameId, player_id, &state_error);
    if (!state) {
        if (state_error == durak::match::MatchError::kMatchNotFound) {
            ThrowGameNotFound(request.gameId);
        }
        common::ThrowApiError<Response409>(common::kErrPlayerUnknown,
                                           "player " + std::to_string(player_id) + " is not a member of game " + request.gameId);
    }

    Response200 ok;
    ok.body = common::MakeGameState(*state);
    return ok;
}

}  // namespace handlers::gameService::makemove
