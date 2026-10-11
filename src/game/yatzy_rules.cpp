#include "yatzy_rules.h"

#include <algorithm>

namespace yatzy {

Game::Game() { reset(); }

void Game::reset() {
    dice_.fill(0);
    held_.fill(false);
    scores_.fill(-1);
    rolls_ = rounds_ = yahtzee_bonus_ = 0;
}

bool Game::roll(const Dice &values) {
    if (finished() || rolls_ == 3 ||
        std::any_of(values.begin(), values.end(), [](int value) { return value < 1 || value > 6; })) {
        return false;
    }
    for (int i = 0; i < 5; ++i) {
        if (!held_[i]) dice_[i] = values[i];
    }
    ++rolls_;
    return true;
}

bool Game::set_held(int index, bool held) {
    if (index < 0 || index >= 5 || rolls_ == 0 || finished()) return false;
    held_[index] = held;
    return true;
}

int Game::evaluate(const Dice &dice, int category) {
    if (category < 0 || category >= CATEGORY_COUNT) return -1;
    std::array<int, 7> counts{};
    int sum = 0;
    for (int value : dice) {
        if (value < 1 || value > 6) return -1;
        ++counts[value];
        sum += value;
    }
    if (category <= SIXES) return counts[category + 1] * (category + 1);
    int largest = *std::max_element(counts.begin(), counts.end());
    switch (category) {
        case THREE_OF_A_KIND: return largest >= 3 ? sum : 0;
        case FOUR_OF_A_KIND: return largest >= 4 ? sum : 0;
        case FULL_HOUSE:
            return largest == 3 && std::find(counts.begin(), counts.end(), 2) != counts.end() ? 25 : 0;
        case SMALL_STRAIGHT:
        case LARGE_STRAIGHT: {
            int needed = category == SMALL_STRAIGHT ? 4 : 5;
            int run = 0;
            for (int value = 1; value <= 6; ++value) {
                run = counts[value] ? run + 1 : 0;
                if (run >= needed) return needed == 4 ? 30 : 40;
            }
            return 0;
        }
        case YAHTZEE: return largest == 5 ? 50 : 0;
        case CHANCE: return sum;
        default: return -1;
    }
}

int Game::preview(int category) const {
    if (finished() || rolls_ == 0 || category < 0 || category >= CATEGORY_COUNT || scores_[category] != -1) return -1;
    // Joker applies once the Yahtzee box is filled, including a scratched zero.
    if (scores_[YAHTZEE] != -1 && evaluate(dice_, YAHTZEE) == 50) {
        int matching_upper = dice_[0] - 1;
        if (scores_[matching_upper] == -1) {
            return category == matching_upper ? dice_[0] * 5 : -1;
        }
        bool lower_available = std::any_of(scores_.begin() + THREE_OF_A_KIND, scores_.end(),
            [](int value) { return value == -1; });
        if (category <= SIXES) return lower_available ? -1 : 0;
        if (category == FULL_HOUSE) return 25;
        if (category == SMALL_STRAIGHT) return 30;
        if (category == LARGE_STRAIGHT) return 40;
    }
    return evaluate(dice_, category);
}

bool Game::score(int category) {
    int value = preview(category);
    if (value < 0) return false;
    if (scores_[YAHTZEE] == 50 && evaluate(dice_, YAHTZEE) == 50) yahtzee_bonus_ += 100;
    scores_[category] = value;
    ++rounds_;
    rolls_ = 0;
    dice_.fill(0);
    held_.fill(false);
    return true;
}

int Game::upper_total() const {
    int sum = 0;
    for (int i = ONES; i <= SIXES; ++i) sum += std::max(0, scores_[i]);
    return sum;
}

int Game::total() const {
    int sum = upper_bonus() + yahtzee_bonus_;
    for (int value : scores_) sum += std::max(0, value);
    return sum;
}

}
