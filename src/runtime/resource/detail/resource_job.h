#pragma once
#include "load_state.h"
#include "runtime/download/download_handle.h"

namespace meowody::detail
{
struct ResourceJob
{
    std::vector<std::shared_ptr<LoadState>> requests;
    godot::String local_path;
    godot::String source_url;
    godot::String pack_entry;
    DownloadHandle download;
    bool submitted = false;
};
}
