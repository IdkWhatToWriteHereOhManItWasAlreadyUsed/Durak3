#include "game_session.hpp"

#include <algorithm>
#include <random>

namespace durak::state {
namespace {
constexpr int kMinPower = 6;
constexpr int kMaxPower = 14;
}  // namespace

GameSession::GameSession() : GameSession(std::random_device{}()) {}

GameSession::GameSession(unsigned seed) {
    InitializeDeck(seed);
}

void GameSession::InitializeDeck(unsigned seed) {
    std::vector<Card> cards;
    cards.reserve(static_cast<std::size_t>(kPlayersCount) * kStacksCount * (kMaxPower - kMinPower + 1));
    for (const int suit : {1, 2, 3, 0}) {
        for (int power = kMinPower; power <= kMaxPower; ++power) {
            cards.push_back(Card{suit, power});
        }
    }
    std::mt19937 rng(seed);
    std::shuffle(cards.begin(), cards.end(), rng);
    deck = std::move(cards);
    GiveCards();
}

void GameSession::GiveCards() {
    for (auto& hand : player_cards) {
        hand.clear();
    }
    for (int round = 0; round < kStartCards; ++round) {
        for (auto& hand : player_cards) {
            hand.push_back(deck.back());
            deck.pop_back();
        }
        trump = deck.front().suit;
    }
    curr_player_move = 0;
    curr_player_attacker = 0;
}

bool GameSession::CanBeat(const Card& attack_card, const Card& target_card) const {
    if (attack_card.suit == trump) {
        if (target_card.suit == trump) {
            return attack_card.power > target_card.power;
        }
        return true;
    }
    if (attack_card.suit != target_card.suit) {
        return false;
    }
    return attack_card.power > target_card.power;
}

bool GameSession::CanPush(const Card& card, int stack_index) const {
    if (stack_index < 0 || stack_index >= kStacksCount) {
        return false;
    }
    const auto& stack = game_stack[stack_index];
    if (curr_player_attacker == curr_player_move) {
        if (stack.size() % 2 != 0) {
            return false;
        }
        for (const auto& curr_stack : game_stack) {
            for (const auto& stack_card : curr_stack) {
                if (stack_card.power == card.power) {
                    return true;
                }
            }
        }
        for (const auto& curr_stack : game_stack) {
            if (!curr_stack.empty()) {
                return false;
            }
        }
        return true;
    }
    if (stack.size() % 2 != 1) {
        return false;
    }
    return CanBeat(card, stack.back());
}

void GameSession::DoMove(const Card& card, int stack_index) {
    auto& hand = player_cards[curr_player_move];
    auto it = std::find(hand.begin(), hand.end(), card);
    if (it != hand.end()) {
        hand.erase(it);
    }
    game_stack[stack_index].push_back(card);
}

void GameSession::Grab() {
    for (auto& stack : game_stack) {
        while (!stack.empty()) {
            player_cards[curr_player_move].push_back(stack.back());
            stack.pop_back();
        }
    }
}

void GameSession::UndoGrab() {
    Grab();
}

void GameSession::GiveCardsAfterDefense() {
    for (int i = 0; i < 12; ++i) {
        if (!deck.empty() && player_cards[curr_player_move].size() < kStartCards) {
            player_cards[curr_player_move].push_back(deck.back());
            deck.pop_back();
        }
        if (!deck.empty() && player_cards[curr_player_attacker].size() < kStartCards) {
            player_cards[curr_player_attacker].push_back(deck.back());
            deck.pop_back();
        }
    }
    for (auto& stack : game_stack) {
        stack.clear();
    }
}

void GameSession::UndoGiveCardsAfterDefense() {
    for (int i = 0; i < 12; ++i) {
        if (!deck.empty() && player_cards[curr_player_move].size() < kStartCards) {
            player_cards[curr_player_move].push_back(deck.back());
            deck.pop_back();
        }
        if (!deck.empty() && player_cards[curr_player_attacker].size() < kStartCards) {
            player_cards[curr_player_attacker].push_back(deck.back());
            deck.pop_back();
        }
    }
    for (auto& stack : game_stack) {
        stack.clear();
    }
}

bool GameSession::AllCardsBeaten() const {
    for (const auto& stack : game_stack) {
        if (stack.size() % 2 != 0) {
            return false;
        }
    }
    return true;
}

bool GameSession::AllStacksEmpty() const {
    for (const auto& stack : game_stack) {
        if (!stack.empty()) {
            return false;
        }
    }
    return true;
}

int GameSession::GetWinner() const {
    for (int i = 0; i < kPlayersCount; ++i) {
        if (player_cards[i].empty()) {
            return i;
        }
    }
    return -1;
}

void GameSession::TransferMove(MoveType move_type) {
    if (IsGameFinished()) {
        return;
    }
    if (move_type == MoveType::kTransfer) {
        if (curr_player_move == curr_player_attacker) {
            curr_player_move = curr_player_move < kPlayersCount - 1 ? curr_player_move + 1 : 0;
        } else {
            curr_player_attacker = curr_player_move;
        }
    } else {
        curr_player_move = curr_player_move < kPlayersCount - 1 ? curr_player_move + 1 : 0;
        curr_player_attacker = curr_player_move;
    }
}

void GameSession::UndoTransferMove(MoveType move_type) {
    if (IsGameFinished()) {
        return;
    }
    if (move_type == MoveType::kTransfer) {
        if (curr_player_move == curr_player_attacker) {
            curr_player_attacker = curr_player_attacker < kPlayersCount - 1 ? curr_player_attacker + 1 : 0;
        } else {
            curr_player_move = curr_player_attacker;
        }
    } else {
        curr_player_move = curr_player_move < kPlayersCount - 1 ? curr_player_move + 1 : 0;
    }
}

bool GameSession::IsGameFinished() const {
    for (const auto& hand : player_cards) {
        if (hand.empty()) {
            return true;
        }
    }
    return false;
}

void GameSession::SetDeck(std::vector<Card> new_deck) {
    deck = std::move(new_deck);
}

void GameSession::SetPlayersCards(std::vector<Card> first, std::vector<Card> second) {
    player_cards[0] = std::move(first);
    player_cards[1] = std::move(second);
}
}  // namespace durak::state
