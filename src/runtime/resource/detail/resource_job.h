#pragma once
#include "load_state.h"

namespace meowody::detail
{
struct ResourceJob
{
    std::vector<std::shared_ptr<LoadState>> requests;
    godot::String local_path;
    bool submitted = false;
};
}
