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
#include "snap_scroll_container.h"
namespace meowody {
class LoopScrollContainer : public SnapScrollContainer {
    GDREFLECT_CLASS(LoopScrollContainer, SnapScrollContainer)
protected:
    bool looping() const override;
};
}
