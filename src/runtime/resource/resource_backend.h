#pragma once

#include <memory>
#include "load_types.h"
#include <godot_cpp/classes/resource_loader.hpp>

namespace meowody
{
namespace detail {
// Internal test seam. Implementations are owned by the center and used only on its thread.
class ResourceBackend
{
public:
    virtual ~ResourceBackend() = default;
    virtual godot::String resolve_uid(const godot::String& path) = 0;
    virtual godot::Ref<godot::Resource> load(const godot::String& path) = 0;
    virtual godot::Error request(const godot::String& path) = 0;
    virtual godot::ResourceLoader::ThreadLoadStatus poll(const godot::String& path, float& progress) = 0;
    // Exactly once per successfully submitted request, including failure/cancellation/shutdown.
    virtual godot::Ref<godot::Resource> get(const godot::String& path) = 0;
};


std::unique_ptr<ResourceBackend> make_resource_backend();
}
}
