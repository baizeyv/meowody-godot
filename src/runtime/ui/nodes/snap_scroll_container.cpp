#include "snap_scroll_container.h"
#include <godot_cpp/variant/callable_method_pointer.hpp>
#include "node_helpers.h"
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/input.hpp>
namespace meowody {
using namespace godot;
SnapScrollContainer::SnapScrollContainer() {
    connect("scroll_started", callable_mp(this, &SnapScrollContainer::begin_scroll));
    connect("scroll_ended", callable_mp(this, &SnapScrollContainer::end_scroll));
}
SnapScrollContainer::~SnapScrollContainer() {
    const auto begin = callable_mp(this, &SnapScrollContainer::begin_scroll);
    const auto end = callable_mp(this, &SnapScrollContainer::end_scroll);
    if (is_connected("scroll_started", begin)) disconnect("scroll_started", begin);
    if (is_connected("scroll_ended", end)) disconnect("scroll_ended", end);
}
void SnapScrollContainer::begin_scroll() { touch_scrolling = true; snapping = false; }
void SnapScrollContainer::end_scroll() { touch_scrolling = false; unsettled = true; idle = 0; }
void SnapScrollContainer::rebuild() {
    snapping = false; unsettled = true; idle = 0;
    VirtualList::rebuild();
    int next = std::min(current_index, count-1);
    if (current_index != next) { current_index = next; if (!editor()) index_changed(next); }
    last_position = scroll_position();
}
double SnapScrollContainer::padding() const { return looping() ? 0 : std::clamp(finite(overscroll), 0.0, 1000.0); }
int SnapScrollContainer::get_current_index() const { return current_index; }
double SnapScrollContainer::target_position(int index) const {
    if (!count) return 0;
    int actual = index;
    if (looping()) actual += int(middle / cycle_extent) * count;
    double target = snap_mode == 1 ? index * view_extent() : item_position(actual);
    if (snap_mode == 2) {
        int row = actual / lanes;
        target -= (view_extent() - (offsets[row + 1] - offsets[row] - std::max(0.0, finite(spacing)))) * 0.5;
    }
    return std::clamp(target, 0.0, std::max(0.0, offsets.back() + padding() - view_extent()));
}
void SnapScrollContainer::set_current_index(int index, bool animated) {
    if (offsets.empty()) rebuild();
    if (!count) return;
    requested_index = std::clamp(index, 0, count - 1);
    from = scroll_position(); destination = target_position(requested_index); elapsed = 0;
    snapping = animated && finite(snap_duration) > 0 && !editor(); unsettled = false;
    snap_started();
    if (!snapping) {
        move_scroll(destination); last_position = scroll_position();
        if (current_index != requested_index) { current_index = requested_index; index_changed(current_index); }
        snap_finished();
    }
}
void SnapScrollContainer::snap_now() {
    if (!count) return;
    double at = scroll_position() + (snap_mode == 2 ? view_extent() * 0.5 : 0);
    int index = snap_mode == 1 ? int(std::round(at / view_extent())) : index_at(at);
    if (looping()) index %= count;
    if (std::abs(velocity) >= std::max(0.0,finite(velocity_threshold)) && velocity_threshold > 0) index += velocity > 0 ? 1 : -1;
    if (looping()) index = (index % count + count) % count;
    set_current_index(index, true); velocity = 0;
}
void SnapScrollContainer::recentered(double shift) { last_position += shift; from += shift; destination += shift; }
void SnapScrollContainer::scroll_step(double delta) {
    double position = scroll_position();
    bool pointer = touch_scrolling || Input::get_singleton()->is_mouse_button_pressed(MOUSE_BUTTON_LEFT);
    if (snapping) {
        if (pointer || std::abs(position - last_position) > 2) { snapping = false; unsettled = true; idle = 0; }
        else {
            elapsed += delta; double t = unit(elapsed / std::max(0.001,finite(snap_duration)));
            double eased = snap_curve.is_valid() ? unit(snap_curve->sample(t)) : 1 - std::pow(1-t,3);
            move_scroll(from + (destination-from)*eased); last_position = scroll_position();
            if (t >= 1) {
                move_scroll(destination); last_position = scroll_position(); snapping = false;
                if (current_index != requested_index) { current_index = requested_index; index_changed(current_index); }
                snap_finished();
            }
            return;
        }
    }
    if (position != last_position) { velocity = (position-last_position) / std::max(0.001,delta); unsettled = true; idle = 0; }
    else idle += delta;
    last_position = position;
    if (unsettled && idle > 0.12 && !pointer) snap_now();
}}
