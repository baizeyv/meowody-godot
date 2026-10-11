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
#include "virtual_list.h"
namespace meowody {
class SnapScrollContainer : public VirtualList {
    GDREFLECT_CLASS(SnapScrollContainer, VirtualList)
protected:
    GDPROPERTY_ENUM("Item,Page,Center") int snap_mode = 2;
    GDPROPERTY_RANGE(0, 3, 0.01) double snap_duration = 0.25;
    GDPROPERTY godot::Ref<godot::Curve> snap_curve;
    GDPROPERTY_RANGE(0, 10000, 10) double velocity_threshold = 500;
    GDPROPERTY_RANGE(0, 1000, 1) double overscroll = 0;
    double last_position = 0, idle = 0, velocity = 0, elapsed = 0, from = 0, destination = 0;
    int current_index = -1, requested_index = -1;
    bool snapping = false, unsettled = false, touch_scrolling = false;
    void begin_scroll();
    void end_scroll();
    void rebuild() override;
    double padding() const override;
    void scroll_step(double delta) override;
    void recentered(double shift) override;
    double target_position(int index) const;
public:
    SnapScrollContainer();
    ~SnapScrollContainer() override;
    GDMETHOD int get_current_index() const;
    GDMETHOD void set_current_index(int index, bool animated);
    GDMETHOD void snap_now();
    GDEMIT_SIGNAL void index_changed(int index);
    GDEMIT_SIGNAL void snap_started();
    GDEMIT_SIGNAL void snap_finished();
};
}
