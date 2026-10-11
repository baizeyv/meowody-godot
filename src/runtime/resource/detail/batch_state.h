#pragma once

#include "../batch_load_handle.h"

namespace meowody
{
namespace detail {
struct BatchState
{
    godot::String group;
    std::uint64_t generation = 0;
    LoadStatus status = LoadStatus::Pending;
    std::vector<LoadHandle<>> items;
    std::vector<godot::String> paths;
    bool suppress_callback = false;
    Delegate<void()> callback;
};
}
}
