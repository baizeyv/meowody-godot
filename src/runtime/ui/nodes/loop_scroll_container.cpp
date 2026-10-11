#include "loop_scroll_container.h"
#include "node_helpers.h"
#include <godot_cpp/classes/h_scroll_bar.hpp>
#include <godot_cpp/classes/v_scroll_bar.hpp>
#include <godot_cpp/classes/label.hpp>
#include <godot_cpp/classes/input.hpp>
namespace meowody {
using namespace godot;
bool LoopScrollContainer::looping() const { return true; }
}
