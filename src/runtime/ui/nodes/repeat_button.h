#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/input_event.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/texture2d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/callable.hpp>
#include <map>
namespace meowody {
class RepeatButton : public godot::Button {
    GDREFLECT_CLASS(RepeatButton, godot::Button)
    GDPROPERTY_RANGE(0, 10, 0.01) double initial_delay = 0.4;
    GDPROPERTY_RANGE(0.01, 10, 0.01) double repeat_interval = 0.15;
    GDPROPERTY_RANGE(0, 10, 0.01) double acceleration = 0.5;
    GDPROPERTY_RANGE(0.01, 1, 0.01) double minimum_interval = 0.03;
    GDPROPERTY bool trigger_on_press = true;
    bool held = false;
    double elapsed = 0, until_repeat = 0;
    int repeats = 0;
    void begin_hold();
    void cancel_hold();
public:
    RepeatButton();
    ~RepeatButton() override;
    void _process(double delta) override;
    void _notification(int what);
    GDEMIT_SIGNAL void repeated(int count);
};
}
