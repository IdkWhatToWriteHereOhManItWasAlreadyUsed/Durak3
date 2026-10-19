#pragma once

#include <string>

namespace durak {

struct Card {
    int suit = 0;
    int power = 0;
};

inline bool operator==(const Card& lhs, const Card& rhs) {
    return lhs.suit == rhs.suit && lhs.power == rhs.power;
}

inline std::string CardToString(const Card& card) {
    return std::to_string(card.suit) + "," + std::to_string(card.power) + ";";
}

}
