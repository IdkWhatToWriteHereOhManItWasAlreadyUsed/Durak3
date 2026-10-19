#pragma once
#include <array>
#include <optional>
#include <string>
#include <vector>

#include "game_session.hpp"

namespace durak::state {
struct MoveSnapshot {
    std::array<std::vector<Card>, GameSession::kPlayersCount> player_cards;
    std::array<std::vector<Card>, GameSession::kStacksCount> game_stack;
    std::vector<Card> deck;
    std::string message;
};

class Replay {
public:
    explicit Replay(GameSession& session);
    void SetReplayData(const std::string& replay_text);

    const std::string& GetPlayer0Name() const { return player0_name_; }

    const std::string& GetPlayer1Name() const { return player1_name_; }

    const std::vector<Move>& GetMoves() const { return moves_; }

    std::optional<std::string> DoMove();
    std::optional<std::string> UndoMove();
    static std::string GetCardName(const Card& card);
private:
    GameSession& session_;
    std::vector<Move> moves_;
    std::vector<MoveSnapshot> snapshots_;
    std::size_t curr_move_ = 0;
    std::string player0_name_;
    std::string player1_name_;
};
}  // namespace durak::state
