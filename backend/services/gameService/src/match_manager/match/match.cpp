#include "match.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace durak::match {
Match::Match(std::string id, std::vector<Player> players, std::optional<std::int64_t> first_attacker_id, unsigned seed)
    : id_(std::move(id)), players_(std::move(players)), session(seed) {
    if (players_.size() != kPlayersCount) {
        throw std::invalid_argument("match requires exactly 2 players");
    }
    created_at_ = std::chrono::system_clock::now();
    if (first_attacker_id) {
        const int seat = SeatOf(*first_attacker_id);
        if (seat >= 0) {
            session.curr_player_move = seat;
            session.curr_player_attacker = seat;
        }
    }
    initial_deck_ = session.deck;
    initial_hand0_ = session.player_cards[0];
    initial_hand1_ = session.player_cards[1];
    BuildReplayHeader();
}

int Match::SeatOf(std::int64_t player_id) const {
    for (int seat = 0; seat < static_cast<int>(players_.size()); ++seat) {
        if (players_[seat].id == player_id) {
            return seat;
        }
    }
    return -1;
}

void Match::Record(
    ActionType type, std::int64_t player_id, std::optional<Card> card, std::optional<int> stack, std::optional<int> card_num) {
    MatchMove entry;
    entry.sequence = history_.size() + 1;
    entry.player_id = player_id;
    entry.type = type;
    entry.card = card;
    entry.stack = stack;
    entry.card_num = card_num;
    entry.at = std::chrono::system_clock::now();
    history_.push_back(std::move(entry));
}

void Match::FinishMatch(FinishReason reason, int winner_seat) {
    status_ = MatchStatus::kFinished;
    finished_at_ = std::chrono::system_clock::now();
    finish_reason_ = reason;
    winner_id_ = players_[winner_seat].id;
    loser_id_ = players_[1 - winner_seat].id;
}

MatchError Match::ApplyMove(std::int64_t player_id, const MoveRequest& move) {
    if (status_ == MatchStatus::kFinished) {
        if (move.type == ActionType::kWin) {
            return MatchError::kOk;
        }
        return MatchError::kMatchAlreadyFinished;
    }
    const int seat = SeatOf(player_id);
    if (seat < 0) {
        return MatchError::kUnknownPlayer;
    }
    if (seat != session.curr_player_move) {
        return MatchError::kNotThisPlayersTurn;
    }
    switch (move.type) {
        case ActionType::kCard:
            return ApplyCardMove(seat, player_id, move);
        case ActionType::kTransfer:
            return ApplyTransferMove(seat, player_id);
        case ActionType::kGrab:
            return ApplyGrabMove(seat, player_id);
        case ActionType::kWin:
            return ApplyWinMove(seat, player_id);
    }
    return MatchError::kIllegalMove;
}

MatchError Match::ApplyCardMove(int seat, std::int64_t player_id, const MoveRequest& move) {
    if (!move.card || !move.stack || *move.stack < 0 || *move.stack >= GameSession::kStacksCount) {
        return MatchError::kIllegalMove;
    }
    const Card& card = *move.card;
    const auto& hand = session.player_cards[seat];
    const auto found = std::find(hand.begin(), hand.end(), card);
    if (found == hand.end()) {
        return MatchError::kIllegalMove;
    }
    const int card_num = static_cast<int>(found - hand.begin());
    if (!session.CanPush(card, *move.stack)) {
        return MatchError::kIllegalMove;
    }
    session.DoMove(card, *move.stack);
    Record(ActionType::kCard, player_id, card, *move.stack, card_num);
    if (session.IsGameFinished()) {
        FinishMatch(FinishReason::kEmptiedHand, session.GetWinner());
    }
    return MatchError::kOk;
}

MatchError Match::ApplyTransferMove(int seat, std::int64_t player_id) {
    if (session.curr_player_attacker == seat) {
        if (session.AllStacksEmpty()) {
            return MatchError::kIllegalMove;
        }
        session.TransferMove(MoveType::kTransfer);
    } else {
        if (!session.AllCardsBeaten()) {
            return MatchError::kIllegalMove;
        }
        session.GiveCardsAfterDefense();
        session.TransferMove(MoveType::kTransfer);
    }
    Record(ActionType::kTransfer, player_id, std::nullopt, std::nullopt, std::nullopt);
    if (session.IsGameFinished()) {
        const int winner = session.GetWinner();
        if (winner >= 0) {
            FinishMatch(FinishReason::kEmptiedHand, winner);
        }
    }
    return MatchError::kOk;
}

MatchError Match::ApplyGrabMove(int seat, std::int64_t player_id) {
    if (session.curr_player_attacker == seat) {
        return MatchError::kIllegalMove;
    }
    session.Grab();
    session.GiveCardsAfterDefense();
    session.TransferMove(MoveType::kGrab);
    Record(ActionType::kGrab, player_id, std::nullopt, std::nullopt, std::nullopt);
    if (session.IsGameFinished()) {
        const int winner = session.GetWinner();
        if (winner >= 0) {
            FinishMatch(FinishReason::kEmptiedHand, winner);
        }
    }
    return MatchError::kOk;
}

MatchError Match::ApplyWinMove(int /*seat*/, std::int64_t player_id) {
    if (!session.IsGameFinished()) {
        return MatchError::kIllegalMove;
    }
    const int winner = session.GetWinner();
    if (winner < 0) {
        return MatchError::kIllegalMove;
    }
    Record(ActionType::kWin, player_id, std::nullopt, std::nullopt, std::nullopt);
    FinishMatch(FinishReason::kEmptiedHand, winner);
    return MatchError::kOk;
}

MatchError Match::Resign(std::int64_t player_id) {
    if (status_ == MatchStatus::kFinished) {
        return MatchError::kMatchAlreadyFinished;
    }
    const int seat = SeatOf(player_id);
    if (seat < 0) {
        return MatchError::kUnknownPlayer;
    }
    Record(ActionType::kWin, player_id, std::nullopt, std::nullopt, std::nullopt);
    FinishMatch(FinishReason::kResign, 1 - seat);
    return MatchError::kOk;
}

std::vector<PlayerPublic> Match::BuildPlayersView() const {
    std::vector<PlayerPublic> view;
    view.reserve(players_.size());
    for (std::size_t seat = 0; seat < players_.size(); ++seat) {
        PlayerPublic player;
        player.id = players_[seat].id;
        player.name = players_[seat].name;
        player.cards_left = static_cast<int>(session.player_cards[seat].size());
        player.is_attacker = session.curr_player_attacker == static_cast<int>(seat);
        view.push_back(std::move(player));
    }
    return view;
}

MatchInfo Match::GetInfo() const {
    MatchInfo info;
    info.match_id = id_;
    info.status = status_;
    info.trump = session.trump;
    info.players = BuildPlayersView();
    info.created_at = created_at_;
    info.finished_at = finished_at_;
    return info;
}

std::optional<MatchState> Match::GetState(std::int64_t viewer_id, MatchError& error) const {
    error = MatchError::kOk;
    const int seat = SeatOf(viewer_id);
    if (seat < 0) {
        error = MatchError::kUnknownPlayer;
        return std::nullopt;
    }
    MatchState state;
    state.match_id = id_;
    state.status = status_;
    state.trump = session.trump;
    state.deck_size = static_cast<int>(session.deck.size());
    if (!session.deck.empty()) {
        state.deck_trump_card = session.deck.front();
    }
    state.table = session.game_stack;
    state.players = BuildPlayersView();
    state.current_player_id = players_[session.curr_player_move].id;
    state.attacker_id = players_[session.curr_player_attacker].id;
    state.defender_id = players_[1 - session.curr_player_attacker].id;
    state.your_hand = session.player_cards[seat];
    state.hidden_opponent_cards = static_cast<int>(session.player_cards[1 - seat].size());
    state.moves_count = static_cast<int>(history_.size());
    return state;
}

MatchResult Match::GetResult() const {
    MatchResult result;
    result.match_id = id_;
    result.status = status_;
    result.reason = finish_reason_;
    result.winner_id = winner_id_;
    result.loser_id = loser_id_;
    return result;
}

void Match::BuildReplayHeader() {
    std::string header = "PLAYER0:";
    for (const auto& card : initial_hand0_) {
        header += CardToString(card);
    }
    header += "PLAYER1:";
    for (const auto& card : initial_hand1_) {
        header += CardToString(card);
    }
    header += "DECK:";
    for (auto it = initial_deck_.rbegin(); it != initial_deck_.rend(); ++it) {
        header += CardToString(*it);
    }
    replay_header_ = header;
}

std::string Match::GetReplayText() const {
    std::string text;
    text += players_[0].name;
    text += "\n";
    text += players_[1].name;
    text += "\n";
    text += replay_header_;
    text += "\n";
    for (const auto& move : history_) {
        text += "Ход " + std::to_string(move.sequence) + " игрока " + std::to_string(move.player_id) + "\n";
        text += "-\n";
        switch (move.type) {
            case ActionType::kCard:
                text += "CARD:" + std::to_string(move.card_num.value_or(-1)) + ";STACK:" + std::to_string(move.stack.value_or(-1)) + "\n";
                break;
            case ActionType::kTransfer:
                text += "MOVE\n";
                break;
            case ActionType::kGrab:
                text += "GRAB\n";
                break;
            case ActionType::kWin:
                text += "WIN\n";
                break;
        }
    }
    return text;
}
}  // namespace durak::match
