#include "match_manager.hpp"
namespace durak::match {
std::string MatchManager::GenerateMatchId() {
    static constexpr char kHex[] = "0123456789abcdef";
    std::mt19937 rng(seed_++);
    std::uniform_int_distribution<int> digit(0, 15);
    std::string id;
    const std::size_t groups[] = {8, 4, 4, 4, 12};
    for (std::size_t group = 0; group < 5; ++group) {
        if (group > 0) id += '-';
        for (std::size_t i = 0; i < groups[group]; ++i) id += kHex[digit(rng)];
    }
    return id;
}
std::optional<std::string> MatchManager::CreateMatch(std::vector<Player> players,
                                                   std::optional<std::int64_t> first_attacker_id) {
    if (players.size() != Match::kPlayersCount) return std::nullopt;
    if (players[0].id < 0 || players[1].id < 0 || players[0].id == players[1].id) {
        return std::nullopt;
    }
    std::lock_guard lock(mutex_);
    const std::string match_id = GenerateMatchId();
    matches_[match_id] = std::make_shared<Match>(match_id, std::move(players), first_attacker_id,
                                             seed_++);
    return match_id;
}
MatchError MatchManager::ApplyMove(const std::string& match_id, std::int64_t player_id,
                                 const MoveRequest& move, MatchMove* applied) {
    std::lock_guard lock(mutex_);
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) return MatchError::kMatchNotFound;
    const MatchError error = found->second->ApplyMove(player_id, move);
    if (error == MatchError::kOk && applied != nullptr && !found->second->GetHistory().empty()) {
        *applied = found->second->GetHistory().back();
    }
    return error;
}
MatchError MatchManager::Resign(const std::string& match_id, std::int64_t player_id) {
    std::lock_guard lock(mutex_);
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) return MatchError::kMatchNotFound;
    return found->second->Resign(player_id);
}
std::optional<MatchInfo> MatchManager::GetMatch(const std::string& match_id) const {
    std::lock_guard lock(mutex_);
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) return std::nullopt;
    return found->second->GetInfo();
}
std::optional<MatchState> MatchManager::GetMatchState(const std::string& match_id,
                                                   std::int64_t viewer_id,
                                                   MatchError* error) const {
    std::lock_guard lock(mutex_);
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) {
        if (error != nullptr) *error = MatchError::kMatchNotFound;
        return std::nullopt;
    }
    MatchError local_error = MatchError::kOk;
    auto state = found->second->GetState(viewer_id, local_error);
    if (error != nullptr) *error = local_error;
    return state;
}
std::optional<MatchResult> MatchManager::GetMatchResult(const std::string& match_id,
                                                      MatchError* error) const {
    std::lock_guard lock(mutex_);
    if (error != nullptr) *error = MatchError::kOk;
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) {
        if (error != nullptr) *error = MatchError::kMatchNotFound;
        return std::nullopt;
    }
    return found->second->GetResult();
}
std::optional<std::vector<MatchMove>> MatchManager::GetMatchHistory(const std::string& match_id,
                                                                    MatchError* error) const {
    std::lock_guard lock(mutex_);
    if (error != nullptr) *error = MatchError::kOk;
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) {
        if (error != nullptr) *error = MatchError::kMatchNotFound;
        return std::nullopt;
    }
    return found->second->GetHistory();
}
std::optional<std::string> MatchManager::GetReplayText(const std::string& match_id,
                                                      MatchError* error) const {
    std::lock_guard lock(mutex_);
    if (error != nullptr) *error = MatchError::kOk;
    const auto found = matches_.find(match_id);
    if (found == matches_.end()) {
        if (error != nullptr) *error = MatchError::kMatchNotFound;
        return std::nullopt;
    }
    return found->second->GetReplayText();
}
std::size_t MatchManager::GetMatchCount() const {
    std::lock_guard lock(mutex_);
    return matches_.size();
}
}
