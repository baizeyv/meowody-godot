#pragma once

#include "../load_types.h"
#include "runtime/core/delegate.hpp"

namespace meowody
{
namespace detail {
struct LoadState
{
    godot::String path;
    LoadStatus status = LoadStatus::Pending;
    float progress = 0.0f;
    LoadResult<> result;
    bool suppress_callback = false;
    Delegate<bool(const godot::Ref<godot::Resource>&)> accepts;
    Delegate<void()> callback;
    void cancel();
};

}
}
