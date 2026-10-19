#pragma once
#include <optional>
#include <string>
#include <vector>
#include "match.hpp"
namespace durak::match {
class IMatchManager {
public:
    virtual ~IMatchManager() = default;
    virtual std::optional<std::string> CreateMatch(std::vector<Player> players,
                                                  std::optional<std::int64_t> first_attacker_id) = 0;
    virtual MatchError ApplyMove(const std::string& match_id, std::int64_t player_id,
                                const MoveRequest& move, MatchMove* applied) = 0;
    virtual MatchError Resign(const std::string& match_id, std::int64_t player_id) = 0;
    virtual std::optional<MatchInfo> GetMatch(const std::string& match_id) const = 0;
    virtual std::optional<MatchState> GetMatchState(const std::string& match_id,
                                                  std::int64_t viewer_id,
                                                  MatchError* error) const = 0;
    virtual std::optional<MatchResult> GetMatchResult(const std::string& match_id,
                                                     MatchError* error) const = 0;
    virtual std::optional<std::vector<MatchMove>> GetMatchHistory(const std::string& match_id,
                                                                   MatchError* error) const = 0;
    virtual std::optional<std::string> GetReplayText(const std::string& match_id,
                                                     MatchError* error) const = 0;
};
}
