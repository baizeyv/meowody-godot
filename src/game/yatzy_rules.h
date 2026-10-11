#pragma once

#include <array>

namespace yatzy {

enum Category {
    ONES, TWOS, THREES, FOURS, FIVES, SIXES,
    THREE_OF_A_KIND, FOUR_OF_A_KIND, FULL_HOUSE,
    SMALL_STRAIGHT, LARGE_STRAIGHT, YAHTZEE, CHANCE,
    CATEGORY_COUNT
};

using Dice = std::array<int, 5>;

// Single-player Yahtzee rules. -1 means an unused/unavailable score.
class Game {
public:
    Game();
    void reset();
    bool roll(const Dice &values);
    bool set_held(int index, bool held);
    int preview(int category) const;
    bool score(int category);
    static int evaluate(const Dice &dice, int category);

    const Dice &dice() const { return dice_; }
    const std::array<bool, 5> &held() const { return held_; }
    const std::array<int, CATEGORY_COUNT> &scores() const { return scores_; }
    int rolls_left() const { return finished() ? 0 : 3 - rolls_; }
    int rounds_completed() const { return rounds_; }
    bool finished() const { return rounds_ == CATEGORY_COUNT; }
    int upper_total() const;
    int upper_bonus() const { return upper_total() >= 63 ? 35 : 0; }
    int yahtzee_bonus() const { return yahtzee_bonus_; }
    int total() const;

private:
    Dice dice_{};
    std::array<bool, 5> held_{};
    std::array<int, CATEGORY_COUNT> scores_{};
    int rolls_ = 0;
    int rounds_ = 0;
    int yahtzee_bonus_ = 0;
};

}
