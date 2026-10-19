#pragma once
#include <array>
#include <chrono>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "match_move.hpp"
#include "state/card.hpp"
#include "state/game_session.hpp"

namespace durak::match {
using durak::state::GameSession;
using durak::state::MoveType;

struct Player {
    std::int64_t id = -1;
    std::string name;
};
enum class MatchStatus {
    kActive,
    kFinished
};
enum class FinishReason {
    kEmptiedHand,
    kResign
};
enum class MatchError {
    kOk,
    kMatchNotFound,
    kUnknownPlayer,
    kNotThisPlayersTurn,
    kIllegalMove,
    kMatchAlreadyFinished,
};

struct MoveRequest {
    ActionType type = ActionType::kTransfer;
    std::optional<Card> card;
    std::optional<int> stack;
};

struct PlayerPublic {
    std::int64_t id = -1;
    std::string name;
    int cards_left = 0;
    bool is_attacker = false;
};

struct MatchResult {
    std::string match_id;
    MatchStatus status = MatchStatus::kActive;
    std::optional<FinishReason> reason;
    std::optional<std::int64_t> winner_id;
    std::optional<std::int64_t> loser_id;
};

struct MatchInfo {
    std::string match_id;
    MatchStatus status = MatchStatus::kActive;
    int trump = 0;
    std::vector<PlayerPublic> players;
    std::chrono::system_clock::time_point created_at{};
    std::optional<std::chrono::system_clock::time_point> finished_at;
};

struct MatchState {
    std::string match_id;
    MatchStatus status = MatchStatus::kActive;
    int trump = 0;
    int deck_size = 0;
    std::optional<Card> deck_trump_card;
    std::array<std::vector<Card>, GameSession::kStacksCount> table;
    std::vector<PlayerPublic> players;
    std::int64_t current_player_id = -1;
    std::int64_t attacker_id = -1;
    std::int64_t defender_id = -1;
    std::vector<Card> your_hand;
    int hidden_opponent_cards = 0;
    int moves_count = 0;
};

class Match {
public:
    static constexpr std::size_t kPlayersCount = 2;
    Match(std::string id, std::vector<Player> players, std::optional<std::int64_t> first_attacker_id, unsigned seed);
    MatchError ApplyMove(std::int64_t player_id, const MoveRequest& move);
    MatchError Resign(std::int64_t player_id);

    bool IsFinished() const { return status_ == MatchStatus::kFinished; }

    const std::string& GetId() const { return id_; }

    const std::vector<Player>& GetPlayers() const { return players_; }

    const std::vector<MatchMove>& GetHistory() const { return history_; }

    MatchInfo GetInfo() const;
    std::optional<MatchState> GetState(std::int64_t viewer_id, MatchError& error) const;
    MatchResult GetResult() const;
    std::string GetReplayText() const;
    GameSession session;
private:
    int SeatOf(std::int64_t player_id) const;
    MatchError ApplyCardMove(int seat, std::int64_t player_id, const MoveRequest& move);
    MatchError ApplyTransferMove(int seat, std::int64_t player_id);
    MatchError ApplyGrabMove(int seat, std::int64_t player_id);
    MatchError ApplyWinMove(int seat, std::int64_t player_id);
    void Record(ActionType type, std::int64_t player_id, std::optional<Card> card, std::optional<int> stack, std::optional<int> card_num);
    void FinishMatch(FinishReason reason, int winner_seat);
    void BuildReplayHeader();
    std::string id_;
    std::vector<Player> players_;
    MatchStatus status_ = MatchStatus::kActive;
    std::optional<FinishReason> finish_reason_;
    std::optional<std::int64_t> winner_id_;
    std::optional<std::int64_t> loser_id_;
    std::vector<MatchMove> history_;
    std::vector<Card> initial_deck_;
    std::vector<Card> initial_hand0_;
    std::vector<Card> initial_hand1_;
    std::chrono::system_clock::time_point created_at_;
    std::optional<std::chrono::system_clock::time_point> finished_at_;
    std::string replay_header_;
};
}  // namespace durak::match
