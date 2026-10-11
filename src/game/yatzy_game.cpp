#include "yatzy_game.h"

YatzyGame::YatzyGame() {
    random_.instantiate();
    random_->randomize();
}

void YatzyGame::reset() {
    game_.reset();
    state_changed();
}

bool YatzyGame::roll() {
    if (game_.rolls_left() == 0) return false;
    yatzy::Dice values;
    for (int &value : values) value = random_->randi_range(1, 6);
    if (!game_.roll(values)) return false;
    state_changed();
    return true;
}

bool YatzyGame::set_held(int index, bool held) {
    if (!game_.set_held(index, held)) return false;
    state_changed();
    return true;
}

int YatzyGame::preview_score(int category) const { return game_.preview(category); }

bool YatzyGame::score(int category) {
    if (!game_.score(category)) return false;
    state_changed();
    return true;
}

godot::Dictionary YatzyGame::get_state() const {
    godot::Dictionary state;
    godot::PackedInt32Array dice, held, scores, previews;
    for (int value : game_.dice()) dice.push_back(value);
    for (bool value : game_.held()) held.push_back(value ? 1 : 0);
    for (int i = 0; i < yatzy::CATEGORY_COUNT; ++i) {
        scores.push_back(game_.scores()[i]);
        previews.push_back(game_.preview(i));
    }
    state["dice"] = dice;
    state["held"] = held;
    state["scores"] = scores;
    state["previews"] = previews;
    state["rolls_left"] = game_.rolls_left();
    state["rounds_completed"] = game_.rounds_completed();
    state["finished"] = game_.finished();
    state["upper_total"] = game_.upper_total();
    state["upper_bonus"] = game_.upper_bonus();
    state["yahtzee_bonus"] = game_.yahtzee_bonus();
    state["total"] = game_.total();
    return state;
}

godot::PackedStringArray YatzyGame::get_category_names() const {
    godot::PackedStringArray names;
    for (const char *name : {"ones", "twos", "threes", "fours", "fives", "sixes",
         "three_of_a_kind", "four_of_a_kind", "full_house", "small_straight",
         "large_straight", "yahtzee", "chance"}) names.push_back(name);
    return names;
}
