#pragma once
#include <chrono>
#include <cstdint>
#include <optional>
#include "state/card.hpp"

namespace durak::match {
enum class ActionType { kCard, kTransfer, kGrab, kWin };

struct MatchMove {
    std::uint64_t sequence = 0;
    std::int64_t player_id = -1;
    ActionType type = ActionType::kTransfer;
    std::optional<Card> card;
    std::optional<int> stack;
    std::optional<int> card_num;
    std::chrono::system_clock::time_point at = std::chrono::system_clock::now();
};
}
