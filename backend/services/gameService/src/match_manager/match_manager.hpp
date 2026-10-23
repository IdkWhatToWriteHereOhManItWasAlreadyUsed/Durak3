#pragma once
#include <memory>
#include <utility>
#include <mutex>
#include <optional>
#include <random>
#include <string>
#include <unordered_map>
#include <vector>

#include "i_match_manager.hpp"
#include "match/match.hpp"

namespace durak::match {
class MatchManager final : public IMatchManager {
public:
    MatchManager() : MatchManager(std::random_device{}()) {}

    explicit MatchManager(unsigned seed) : seed_(seed) {}

    std::optional<std::string> CreateMatch(std::vector<Player> players, std::optional<std::int64_t> first_attacker_id) override;
    MatchError ApplyMove(const std::string& match_id, std::int64_t player_id, const MoveRequest& move, MatchMove* applied) override;
    MatchError Resign(const std::string& match_id, std::int64_t player_id) override;
    std::optional<MatchInfo> GetMatch(const std::string& match_id) const override;
    std::optional<MatchState> GetMatchState(const std::string& match_id, std::int64_t viewer_id, MatchError* error) const override;
    std::optional<MatchResult> GetMatchResult(const std::string& match_id, MatchError* error) const override;
    std::optional<std::vector<MatchMove>> GetMatchHistory(const std::string& match_id, MatchError* error) const override;
    std::optional<std::string> GetReplayText(const std::string& match_id, MatchError* error) const override;
    std::size_t GetMatchCount() const;
private:
    std::string GenerateMatchId();
    unsigned seed_;
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::shared_ptr<Match>> matches_;
};

}  // namespace durak::match
