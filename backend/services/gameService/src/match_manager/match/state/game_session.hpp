#pragma once
#include <array>
#include <vector>

#include "card.hpp"
#include "move.hpp"

namespace durak::state {
class GameSession {
public:
    static constexpr int kPlayersCount = 2;
    static constexpr int kStacksCount = 4;
    static constexpr int kStartCards = 6;
    GameSession();
    explicit GameSession(unsigned seed);
    std::array<std::vector<Card>, kPlayersCount> player_cards;
    std::vector<Card> deck;
    std::array<std::vector<Card>, kStacksCount> game_stack;
    int trump = 0;
    int curr_player_move = 0;
    int curr_player_attacker = 0;
    bool CanPush(const Card& card, int stack_index) const;
    bool CanBeat(const Card& attack_card, const Card& target_card) const;
    void DoMove(const Card& card, int stack_index);
    void Grab();
    void GiveCardsAfterDefense();
    void TransferMove(MoveType move_type);
    void UndoTransferMove(MoveType move_type);
    void UndoGrab();
    void UndoGiveCardsAfterDefense();
    bool AllCardsBeaten() const;
    bool AllStacksEmpty() const;
    bool IsGameFinished() const;
    int GetWinner() const;
    void SetDeck(std::vector<Card> deck);
    void SetPlayersCards(std::vector<Card> first, std::vector<Card> second);
private:
    void InitializeDeck(unsigned seed);
    void GiveCards();
};
}  // namespace durak::state
