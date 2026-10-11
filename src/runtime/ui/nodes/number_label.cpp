#include "number_label.h"
#include "node_helpers.h"
#include <godot_cpp/classes/font.hpp>
#include <godot_cpp/variant/callable_method_pointer.hpp>
namespace meowody {
using namespace godot;
NumberLabel::NumberLabel() { set_process(true); set_clip_text(true); }
double NumberLabel::get_displayed_value() const { return displayed; }
String NumberLabel::format_number(double number) const {
    number = finite(number);
    String unit_suffix;
    if (compact) {
        static const char *units[] = {"", "K", "M", "B", "T"};
        int index = 0;
        while (std::abs(number) >= 1000 && index < 4) { number /= 1000; ++index; }
        unit_suffix = units[index];
    }
    int digits = std::clamp(decimals, 0, 8);
    String result = String::num(number, digits);
    // String::num trims trailing zeros: formatting always honors Decimals.
    int dot = result.find(".");
    if (digits > 0) {
        if (dot < 0) { result += "."; dot = result.length() - 1; }
        while (result.length() - dot - 1 < digits) result += "0";
    }
    if (grouping && unit_suffix.is_empty()) {
        int end = result.find("."); if (end < 0) end = result.length();
        for (int i = end - 3; i > (result.begins_with("-") ? 1 : 0); i -= 3) result = result.insert(i, ",");
    }
    return prefix + result + unit_suffix + suffix;
}
void NumberLabel::update_text() {
    String text = format_number(displayed);
    if (rolling && animating && !editor()) {
        if (!previous_label) {
            previous_label = memnew(Label); next_label = memnew(Label);
            add_child(previous_label, false, INTERNAL_MODE_BACK); add_child(next_label, false, INTERNAL_MODE_BACK);
            previous_label->set_mouse_filter(MOUSE_FILTER_IGNORE); next_label->set_mouse_filter(MOUSE_FILTER_IGNORE);
        }
        set_clip_contents(true); set_text("");
        double t = duration > 0 ? unit(elapsed / duration) : 1;
        for (auto *label : {previous_label, next_label}) {
            label->show(); label->set_size(get_size());
            label->set_horizontal_alignment(get_horizontal_alignment()); label->set_vertical_alignment(get_vertical_alignment());
            label->add_theme_font_override("font", get_theme_font("font"));
            label->add_theme_font_size_override("font_size", get_theme_font_size("font_size"));
            label->add_theme_color_override("font_color", get_theme_color("font_color"));
        }
        previous_label->set_text(previous_text); next_label->set_text(format_number(target));
        double sign = target >= from ? 1 : -1;
        previous_label->set_position(Vector2(0, -sign * get_size().y * t));
        next_label->set_position(Vector2(0, sign * get_size().y * (1 - t)));
    } else {
        if (previous_label) { previous_label->hide(); next_label->hide(); }
        set_text(text);
    }
}
void NumberLabel::_process(double delta) {
    bool reformat = changed(options, {decimals, grouping, compact, prefix, suffix, rolling, get_size()});
    double requested = finite(value);
    if (!initialized) { initialized = true; displayed = target = requested; update_text(); }
    if (requested != target) {
        previous_text = format_number(displayed); from = displayed; target = requested; elapsed = 0;
        animating = animate_change && !editor() && finite(duration) > 0;
        if (!animating) displayed = target;
        reformat = true;
    }
    if (animating) {
        elapsed += std::max(0.0, delta);
        double t = unit(elapsed / std::max(0.0001, finite(duration)));
        displayed = from + (target - from) * (1 - std::pow(1 - t, 3));
        if (t >= 1 || !animate_change) { displayed = target; animating = false; }
        update_text();
        if (!animating) animation_finished();
    } else if (reformat) update_text();
}
void NumberLabel::finish_animation() { bool was = animating; initialized = true; displayed = target = finite(value); animating = false; update_text(); if (was) animation_finished(); }
}
