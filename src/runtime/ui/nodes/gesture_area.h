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
class GestureArea : public godot::Control {
    GDREFLECT_CLASS(GestureArea, godot::Control)
    GDPROPERTY_RANGE(0, 100, 1) double movement_threshold = 10;
    GDPROPERTY_RANGE(0.01, 5, 0.01) double long_press_time = 0.5;
    GDPROPERTY_RANGE(0.01, 2, 0.01) double double_tap_time = 0.3;
    GDPROPERTY_RANGE(1, 1000, 1) double swipe_distance = 60;
    GDPROPERTY_RANGE(0.01, 5, 0.01) double swipe_time = 0.5;
    GDPROPERTY bool mouse_enabled = true;
    struct Touch { godot::Vector2 start, position; double time = 0; bool moved = false, long_fired = false, multi = false; };
    std::map<int, Touch> touches;
    double clock = 0, last_tap_time = -100;
    godot::Vector2 last_tap_position;
    void begin(int id, godot::Vector2 position);
    void move(int id, godot::Vector2 position);
    void end(int id, godot::Vector2 position, bool canceled);
public:
    GestureArea();
    void _gui_input(const godot::Ref<godot::InputEvent> &event) override;
    void _process(double delta) override;
    void _notification(int what);
    GDMETHOD void cancel_gestures();
    GDEMIT_SIGNAL void tap(godot::Vector2 position);
    GDEMIT_SIGNAL void double_tap(godot::Vector2 position);
    GDEMIT_SIGNAL void long_press(godot::Vector2 position);
    GDEMIT_SIGNAL void swipe(godot::Vector2 direction);
    GDEMIT_SIGNAL void pan(godot::Vector2 delta);
    GDEMIT_SIGNAL void pinch(double scale);
    GDEMIT_SIGNAL void rotate(double angle);
};
}
