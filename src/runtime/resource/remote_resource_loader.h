#pragma once
#include "load_types.h"
#include <set>

namespace meowody::detail
{
struct RemotePreparation
{
    godot::String native_path;
    LoadResult<> result;
};
class RemoteResourceLoader
{
public:
    static bool supports(const godot::String& url);
    RemotePreparation prepare(const godot::String& local_file, const godot::String& pack_entry = {});
private:
    std::set<godot::String> m_mounted;
};
}
