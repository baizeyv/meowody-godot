#include "virtual_list.h"
#include "node_helpers.h"
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/input.hpp>
namespace meowody {
using namespace godot;
VirtualList::VirtualList() { set_process(true); set_clip_contents(true); }
void VirtualList::clear_cells() {
    // Internal children have no owner and are never serialized into user scenes.
    for (auto &entry : cells) { if (!editor()) unbind_item(entry.second, count ? entry.first % count : -1); entry.second->queue_free(); }
    for (auto *cell : spare) cell->queue_free();
    cells.clear(); spare.clear();
}
double VirtualList::scroll_position() const { return direction == 1 ? get_h_scroll() : get_v_scroll(); }
void VirtualList::move_scroll(double value) { if (direction == 1) set_h_scroll(int(std::max(0.0, finite(value)))); else set_v_scroll(int(std::max(0.0, finite(value)))); }
double VirtualList::view_extent() const { return std::max(1.0, double(direction == 1 ? get_size().x : get_size().y)); }
void VirtualList::rebuild() {
    if (!content) { content = memnew(Control); content->set_mouse_filter(MOUSE_FILTER_PASS); add_child(content, false, INTERNAL_MODE_BACK); }
    clear_cells(); visible_state.clear(); invalid_scene = false;
    count = std::clamp(editor() ? preview_count : item_count, 0, 1000000);
    lanes = looping() ? 1 : std::clamp(columns, 1, 64);
    set_horizontal_scroll_mode(direction == 1 ? SCROLL_MODE_AUTO : SCROLL_MODE_DISABLED);
    set_vertical_scroll_mode(direction == 1 ? SCROLL_MODE_DISABLED : SCROLL_MODE_AUTO);
    int rows = (count + lanes - 1) / lanes;
    offsets.clear(); offsets.reserve(rows + 1); offsets.push_back(std::max(0.0, padding()));
    auto extent = [&](int i) { return std::max(1.0, finite(lanes == 1 && i < item_extents.size() ? item_extents[i] : item_extent, 64)); };
    double gap = std::clamp(finite(spacing),0.0,256.0);
    for (int row = 0; row < rows; ++row) offsets.push_back(offsets.back() + extent(row) + gap);
    cycle_extent = offsets.back() - offsets.front(); middle = 0;
    virtual_count = count;
    if (looping() && count > 0) {
        int repetitions = std::clamp(3 + int(std::ceil(view_extent() / std::max(1.0, cycle_extent))) * 2, 3, 4096);
        // Repeat whole data cycles, including when a single cycle is smaller than the viewport.
        middle = (repetitions / 2) * cycle_extent;
        offsets.reserve(size_t(rows) * repetitions + 1);
        for (int cycle = 1; cycle < repetitions; ++cycle) for (int row = 0; row < rows; ++row) offsets.push_back(offsets.back() + extent(row) + gap);
        virtual_count = count * repetitions;
    }
    double total = count ? offsets.back() - (looping() ? 0 : gap) + padding() : 0;
    Vector2 minimum = direction == 1 ? Vector2(total, 0) : Vector2(0, total);
    content->set_custom_minimum_size(minimum);
    content->set_size(direction == 1 ? Vector2(total, get_size().y) : Vector2(get_size().x, total));
    if (looping()) move_scroll(middle);
}
int VirtualList::index_at(double position) const {
    if (offsets.size() < 2) return 0;
    auto it = std::upper_bound(offsets.begin(), offsets.end(), position);
    int row = std::clamp(int(it - offsets.begin()) - 1, 0, int(offsets.size()) - 2);
    return row * lanes;
}
double VirtualList::item_position(int index) const { return offsets.empty() ? 0 : offsets[std::clamp(index / lanes, 0, int(offsets.size()) - 1)]; }
void VirtualList::update_cells() {
    if (!count || refreshing || !content || invalid_scene) return;
    refreshing = true;
    double position = scroll_position(), gap = std::clamp(finite(spacing),0.0,256.0);
    int extra = std::clamp(preload_count,0,20) * lanes;
    int first = std::max(0, index_at(position) - extra), last = std::min(virtual_count - 1, index_at(position + view_extent()) + lanes - 1 + extra);
    double cross = direction == 1 ? get_size().y - (get_h_scroll_bar()->is_visible() ? get_h_scroll_bar()->get_size().y : 0) : get_size().x - (get_v_scroll_bar()->is_visible() ? get_v_scroll_bar()->get_size().x : 0);
    if (!changed(visible_state, {first, last, cross}) && !force_bind) { refreshing = false; return; }
    for (auto it = cells.begin(); it != cells.end();) {
        if (it->first < first || it->first > last) {
            Control *cell = it->second;
            if (!editor()) unbind_item(cell, it->first % count);
            cell->hide(); spare.push_back(cell); it = cells.erase(it);
        } else ++it;
    }

    double breadth = std::max(1.0, (cross - gap * (lanes - 1)) / lanes);
    for (int index = first; index <= last; ++index) {
        auto found = cells.find(index);
        bool fresh = found == cells.end();
        Control *cell = fresh ? nullptr : found->second;
        if (fresh) {
            if (!spare.empty()) { cell = spare.back(); spare.pop_back(); }
            else if (item_scene.is_valid() && (!editor() || preview_in_editor)) {
                Node *node = item_scene->instantiate(); cell = Object::cast_to<Control>(node);
                if (!cell) { if (node) memdelete(node); invalid_scene = true; refreshing = false; update_configuration_warnings(); return; }
                content->add_child(cell, false, INTERNAL_MODE_BACK);
            } else {
                auto *label = memnew(Label); label->set_mouse_filter(MOUSE_FILTER_PASS); cell = label;
                content->add_child(cell, false, INTERNAL_MODE_BACK);
            }
            cells[index] = cell;
        }
        int row = index / lanes;
        double extent = std::max(1.0, offsets[row + 1] - offsets[row] - gap);
        Vector2 at((index % lanes) * (breadth + gap), offsets[row]), size(breadth, extent);
        if (direction == 1) { std::swap(at.x, at.y); std::swap(size.x, size.y); }
        cell->set_position(at); cell->set_size(size); cell->show();
        if (fresh || force_bind) {
            if (editor() || item_scene.is_null()) { auto *label = Object::cast_to<Label>(cell); if (label) label->set_text("Item " + String::num_int64(index % count)); }
            if (!editor()) { if (binder) binder(cell, index % count); bind_item(cell, index % count); }
        }
    }
    // Shrinking a viewport must release surplus retained cells too.
    while (spare.size() > size_t(2 * extra + lanes)) { spare.back()->queue_free(); spare.pop_back(); }
    force_bind = false; refreshing = false;
}
void VirtualList::_process(double delta) {
    if (refreshing) return;
    if (changed(state, {item_scene, item_count, direction, item_extent, spacing, preload_count, columns, int64_t(reinterpret_cast<uintptr_t>(item_extents.ptr())), item_extents.size(), preview_in_editor, preview_count, get_size(), padding()})) rebuild();
    if (looping() && cycle_extent > 0 && count) {
        double position = scroll_position(), wrapped = middle + std::fmod(position - middle, cycle_extent);
        if (wrapped < middle) wrapped += cycle_extent;
        if (std::abs(position - wrapped) >= cycle_extent - 1) { move_scroll(wrapped); recentered(scroll_position() - position); }
    }
    if (!editor()) scroll_step(delta);
    update_cells();
}
PackedStringArray VirtualList::_get_configuration_warnings() const {
    PackedStringArray result;
    if (invalid_scene) result.append("Item Scene must have a Control root.");
    if (columns > 1 && !item_extents.is_empty()) result.append("Item Extents is only used in single-lane lists; grids use Item Extent.");
    if (looping() && columns != 1) result.append("LoopScrollContainer uses one lane.");
    return result;
}
void VirtualList::refresh() { force_bind = true; }
void VirtualList::set_binder(std::function<void(Control *, int)> callback) { binder = std::move(callback); refresh(); }
void VirtualList::scroll_to_index(int index) {
    if (offsets.empty()) rebuild();
    if (!count) return;
    int actual = std::clamp(index,0,count-1);
    if (looping()) actual += int(middle / cycle_extent) * count;
    move_scroll(item_position(actual)); force_bind = true;
}
int VirtualList::get_visible_node_count() const { return int(cells.size() + spare.size()); }
Control *VirtualList::get_item_control(int index) const { for (const auto &entry : cells) if (count && entry.first % count == index) return entry.second; return nullptr; }
}
