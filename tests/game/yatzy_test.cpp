// Compiled as a standalone executable by run_tests.py.
#ifdef YATZY_TEST
#include "game/yatzy_rules.h"
#include <cassert>
#include <iostream>

using namespace yatzy;

int main() {
    assert(Game::evaluate({2, 2, 2, 5, 5}, FULL_HOUSE) == 25);
    assert(Game::evaluate({2, 2, 2, 2, 2}, FULL_HOUSE) == 0);
    assert(Game::evaluate({2, 2, 2, 5, 5}, THREE_OF_A_KIND) == 16);
    assert(Game::evaluate({6, 6, 6, 6, 1}, FOUR_OF_A_KIND) == 25);
    assert(Game::evaluate({1, 2, 2, 3, 4}, SMALL_STRAIGHT) == 30);
    assert(Game::evaluate({2, 3, 4, 5, 6}, LARGE_STRAIGHT) == 40);
    assert(Game::evaluate({1, 2, 3, 4, 6}, LARGE_STRAIGHT) == 0);
    assert(Game::evaluate({6, 6, 6, 6, 6}, YAHTZEE) == 50);
    assert(Game::evaluate({1, 2, 3, 4, 5}, CHANCE) == 15);
    assert(Game::evaluate({0, 2, 3, 4, 5}, CHANCE) == -1);
    assert(Game::evaluate({1, 2, 3, 4, 5}, 13) == -1);

    Game game;
    assert(!game.score(CHANCE) && !game.set_held(0, true));
    assert(!game.roll({0, 1, 1, 1, 1}) && game.rolls_left() == 3);
    assert(game.roll({1, 2, 3, 4, 5}));
    assert(game.set_held(0, true) && !game.set_held(5, true));
    assert(game.roll({6, 6, 6, 6, 6}) && game.dice()[0] == 1);
    assert(game.roll({5, 5, 5, 5, 5}));
    assert(!game.roll({1, 1, 1, 1, 1}));
    assert(game.score(CHANCE) && game.scores()[CHANCE] == 21);
    assert(game.rolls_left() == 3 && !game.held()[0] && game.dice()[0] == 0);
    assert(!game.score(CHANCE));

    game.reset();
    for (int face = 1; face <= 6; ++face) {
        assert(game.roll({face, face, face, face == 1 ? 2 : 1, face == 1 ? 2 : 1}));
        assert(game.score(face - 1));
    }
    assert(game.upper_total() == 63 && game.upper_bonus() == 35);
    for (int category = THREE_OF_A_KIND; category < CATEGORY_COUNT; ++category) {
        assert(game.roll({1, 2, 3, 4, 5}) && game.score(category));
    }
    assert(game.finished() && game.rounds_completed() == 13 && game.rolls_left() == 0);
    assert(game.total() == 183 && !game.roll({1, 2, 3, 4, 5}));
    assert(!game.score(-1) && !game.set_held(0, true));

    game.reset();
    assert(game.roll({6, 6, 6, 6, 6}) && game.score(YAHTZEE));
    assert(game.roll({6, 6, 6, 6, 6}));
    assert(game.preview(FULL_HOUSE) == -1 && !game.score(CHANCE));
    assert(game.yahtzee_bonus() == 0);
    assert(game.score(SIXES) && game.yahtzee_bonus() == 100);
    assert(game.roll({6, 6, 6, 6, 6}) && game.preview(FULL_HOUSE) == 25);
    assert(game.score(FULL_HOUSE) && game.yahtzee_bonus() == 200);
    for (int category : {THREE_OF_A_KIND, FOUR_OF_A_KIND, SMALL_STRAIGHT, LARGE_STRAIGHT, CHANCE}) {
        assert(game.roll({6, 6, 6, 6, 6}) && game.score(category));
    }
    assert(game.roll({6, 6, 6, 6, 6}) && game.preview(ONES) == 0 && game.score(ONES));

    game.reset();
    assert(game.roll({1, 2, 3, 4, 5}) && game.score(YAHTZEE));
    assert(game.roll({2, 2, 2, 2, 2}) && !game.score(FULL_HOUSE) && game.score(TWOS));
    assert(game.roll({2, 2, 2, 2, 2}) && game.score(LARGE_STRAIGHT));
    assert(game.yahtzee_bonus() == 0 && game.scores()[LARGE_STRAIGHT] == 40);
    game.reset();
    assert(game.total() == 0 && game.rounds_completed() == 0 && game.scores()[YAHTZEE] == -1);
    std::cout << "Yahtzee rules: PASS\n";
}
#endif
