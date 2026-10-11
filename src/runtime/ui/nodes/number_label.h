#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/array.hpp>
namespace meowody {
class NumberLabel : public godot::Label {
    GDREFLECT_CLASS(NumberLabel, godot::Label)
    GDPROPERTY double value = 0;
    GDPROPERTY bool animate_change = true;
    GDPROPERTY_RANGE(0, 10, 0.01) double duration = 0.35;
    GDPROPERTY_RANGE(0, 8, 1) int decimals = 0;
    GDPROPERTY bool grouping = true;
    GDPROPERTY bool compact = false;
    GDPROPERTY godot::String prefix;
    GDPROPERTY godot::String suffix;
    GDPROPERTY bool rolling = false;
    godot::Array options;
    double displayed = 0, from = 0, target = 0, elapsed = 0;
    bool initialized = false, animating = false;
    godot::Label *previous_label = nullptr, *next_label = nullptr;
    godot::String previous_text;
    void update_text();
public:
    NumberLabel();
    void _process(double delta) override;
    GDMETHOD godot::String format_number(double number) const;
    GDMETHOD double get_displayed_value() const;
    GDMETHOD void finish_animation();
    GDEMIT_SIGNAL void animation_finished();
};
}
