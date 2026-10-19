#pragma once
namespace durak::state {
enum class MoveType { kGrab, kTransfer, kWin };
struct Move {
    MoveType type = MoveType::kTransfer;
    int card_num = -1;
    int stack_num = -1;
};
inline bool operator==(const Move& lhs, const Move& rhs) {
    return lhs.type == rhs.type && lhs.card_num == rhs.card_num && lhs.stack_num == rhs.stack_num;
}
}
