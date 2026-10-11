#pragma once
#include "reflection/reflection.h"
#include <godot_cpp/classes/scroll_container.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/curve.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <functional>
#include <map>
#include <vector>
namespace meowody {
class VirtualList : public godot::ScrollContainer {
    GDREFLECT_CLASS(VirtualList, godot::ScrollContainer)
protected:
    GDPROPERTY godot::Ref<godot::PackedScene> item_scene;
    GDPROPERTY_RANGE(0, 1000000, 1) int item_count = 0;
    GDPROPERTY_ENUM("Vertical,Horizontal") int direction = 0;
    GDPROPERTY_RANGE(1, 4096, 1) double item_extent = 64;
    GDPROPERTY_RANGE(0, 256, 1) double spacing = 4;
    GDPROPERTY_RANGE(0, 20, 1) int preload_count = 2;
    GDPROPERTY_RANGE(1, 64, 1) int columns = 1;
    GDPROPERTY godot::PackedFloat32Array item_extents;
    GDPROPERTY bool preview_in_editor = false;
    GDPROPERTY_RANGE(0, 1000, 1) int preview_count = 20;
    godot::Control *content = nullptr;
    godot::Array state, visible_state;
    std::map<int, godot::Control *> cells;
    std::vector<godot::Control *> spare;
    std::vector<double> offsets;
    std::function<void(godot::Control *, int)> binder;
    int count = 0, virtual_count = 0, lanes = 1;
    double cycle_extent = 0, middle = 0;
    bool refreshing = false, force_bind = false, invalid_scene = false;
    virtual bool looping() const { return false; }
    virtual double padding() const { return 0; }
    virtual void scroll_step(double delta) {}
    virtual void recentered(double shift) {}
    virtual void rebuild();
    void clear_cells();
    void update_cells();
    double scroll_position() const;
    void move_scroll(double value);
    double view_extent() const;
    double item_position(int index) const;
    int index_at(double position) const;
public:
    VirtualList();
    void _process(double delta) override;
    godot::PackedStringArray _get_configuration_warnings() const override;
    GDMETHOD void refresh();
    GDMETHOD void scroll_to_index(int index);
    GDMETHOD int get_visible_node_count() const;
    GDMETHOD godot::Control *get_item_control(int index) const;
    GDEMIT_SIGNAL void bind_item(godot::Control *item, int index);
    GDEMIT_SIGNAL void unbind_item(godot::Control *item, int index);
    void set_binder(std::function<void(godot::Control *, int)> callback);
};
}
