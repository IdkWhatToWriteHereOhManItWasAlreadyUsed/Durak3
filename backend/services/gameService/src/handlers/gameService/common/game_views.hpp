#pragma once

#include <chrono>
#include <handlers/gameService/openapi.hpp>
#include <match_manager/match_manager.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <userver/formats/json/serialize.hpp>
#include <userver/formats/json/value.hpp>
#include <userver/formats/json/value_builder.hpp>
#include <userver/logging/log.hpp>
#include <userver/storages/redis/client.hpp>
#include <userver/utils/boost_uuid4.hpp>
#include <userver/utils/datetime/timepoint_tz.hpp>
#include <utility>
#include <vector>

namespace handlers::gameService::common {

struct ApiTypes {
    using Card = ::handlers::gameService::Card;
    using PlayerPublic = ::handlers::gameService::PlayerPublic;
    using Game = ::handlers::gameService::Game;
    using GameState = ::handlers::gameService::GameState;
    using GameResult = ::handlers::gameService::GameResult;
    using AppliedMove = ::handlers::gameService::AppliedMove;
    using MoveRequest = ::handlers::gameService::MoveRequest;
    using Error = ::handlers::gameService::Error;
    using Stack = ::handlers::gameService::Stack;
};

inline constexpr std::string_view kErrBadRequest = "BAD_REQUEST";
inline constexpr std::string_view kErrGameNotFound = "GAME_NOT_FOUND";
inline constexpr std::string_view kErrPlayerUnknown = "PLAYER_UNKNOWN";
inline constexpr std::string_view kErrNotYourTurn = "NOT_YOUR_TURN";
inline constexpr std::string_view kErrIllegalMove = "ILLEGAL_MOVE";
inline constexpr std::string_view kErrGameFinished = "GAME_FINISHED";


template <class ErrorResponse>
[[noreturn]] void ThrowApiError(std::string_view code, std::string message) {
    ErrorResponse err;
    err.body = ::handlers::gameService::Error{std::string{code}, std::move(message)};
    throw err;
}

inline boost::uuids::uuid ParseGameId(const std::string& game_id) {
    return userver::utils::BoostUuidFromString(game_id);
}

inline std::string_view ToStringActionType(durak::match::ActionType type) {
    switch (type) {
        case durak::match::ActionType::kCard:
            return "CARD";
        case durak::match::ActionType::kTransfer:
            return "MOVE";
        case durak::match::ActionType::kGrab:
            return "GRAB";
        case durak::match::ActionType::kWin:
            return "WIN";
    }
    return "MOVE";
}

inline ApiTypes::Card ToApiCard(const durak::Card& card) {
    ApiTypes::Card api_card;
    api_card.suit = card.suit;
    api_card.power = card.power;
    return api_card;
}

inline std::vector<ApiTypes::Card> ToApiCards(const std::vector<durak::Card>& cards) {
    std::vector<ApiTypes::Card> result;
    result.reserve(cards.size());
    for (const auto& card : cards) {
        result.push_back(ToApiCard(card));
    }
    return result;
}

inline ApiTypes::PlayerPublic ToApiPlayer(const durak::match::PlayerPublic& player) {
    ApiTypes::PlayerPublic api_player;
    api_player.id = static_cast<int>(player.id);
    api_player.name = player.name;
    api_player.cardsLeft = player.cards_left;
    api_player.isAttacker = player.is_attacker;
    return api_player;
}

inline std::vector<ApiTypes::PlayerPublic> ToApiPlayers(const std::vector<durak::match::PlayerPublic>& players) {
    std::vector<ApiTypes::PlayerPublic> result;
    result.reserve(players.size());
    for (const auto& player : players) {
        result.push_back(ToApiPlayer(player));
    }
    return result;
}

inline durak::match::ActionType ToEngineActionType(ApiTypes::MoveRequest::Type type) {
    switch (type) {
        case ApiTypes::MoveRequest::Type::kCard:
            return durak::match::ActionType::kCard;
        case ApiTypes::MoveRequest::Type::kMove:
            return durak::match::ActionType::kTransfer;
        case ApiTypes::MoveRequest::Type::kGrab:
            return durak::match::ActionType::kGrab;
        case ApiTypes::MoveRequest::Type::kWin:
            return durak::match::ActionType::kWin;
    }
    throw std::runtime_error("unknown move type");
}

inline durak::match::MoveRequest ToEngineMove(const ApiTypes::MoveRequest& request) {
    durak::match::MoveRequest engine_move;
    engine_move.type = ToEngineActionType(request.type);
    if (request.card) {
        engine_move.card = durak::Card{request.card->suit, request.card->power};
    }
    if (request.stack) {
        engine_move.stack = *request.stack;
    }
    return engine_move;
}

inline ApiTypes::Game::Status ToApiGameStatus(durak::match::MatchStatus status) {
    return status == durak::match::MatchStatus::kActive ? ApiTypes::Game::Status::kActive : ApiTypes::Game::Status::kFinished;
}

inline ApiTypes::GameState::Status ToApiGameStateStatus(durak::match::MatchStatus status) {
    return status == durak::match::MatchStatus::kActive ? ApiTypes::GameState::Status::kActive : ApiTypes::GameState::Status::kFinished;
}

inline ApiTypes::GameResult::Status ToApiGameResultStatus(durak::match::MatchStatus status) {
    return status == durak::match::MatchStatus::kActive ? ApiTypes::GameResult::Status::kActive : ApiTypes::GameResult::Status::kFinished;
}

inline ApiTypes::Game MakeGame(const durak::match::MatchInfo& info, const std::optional<boost::uuids::uuid>& match_id = std::nullopt) {
    ApiTypes::Game game;
    game.id = ParseGameId(info.match_id);
    game.matchId = match_id;
    game.status = ToApiGameStatus(info.status);
    game.players = ToApiPlayers(info.players);
    game.trump = info.trump;
    game.createdAt = userver::utils::datetime::TimePointTz{info.created_at};
    if (info.finished_at) {
        game.finishedAt = userver::utils::datetime::TimePointTz{*info.finished_at};
    }
    game.replayId = std::nullopt;  // повторы хранятся в replay-service
    return game;
}

inline ApiTypes::GameState MakeGameState(const durak::match::MatchState& state) {
    ApiTypes::GameState result;
    result.gameId = ParseGameId(state.match_id);
    result.status = ToApiGameStateStatus(state.status);
    result.trump = state.trump;
    result.deckSize = state.deck_size;
    if (state.deck_trump_card) {
        result.deckTrumpCard = ToApiCard(*state.deck_trump_card);
    }
    result.table.reserve(state.table.size());
    for (const auto& stack : state.table) {
        result.table.push_back(ToApiCards(stack));
    }
    result.players = ToApiPlayers(state.players);
    result.currentPlayerId = static_cast<int>(state.current_player_id);
    result.attackerId = static_cast<int>(state.attacker_id);
    result.defenderId = static_cast<int>(state.defender_id);
    result.yourHand = ToApiCards(state.your_hand);
    result.hiddenOpponentCards = state.hidden_opponent_cards;
    result.movesCount = state.moves_count;
    return result;
}

inline ApiTypes::GameResult MakeGameResult(const durak::match::MatchResult& result) {
    ApiTypes::GameResult api_result;
    api_result.gameId = ParseGameId(result.match_id);
    api_result.status = ToApiGameResultStatus(result.status);
    if (result.winner_id) {
        api_result.winnerId = static_cast<int>(*result.winner_id);
    }
    if (result.loser_id) {
        api_result.loserId = static_cast<int>(*result.loser_id);
    }
    if (result.reason) {
        switch (*result.reason) {
            case durak::match::FinishReason::kEmptiedHand:
                api_result.reason = ApiTypes::GameResult::Reason::kEmptiedHand;
                break;
            case durak::match::FinishReason::kResign:
                api_result.reason = ApiTypes::GameResult::Reason::kResign;
                break;
        }
    }
    return api_result;
}

inline ApiTypes::AppliedMove MakeAppliedMove(const durak::match::MatchMove& move) {
    ApiTypes::AppliedMove applied;
    applied.sequence = static_cast<int>(move.sequence);
    applied.playerId = static_cast<int>(move.player_id);
    switch (move.type) {
        case durak::match::ActionType::kCard:
            applied.type = ApiTypes::AppliedMove::Type::kCard;
            break;
        case durak::match::ActionType::kTransfer:
            applied.type = ApiTypes::AppliedMove::Type::kMove;
            break;
        case durak::match::ActionType::kGrab:
            applied.type = ApiTypes::AppliedMove::Type::kGrab;
            break;
        case durak::match::ActionType::kWin:
            applied.type = ApiTypes::AppliedMove::Type::kWin;
            break;
    }
    if (move.card) {
        applied.card = ToApiCard(*move.card);
    }
    if (move.stack) {
        applied.stack = *move.stack;
    }
    applied.at = userver::utils::datetime::TimePointTz{move.at};
    return applied;
}

inline std::string_view ToStringFinishReason(const durak::match::MatchResult& result) {
    if (!result.reason) {
        return "draw";
    }
    return *result.reason == durak::match::FinishReason::kResign ? "resign" : "emptied_hand";
}


inline constexpr std::string_view kChannelGameMove = "game.move";
inline constexpr std::string_view kChannelGameFinished = "game.finished";

inline constexpr std::chrono::seconds kRedisTimeoutSingle{15};
inline constexpr std::chrono::seconds kRedisTimeoutAll{60};
inline constexpr std::size_t kRedisMaxRetries{4};

inline userver::storages::redis::CommandControl RedisCommandControl() {
    return userver::storages::redis::CommandControl{kRedisTimeoutSingle, kRedisTimeoutAll, kRedisMaxRetries};
}

inline std::string BuildMoveEvent(const boost::uuids::uuid& game_id, const durak::match::MatchMove& move, bool match_finished) {
    userver::formats::json::ValueBuilder event(userver::formats::json::Type::kObject);
    event["type"] = "game.move";
    event["gameId"] = userver::utils::ToString(game_id);
    event["playerId"] = static_cast<int>(move.player_id);
    event["movesCount"] = static_cast<int>(move.sequence);
    event["finished"] = match_finished;

    userver::formats::json::ValueBuilder move_json(userver::formats::json::Type::kObject);
    move_json["type"] = ToStringActionType(move.type);
    if (move.card) {
        userver::formats::json::ValueBuilder card(userver::formats::json::Type::kObject);
        card["suit"] = move.card->suit;
        card["power"] = move.card->power;
        move_json["card"] = card.ExtractValue();
    }
    if (move.stack) {
        move_json["stack"] = *move.stack;
    }
    event["move"] = move_json.ExtractValue();

    return userver::formats::json::ToString(event.ExtractValue());
}

inline std::string BuildFinishedEvent(const boost::uuids::uuid& game_id, const durak::match::MatchResult& result) {
    userver::formats::json::ValueBuilder event(userver::formats::json::Type::kObject);
    event["type"] = "game.finished";
    event["gameId"] = userver::utils::ToString(game_id);
    if (result.winner_id) {
        event["winnerId"] = static_cast<int>(*result.winner_id);
    }
    if (result.loser_id) {
        event["loserId"] = static_cast<int>(*result.loser_id);
    }
    event["reason"] = ToStringFinishReason(result);
    return userver::formats::json::ToString(event.ExtractValue());
}

inline void TryPublish(const userver::storages::redis::ClientPtr& client, std::string_view channel, std::string message) {
    try {
        client->Publish(std::string{channel}, std::string{std::move(message)}, RedisCommandControl());
    } catch (const std::exception& e) {
        LOG_WARNING() << "Failed to publish event to Redis channel '" << channel << "': " << e.what();
    }
}

}  // namespace handlers::gameService::common
