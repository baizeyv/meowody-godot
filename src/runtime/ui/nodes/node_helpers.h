#pragma once
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <algorithm>
#include <cmath>
namespace meowody {
inline bool changed(godot::Array &previous, std::initializer_list<godot::Variant> values) {
    bool different = previous.size() != int64_t(values.size());
    int index = 0;
    if (!different) for (const auto &value : values) {
        if (previous[index++] != value) { different = true; break; }
    }
    if (!different) return false;
    previous.resize(values.size());
    index = 0;
    for (const auto &value : values) previous[index++] = value;
    return true;
}
inline double finite(double value, double fallback = 0) { return std::isfinite(value) ? value : fallback; }
inline double unit(double value) { return std::clamp(finite(value), 0.0, 1.0); }
inline bool editor() { return godot::Engine::get_singleton()->is_editor_hint(); }
}
