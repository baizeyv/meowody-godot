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
class LongPressButton : public godot::Button {
    GDREFLECT_CLASS(LongPressButton, godot::Button)
    GDPROPERTY_RANGE(0.01, 10, 0.01) double long_press_time = 0.5;
    GDPROPERTY_RANGE(0, 1000, 1) double cancel_distance = 20;
    GDPROPERTY bool repeat = false;
    bool held = false, fired = false;
    double elapsed = 0;
    godot::Vector2 origin;
    void begin_hold();
    void cancel_hold();
public:
    LongPressButton();
    ~LongPressButton() override;
    void _process(double delta) override;
    void _notification(int what);
    GDEMIT_SIGNAL void long_pressed();
    GDEMIT_SIGNAL void long_press_progress(double progress);
};
}
