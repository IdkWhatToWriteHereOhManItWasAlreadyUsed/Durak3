#include "view.hpp"

#include <components/match_manager_component.hpp>
#include <handlers/gameService/common/game_views.hpp>

namespace handlers::gameService::creategame {
namespace common = ::handlers::gameService::common;

namespace {

durak::match::Player ToEnginePlayer(const CreateGameRequest::PlayersA& player) {
    return durak::match::Player{static_cast<std::int64_t>(player.id), player.name};
}

bool IsPlayerSetValid(const CreateGameRequest& body) {
    if (body.players.size() != durak::match::Match::kPlayersCount) {
        return false;
    }
    for (const auto& player : body.players) {
        if (player.id < 0) {
            return false;
        }
    }
    return body.players[0].id != body.players[1].id;
}

}  // namespace

Response View::Handle(Request&& request, Deps&& deps, RequestContext& /*context*/) {
    auto& match_manager_component = deps[kMatchManagerComponent];
    auto& match_manager = match_manager_component.GetManager();

    const auto& body = request.body;
    if (!IsPlayerSetValid(body)) {
        common::ThrowApiError<Response400>(common::kErrBadRequest, "players must contain exactly 2 unique players with non-negative ids");
    }

    std::vector<durak::match::Player> players;
    players.reserve(body.players.size());
    for (const auto& player : body.players) {
        players.push_back(ToEnginePlayer(player));
    }

    std::optional<std::int64_t> first_attacker_id;
    if (body.firstAttackerId) {
        first_attacker_id = static_cast<std::int64_t>(*body.firstAttackerId);
    }

    // Движок перемешивает колоду, раздаёт карты, выбирает козырь.
    const auto game_id = match_manager.CreateMatch(std::move(players), first_attacker_id);
    if (!game_id) {
        common::ThrowApiError<Response400>(common::kErrBadRequest, "failed to create the game");
    }

    const auto info = match_manager.GetMatch(*game_id);
    if (!info) {
        common::ThrowApiError<Response400>(common::kErrBadRequest, "created game is missing");
    }

    Response201 created;
    created.body = common::MakeGame(*info, body.matchId);
    return created;
}

}  // namespace handlers::gameService::creategame
