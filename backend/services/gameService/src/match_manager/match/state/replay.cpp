#include "replay.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>

namespace durak::state {
namespace {
std::vector<std::string> SplitLines(const std::string& text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char c : text) {
        if (c == '\n' || c == '\r') {
            if (!current.empty()) {
                lines.push_back(current);
            }
            current.clear();
        } else {
            current.push_back(c);
        }
    }
    if (!current.empty()) {
        lines.push_back(current);
    }
    return lines;
}

std::string Trim(const std::string& value) {
    std::size_t begin = 0;
    std::size_t end = value.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(value[begin]))) {
        ++begin;
    }
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::vector<Card> ParseCards(const std::string& cards_string) {
    std::vector<Card> cards;
    std::size_t begin = 0;
    while (begin <= cards_string.size()) {
        const std::size_t end = cards_string.find(';', begin);
        const std::string entry = cards_string.substr(begin, end == std::string::npos ? std::string::npos : end - begin);
        if (!entry.empty()) {
            const std::size_t comma = entry.find(',');
            if (comma != std::string::npos) {
                const std::string suit_str = Trim(entry.substr(0, comma));
                const std::string power_str = Trim(entry.substr(comma + 1));
                Card card;
                card.suit = std::atoi(suit_str.c_str());
                card.power = std::atoi(power_str.c_str());
                if (suit_str.find_first_not_of("0123456789") == std::string::npos &&
                    power_str.find_first_not_of("0123456789") == std::string::npos) {
                    cards.push_back(card);
                }
            }
        }
        if (end == std::string::npos) {
            break;
        }
        begin = end + 1;
    }
    return cards;
}
}  // namespace

Replay::Replay(GameSession& session) : session_(session) {}

void Replay::SetReplayData(const std::string& replay_text) {
    moves_.clear();
    snapshots_.clear();
    curr_move_ = 0;
    player0_name_.clear();
    player1_name_.clear();
    const auto lines = SplitLines(replay_text);
    if (lines.size() >= 2) {
        player0_name_ = Trim(lines[0]);
        player1_name_ = Trim(lines[1]);
    }
    if (lines.size() < 3) {
        return;
    }
    const std::string& state_line = lines[2];
    const std::size_t player0_pos = state_line.find("PLAYER0:");
    const std::size_t player1_pos = state_line.find("PLAYER1:");
    const std::size_t deck_pos = state_line.find("DECK:");
    std::vector<Card> player0_cards;
    std::vector<Card> player1_cards;
    std::vector<Card> deck_cards;
    if (player0_pos != std::string::npos) {
        const std::size_t end = player1_pos == std::string::npos ? state_line.size() : player1_pos;
        player0_cards = ParseCards(state_line.substr(player0_pos + 8, end - player0_pos - 8));
    }
    if (player1_pos != std::string::npos) {
        const std::size_t end = deck_pos == std::string::npos ? state_line.size() : deck_pos;
        player1_cards = ParseCards(state_line.substr(player1_pos + 8, end - player1_pos - 8));
    }
    if (deck_pos != std::string::npos) {
        deck_cards = ParseCards(state_line.substr(deck_pos + 5));
    }
    {
        std::reverse(deck_cards.begin(), deck_cards.end());
        session_.SetDeck(std::move(deck_cards));
        session_.SetPlayersCards(std::move(player0_cards), std::move(player1_cards));
        if (!session_.deck.empty()) {
            session_.trump = session_.deck.front().suit;
        }
        session_.curr_player_move = 0;
        session_.curr_player_attacker = 0;
    }
    std::vector<Move> moves;
    for (std::size_t i = 3; i < lines.size(); i += 3) {
        if (i + 2 >= lines.size()) {
            break;
        }
        const std::string move_line = Trim(lines[i + 2]);
        Move move;
        if (move_line.rfind("CARD:", 0) == 0) {
            std::size_t begin = 0;
            for (int part = 0; part < 2; ++part) {
                const std::size_t end = move_line.find(';', begin);
                if (end == std::string::npos) {
                    break;
                }
                const std::string value = Trim(move_line.substr(begin, end - begin));
                const std::size_t colon = value.find(':');
                if (colon != std::string::npos) {
                    const std::string number = value.substr(colon + 1);
                    if (part == 0) {
                        move.card_num = std::atoi(number.c_str());
                    } else {
                        move.stack_num = std::atoi(number.c_str());
                    }
                }
                begin = end + 1;
            }
            move.type = MoveType::kTransfer;
        } else if (move_line == "MOVE") {
            move.type = MoveType::kTransfer;
        } else if (move_line == "GRAB") {
            move.type = MoveType::kGrab;
        } else if (move_line == "WIN") {
            move.type = MoveType::kWin;
        } else {
            continue;
        }
        moves.push_back(move);
        if (move.type == MoveType::kWin) {
            break;
        }
    }
    moves_ = std::move(moves);
}

std::optional<std::string> Replay::DoMove() {
    if (curr_move_ == moves_.size()) {
        return std::nullopt;
    }
    MoveSnapshot snapshot;
    snapshot.player_cards = session_.player_cards;
    snapshot.game_stack = session_.game_stack;
    snapshot.deck = session_.deck;
    const Move& move = moves_[curr_move_];
    std::string message;
    if (move.type == MoveType::kTransfer && move.card_num != -1) {
        const Card card = session_.player_cards[session_.curr_player_move][move.card_num];
        session_.DoMove(card, move.stack_num);
        ++curr_move_;
        message = (session_.curr_player_move == 0 ? player0_name_ : player1_name_) + " ходит " + GetCardName(card);
        snapshot.message = message;
        snapshots_.push_back(std::move(snapshot));
        return message;
    }
    snapshots_.push_back(std::move(snapshot));
    if (move.type == MoveType::kTransfer) {
        if (session_.curr_player_move != session_.curr_player_attacker) {
            session_.GiveCardsAfterDefense();
        }
        session_.TransferMove(MoveType::kTransfer);
        ++curr_move_;
        message = "Ход передан игроку " + (session_.curr_player_move == 0 ? player0_name_ : player1_name_);
        return message;
    }
    if (move.type == MoveType::kGrab) {
        session_.Grab();
        session_.GiveCardsAfterDefense();
        session_.TransferMove(MoveType::kGrab);
        ++curr_move_;
        message = "Игрок " + (session_.curr_player_move == 1 ? player0_name_ : player1_name_) + " гребёт";
        return message;
    }
    return "Игрок " + (session_.curr_player_move == 0 ? player0_name_ : player1_name_) + " побеждает";
}

std::optional<std::string> Replay::UndoMove() {
    if (curr_move_ == 0) {
        return std::nullopt;
    }
    --curr_move_;
    MoveSnapshot snapshot = std::move(snapshots_.back());
    snapshots_.pop_back();
    session_.player_cards = snapshot.player_cards;
    session_.game_stack = snapshot.game_stack;
    session_.deck = snapshot.deck;
    const Move& move = moves_[curr_move_];
    if (move.type == MoveType::kTransfer) {
        if (move.card_num != -1) {
            std::reverse(session_.deck.begin(), session_.deck.end());
            return snapshot.message;
        }
        session_.UndoTransferMove(MoveType::kTransfer);
        return "Ход передан игроку " + (session_.curr_player_move == 0 ? player0_name_ : player1_name_);
    }
    session_.UndoTransferMove(MoveType::kGrab);
    return "Игрок " + (session_.curr_player_move == 1 ? player0_name_ : player1_name_) + " гребёт";
}

std::string Replay::GetCardName(const Card& card) {
    static const char* kSuitNames[] = {"бубен", "пик", "червей", "крестей"};
    static const char* kRankNames[] = {"шестёркой", "семёркой", "восьмёркой", "девяткой", "десяткой",
                                       "валетом",   "дамой",    "королёи",    "тузом"};
    const char* suit_name = card.suit >= 0 && card.suit <= 3 ? kSuitNames[card.suit] : nullptr;
    const char* rank_name = card.power >= 6 && card.power <= 14 ? kRankNames[card.power - 6] : nullptr;
    if (suit_name == nullptr || rank_name == nullptr) {
        return {};
    }
    return std::string(rank_name) + " " + suit_name;
}
}  // namespace durak::state
