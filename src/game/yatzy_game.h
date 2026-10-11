#pragma once

#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/random_number_generator.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>

#include "reflection/reflection.h"
#include "yatzy_rules.h"

class YatzyGame : public godot::RefCounted {
    GDREFLECT_CLASS(YatzyGame, godot::RefCounted)

public:
    YatzyGame();
    GDMETHOD
    void reset();
    GDMETHOD
    bool roll();
    GDMETHOD
    bool set_held(int index, bool held);
    GDMETHOD
    int preview_score(int category) const;
    GDMETHOD
    bool score(int category);
    GDMETHOD
    godot::Dictionary get_state() const;
    GDMETHOD
    godot::PackedStringArray get_category_names() const;
    GDEMIT_SIGNAL
    void state_changed();

private:
    yatzy::Game game_;
    godot::Ref<godot::RandomNumberGenerator> random_;
};
