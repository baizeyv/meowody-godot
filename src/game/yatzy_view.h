#pragma once

#include <godot_cpp/classes/control.hpp>
#include <godot_cpp/classes/button.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/accept_dialog.hpp>
#include <godot_cpp/classes/confirmation_dialog.hpp>
#include "yatzy_game.h"

// Runtime-only UI; the editor keeps the lightweight scene root.
class YatzyView : public godot::Control {
    GDREFLECT_CLASS(YatzyView, godot::Control)
public:
    void _ready() override;
    GDMETHOD
    void refresh();
    GDMETHOD
    void on_roll();
    GDMETHOD
    void on_hold(int index);
    GDMETHOD
    void on_select(int category);
    GDMETHOD
    void on_score();
    GDMETHOD
    void on_restart();
    GDMETHOD
    void on_settings();
    GDMETHOD
    void on_back();
    GDMETHOD
    void on_quit();
private:
    godot::Ref<YatzyGame> game_;
    std::array<godot::Button *, 13> rows_{};
    std::array<godot::Label *, 13> values_{};
    std::array<godot::Button *, 5> dice_{};
    godot::Button *roll_ = nullptr;
    godot::Button *score_ = nullptr;
    godot::Label *total_ = nullptr;
    godot::Label *round_ = nullptr;
    godot::Label *hint_ = nullptr;
    godot::Label *bonus_ = nullptr;
    godot::Label *extra_bonus_ = nullptr;
    godot::AcceptDialog *settings_ = nullptr;
    godot::AcceptDialog *result_ = nullptr;
    godot::ConfirmationDialog *exit_ = nullptr;
    int selected_ = -1;
};
