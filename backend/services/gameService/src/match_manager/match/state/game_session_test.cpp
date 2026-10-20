#include "game_session.hpp"

#include <set>
#include <userver/utest/utest.hpp>
#include <vector>

namespace {
using durak::Card;
using durak::state::GameSession;
using durak::state::MoveType;

std::vector<Card> MakeDeck(std::size_t count, Card card = {0, 6}) {
    return std::vector<Card>(count, card);
}
}  // namespace

UTEST(GameSession, InitializeGivesCardsAndUpdatesTrump) {
    GameSession session(1);
    for (const auto& hand : session.player_cards) {
        ASSERT_EQ(hand.size(), GameSession::kStartCards);
    }
    ASSERT_EQ(session.deck.size(), 24u);
    EXPECT_EQ(session.trump, session.deck.front().suit);
    EXPECT_EQ(session.curr_player_move, 0);
    EXPECT_EQ(session.curr_player_attacker, 0);
}

UTEST(GameSession, DeckContainsAllUniqueCards) {
    GameSession session(7);
    std::set<std::pair<int, int>> seen;
    for (const auto& hand : session.player_cards) {
        for (const auto& card : hand) {
            seen.insert({card.suit, card.power});
        }
    }
    for (const auto& card : session.deck) {
        seen.insert({card.suit, card.power});
    }
    ASSERT_EQ(seen.size(), 36u);
    for (const int suit : {0, 1, 2, 3}) {
        for (int power = 6; power <= 14; ++power) {
            EXPECT_NE(seen.find({suit, power}), seen.end());
        }
    }
}

UTEST(GameSession, CanBeatRespectsTrump) {
    GameSession session(1);
    session.trump = 1;
    EXPECT_TRUE(session.CanBeat(Card{1, 7}, Card{1, 6}));
    EXPECT_FALSE(session.CanBeat(Card{1, 6}, Card{1, 7}));
    EXPECT_TRUE(session.CanBeat(Card{1, 6}, Card{0, 14}));
    EXPECT_FALSE(session.CanBeat(Card{0, 14}, Card{1, 6}));
    EXPECT_TRUE(session.CanBeat(Card{1, 10}, Card{1, 7}));
    EXPECT_FALSE(session.CanBeat(Card{2, 10}, Card{1, 6}));
}

UTEST(GameSession, CanPushFirstAttackIsFree) {
    GameSession session(1);
    session.player_cards[0] = {Card{2, 7}};
    for (int stack = 0; stack < GameSession::kStacksCount; ++stack) {
        EXPECT_TRUE(session.CanPush(Card{2, 7}, stack));
    }
    EXPECT_FALSE(session.CanPush(Card{2, 7}, -1));
    EXPECT_FALSE(session.CanPush(Card{2, 7}, GameSession::kStacksCount));
}

UTEST(GameSession, CanPushRequiresMatchingRank) {
    GameSession session(1);
    session.game_stack[0] = {Card{2, 7}};
    EXPECT_TRUE(session.CanPush(Card{1, 7}, 1));
    EXPECT_FALSE(session.CanPush(Card{1, 8}, 1));
    EXPECT_FALSE(session.CanPush(Card{2, 6}, 3));
}

UTEST(GameSession, CanPushAttackerSkipsUnevenStack) {
    GameSession session(1);
    session.game_stack[0] = {Card{2, 7}};
    EXPECT_FALSE(session.CanPush(Card{1, 7}, 0));
    EXPECT_TRUE(session.CanPush(Card{3, 7}, 1));
}


UTEST(GameSession, DoMoveTransfersCardToStack) {
    GameSession session(1);
    session.player_cards[0] = {Card{0, 7}, Card{0, 8}};
    session.DoMove(Card{0, 8}, 2);
    EXPECT_EQ(session.player_cards[0], (std::vector<Card>{Card{0, 7}}));
    EXPECT_EQ(session.game_stack[2], (std::vector<Card>{Card{0, 8}}));
}

UTEST(GameSession, GrabCollectsAllStackCardsToMover) {
    GameSession session(1);
    session.curr_player_move = 1;
    session.curr_player_attacker = 0;
    session.player_cards[1] = {};
    session.game_stack[0] = {Card{1, 6}, Card{2, 6}};
    session.game_stack[2] = {Card{3, 7}};
    session.Grab();
    EXPECT_EQ(session.player_cards[1].size(), 3u);
    EXPECT_TRUE(session.AllStacksEmpty());
}

UTEST(GameSession, GiveCardsAfterDefenseRefillsBothHands) {
    GameSession session(1);
    session.player_cards[0] = {Card{0, 7}};
    session.player_cards[1] = {Card{1, 6}, Card{2, 6}};
    session.SetDeck(MakeDeck(10));
    session.game_stack[0] = {Card{0, 7}, Card{1, 6}};
    session.curr_player_move = 0;
    session.curr_player_attacker = 1;
    session.GiveCardsAfterDefense();
    EXPECT_EQ(session.player_cards[0].size(), GameSession::kStartCards);
    EXPECT_EQ(session.player_cards[1].size(), GameSession::kStartCards);
    EXPECT_EQ(session.deck.size(), 1u);
    EXPECT_TRUE(session.AllStacksEmpty());
}

UTEST(GameSession, GiveCardsAfterDefenseStopsOnEmptyDeck) {
    GameSession session(1);
    session.player_cards[0] = {Card{0, 7}};
    session.player_cards[1] = {Card{1, 6}};
    session.SetDeck(MakeDeck(3));
    session.GiveCardsAfterDefense();
    EXPECT_EQ(session.player_cards[0].size(), 4u);
    EXPECT_EQ(session.player_cards[1].size(), 1u);
    EXPECT_TRUE(session.deck.empty());
}

UTEST(GameSession, TransferMoveAfterAttackPassesToDefender) {
    GameSession session(1);
    session.curr_player_move = 0;
    session.curr_player_attacker = 0;
    session.TransferMove(MoveType::kTransfer);
    EXPECT_EQ(session.curr_player_move, 1);
    EXPECT_EQ(session.curr_player_attacker, 0);
}

UTEST(GameSession, TransferMoveAfterDefenseSwapsAttacker) {
    GameSession session(1);
    session.curr_player_move = 1;
    session.curr_player_attacker = 0;
    session.TransferMove(MoveType::kTransfer);
    EXPECT_EQ(session.curr_player_move, 1);
    EXPECT_EQ(session.curr_player_attacker, 1);
}

UTEST(GameSession, TransferMoveAfterGrabAdvancesBoth) {
    GameSession session(1);
    session.curr_player_move = 1;
    session.curr_player_attacker = 0;
    session.TransferMove(MoveType::kGrab);
    EXPECT_EQ(session.curr_player_move, 0);
    EXPECT_EQ(session.curr_player_attacker, 0);
}

UTEST(GameSession, TransferMoveIgnoredWhenGameFinished) {
    GameSession session(1);
    session.player_cards[0] = {};
    session.curr_player_move = 1;
    session.curr_player_attacker = 1;
    session.TransferMove(MoveType::kTransfer);
    EXPECT_EQ(session.curr_player_move, 1);
    EXPECT_EQ(session.curr_player_attacker, 1);
}

UTEST(GameSession, FinishedAndWinnerDetection) {
    GameSession session(1);
    session.player_cards[0] = {};
    session.player_cards[1] = {Card{1, 6}};
    EXPECT_TRUE(session.IsGameFinished());
    EXPECT_EQ(session.GetWinner(), 0);
    session.player_cards[0] = {Card{0, 6}};
    session.player_cards[1] = {};
    EXPECT_TRUE(session.IsGameFinished());
    EXPECT_EQ(session.GetWinner(), 1);
    session.player_cards[1] = {Card{1, 6}};
    EXPECT_FALSE(session.IsGameFinished());
    EXPECT_EQ(session.GetWinner(), -1);
}

UTEST(GameSession, AllCardsBeatenRule) {
    GameSession session(1);
    session.game_stack[0] = {Card{1, 6}, Card{2, 6}};
    session.game_stack[1] = {};
    EXPECT_TRUE(session.AllCardsBeaten());
    session.game_stack[2] = {Card{3, 7}};
    EXPECT_FALSE(session.AllCardsBeaten());
}
