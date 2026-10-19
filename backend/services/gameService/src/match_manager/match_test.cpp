#include <userver/utest/utest.hpp>
#include <utility>
#include "match/state/game_session.hpp"
#include "match/state/replay.hpp"
#include "match/match.hpp"
#include "match_manager.hpp"
namespace {
using durak::Card;
using durak::match::ActionType;
using durak::match::FinishReason;
using durak::match::Match;
using durak::match::MatchManager;
using durak::match::MatchError;
using durak::match::MoveRequest;
using durak::match::Player;
Player MakePlayer(std::int64_t id, std::string name) {
    return Player{id, std::move(name)};
}
}
UTEST(MatchManager, CreateReturnsIdAndInfo) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")});
    ASSERT_TRUE(match_id.has_value());
    EXPECT_EQ(manager.GetMatchCount(), 1u);
    const auto info = manager.GetMatch(*match_id);
    ASSERT_TRUE(info.has_value());
    EXPECT_EQ(info->match_id, *match_id);
    EXPECT_EQ(info->status, durak::match::MatchStatus::kActive);
    ASSERT_EQ(info->players.size(), 2u);
    EXPECT_EQ(info->players[0].id, 1);
    EXPECT_EQ(info->players[1].id, 2);
    EXPECT_EQ(info->players[0].name, "Alice");
}
UTEST(MatchManager, CreateWithExplicitFirstAttacker) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, 2);
    ASSERT_TRUE(match_id.has_value());
    auto state = manager.GetMatchState(*match_id, 1);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->current_player_id, 2);
    EXPECT_EQ(state->attacker_id, 2);
    EXPECT_EQ(state->defender_id, 1);
}
UTEST(MatchManager, CreateRejectsBadPlayers) {
    MatchManager manager(42);
    EXPECT_FALSE(manager.CreateMatch({MakePlayer(1, "solo")}).has_value());
    EXPECT_FALSE(
        manager.CreateMatch({MakePlayer(1, "a"), MakePlayer(2, "b"), MakePlayer(3, "c")})
            .has_value());
    EXPECT_FALSE(manager.CreateMatch({MakePlayer(1, "a"), MakePlayer(1, "dup")}).has_value());
    EXPECT_EQ(manager.GetMatchCount(), 0u);
}
UTEST(MatchManager, UnknownMatch) {
    MatchManager manager(42);
    EXPECT_FALSE(manager.GetMatch("no-such-match").has_value());
    EXPECT_EQ(manager.ApplyMove("no-such-match", 1, MoveRequest{}), MatchError::kMatchNotFound);
    MatchError error = MatchError::kOk;
    EXPECT_FALSE(manager.GetMatchHistory("no-such-match", &error).has_value());
    EXPECT_EQ(error, MatchError::kMatchNotFound);
    EXPECT_FALSE(manager.GetMatchResult("no-such-match", &error).has_value());
    EXPECT_EQ(error, MatchError::kMatchNotFound);
    EXPECT_EQ(manager.Resign("no-such-match", 1), MatchError::kMatchNotFound);
}
UTEST(MatchManager, GetMatchStateViewersPerspective) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")});
    ASSERT_TRUE(match_id.has_value());
    auto state = manager.GetMatchState(*match_id, 1);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(state->deck_size, 24);
    EXPECT_EQ(state->your_hand.size(), 6u);
    EXPECT_EQ(state->hidden_opponent_cards, 6);
    EXPECT_EQ(state->current_player_id, 1);
    EXPECT_TRUE(state->table[0].empty());
    EXPECT_EQ(state->match_id, *match_id);
    auto opponent_state = manager.GetMatchState(*match_id, 2);
    ASSERT_TRUE(opponent_state.has_value());
    EXPECT_EQ(opponent_state->your_hand.size(), 6u);
    MatchError error = MatchError::kOk;
    EXPECT_FALSE(manager.GetMatchState(*match_id, 999, &error).has_value());
    EXPECT_EQ(error, MatchError::kUnknownPlayer);
}
UTEST(MatchManager, MoveTurnOrderEnforced) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")});
    ASSERT_TRUE(match_id.has_value());
    EXPECT_EQ(manager.ApplyMove(*match_id, 2, MoveRequest{}), MatchError::kNotThisPlayersTurn);
    EXPECT_EQ(manager.ApplyMove(*match_id, 999, MoveRequest{}), MatchError::kUnknownPlayer);
}
UTEST(MatchManager, LegalMoveFlowWithHistory) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")});
    ASSERT_TRUE(match_id.has_value());
    auto state = manager.GetMatchState(*match_id, 1);
    ASSERT_TRUE(state.has_value());
    const Card attack_card = state->your_hand.front();
    durak::match::MatchMove applied;
    EXPECT_EQ(
        manager.ApplyMove(*match_id, 1, MoveRequest{ActionType::kCard, attack_card, 0}, &applied),
        MatchError::kOk);
    EXPECT_EQ(applied.player_id, 1);
    EXPECT_EQ(applied.type, ActionType::kCard);
    EXPECT_EQ(applied.stack, 0);
    EXPECT_EQ(applied.sequence, 1u);
    EXPECT_EQ(applied.card, attack_card);
    EXPECT_EQ(manager.ApplyMove(*match_id, 1, MoveRequest{ActionType::kTransfer}),
              MatchError::kOk);
    EXPECT_EQ(manager.ApplyMove(*match_id, 1, MoveRequest{ActionType::kCard, attack_card, 0}),
              MatchError::kIllegalMove);
    EXPECT_EQ(manager.ApplyMove(*match_id, 2, MoveRequest{ActionType::kTransfer}),
              MatchError::kIllegalMove);
    EXPECT_EQ(manager.ApplyMove(*match_id, 2, MoveRequest{ActionType::kGrab}), MatchError::kOk);
    MatchError error = MatchError::kOk;
    const auto history = manager.GetMatchHistory(*match_id, &error);
    ASSERT_TRUE(history.has_value());
    ASSERT_EQ(history->size(), 3u);
    EXPECT_EQ(history->at(0).type, ActionType::kCard);
    EXPECT_EQ(history->at(1).type, ActionType::kTransfer);
    EXPECT_EQ(history->at(2).type, ActionType::kGrab);
    EXPECT_EQ(history->at(2).player_id, 2);
    EXPECT_EQ(history->at(2).sequence, 3u);
    EXPECT_EQ(error, MatchError::kOk);
    auto after_grab = manager.GetMatchState(*match_id, 1);
    ASSERT_TRUE(after_grab.has_value());
    EXPECT_EQ(after_grab->attacker_id, 1);
    EXPECT_TRUE(after_grab->table[0].empty());
}
UTEST(MatchManager, ReplayTextIsCompatibleWithStateReplay) {
    MatchManager manager(42);
    const auto match_id = manager.CreateMatch({MakePlayer(1, "Alice"), MakePlayer(2, "Bob")});
    ASSERT_TRUE(match_id.has_value());
    auto state = manager.GetMatchState(*match_id, 1);
    ASSERT_TRUE(state.has_value());
    const Card attack_card = state->your_hand.front();
    EXPECT_EQ(manager.ApplyMove(*match_id, 1, MoveRequest{ActionType::kCard, attack_card, 0}),
              MatchError::kOk);
    EXPECT_EQ(manager.ApplyMove(*match_id, 1, MoveRequest{ActionType::kTransfer}),
              MatchError::kOk);
    EXPECT_EQ(manager.ApplyMove(*match_id, 2, MoveRequest{ActionType::kGrab}), MatchError::kOk);
    MatchError error = MatchError::kOk;
    const auto replay = manager.GetReplayText(*match_id, &error);
    ASSERT_TRUE(replay.has_value());
    EXPECT_EQ(error, MatchError::kOk);
    const std::string& text = *replay;
    EXPECT_NE(text.find("Alice\n"), std::string::npos);
    EXPECT_NE(text.find("Bob\n"), std::string::npos);
    EXPECT_NE(text.find("PLAYER0:"), std::string::npos);
    EXPECT_NE(text.find("PLAYER1:"), std::string::npos);
    EXPECT_NE(text.find("DECK:"), std::string::npos);
    EXPECT_NE(text.find("MOVE\n"), std::string::npos);
    EXPECT_NE(text.find("GRAB\n"), std::string::npos);
    durak::state::GameSession session(1);
    durak::state::Replay playback(session);
    playback.SetReplayData(text);
    EXPECT_EQ(playback.GetPlayer0Name(), "Alice");
    EXPECT_EQ(playback.GetPlayer1Name(), "Bob");
    ASSERT_EQ(playback.GetMoves().size(), 3u);
    EXPECT_EQ(playback.GetMoves().back().type, durak::state::MoveType::kGrab);
    EXPECT_TRUE(playback.DoMove().has_value());
}
UTEST(Match, WinConfirmsEmptyHand) {
    Match match("m-win", {MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, 2, 1);
    match.session.SetPlayersCards({}, {Card{1, 6}});
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kWin}), MatchError::kOk);
    EXPECT_TRUE(match.IsFinished());
    const auto outcome = match.GetResult();
    EXPECT_EQ(outcome.status, durak::match::MatchStatus::kFinished);
    ASSERT_TRUE(outcome.reason.has_value());
    EXPECT_EQ(*outcome.reason, FinishReason::kEmptiedHand);
    EXPECT_EQ(outcome.winner_id, 1);
    EXPECT_EQ(outcome.loser_id, 2);
    const std::size_t history_size = match.GetHistory().size();
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kWin}), MatchError::kOk);
    EXPECT_EQ(match.GetHistory().size(), history_size);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kCard, Card{1, 6}, 0}),
              MatchError::kMatchAlreadyFinished);
}
UTEST(Match, WinRequiresFinishedSession) {
    Match match("m-win-early", {MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, std::nullopt, 1);
    EXPECT_EQ(match.ApplyMove(1, MoveRequest{ActionType::kWin}), MatchError::kIllegalMove);
    EXPECT_FALSE(match.IsFinished());
}
UTEST(Match, ResignFinishesMatch) {
    Match match("m-resign", {MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, std::nullopt, 1);
    EXPECT_EQ(match.Resign(1), MatchError::kOk);
    EXPECT_TRUE(match.IsFinished());
    const auto outcome = match.GetResult();
    ASSERT_TRUE(outcome.reason.has_value());
    EXPECT_EQ(*outcome.reason, FinishReason::kResign);
    EXPECT_EQ(outcome.winner_id, 2);
    EXPECT_EQ(outcome.loser_id, 1);
    EXPECT_EQ(match.Resign(2), MatchError::kMatchAlreadyFinished);
    EXPECT_EQ(match.Resign(999), MatchError::kMatchAlreadyFinished);
}
UTEST(Match, CraftedAttackDefenseFlow) {
    Match match("m-flow", {MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, std::nullopt, 1);
    match.session.SetPlayersCards({Card{0, 8}}, {Card{0, 9}, Card{2, 7}});
    match.session.trump = 3;
    EXPECT_EQ(match.ApplyMove(1, MoveRequest{ActionType::kCard, Card{0, 8}, 0}), MatchError::kOk);
    EXPECT_EQ(match.ApplyMove(1, MoveRequest{ActionType::kGrab}), MatchError::kIllegalMove);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kTransfer}), MatchError::kIllegalMove);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kCard, Card{0, 9}, 0}), MatchError::kOk);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kTransfer}), MatchError::kOk);
    MatchError error = MatchError::kOk;
    auto state = match.GetState(2, error);
    ASSERT_TRUE(state.has_value());
    EXPECT_EQ(error, MatchError::kOk);
    EXPECT_EQ(state->attacker_id, 2);
    EXPECT_EQ(state->defender_id, 1);
    EXPECT_TRUE(state->table[0].empty());
    EXPECT_EQ(match.ApplyMove(1, MoveRequest{ActionType::kCard}), MatchError::kNotThisPlayersTurn);
}
UTEST(Match, IllegalDefenseRejectedWithoutHistoryRecord) {
    Match match("m-rules", {MakePlayer(1, "Alice"), MakePlayer(2, "Bob")}, std::nullopt, 1);
    match.session.SetPlayersCards({Card{0, 8}}, {Card{2, 9}});
    match.session.trump = 3;
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kCard}), MatchError::kNotThisPlayersTurn);
    EXPECT_EQ(match.ApplyMove(1, MoveRequest{ActionType::kCard, Card{0, 8}, 0}), MatchError::kOk);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kCard, Card{2, 9}, 1}),
              MatchError::kIllegalMove);
    EXPECT_EQ(match.ApplyMove(2, MoveRequest{ActionType::kCard, Card{2, 9}, 2}),
              MatchError::kIllegalMove);
    EXPECT_EQ(match.GetHistory().size(), 1u);
}
