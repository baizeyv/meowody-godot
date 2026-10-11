#include "resource_backend.h"
#include <godot_cpp/classes/resource_uid.hpp>

namespace meowody::detail
{
namespace {
class GodotResourceBackend final : public detail::ResourceBackend
{
public:
    godot::String resolve_uid(const godot::String& path) override
    {
        auto* uid = godot::ResourceUID::get_singleton();
        const auto id = uid->text_to_id(path);
        return uid->has_id(id) ? uid->get_id_path(id) : godot::String();
    }
    godot::Ref<godot::Resource> load(const godot::String& path) override
    {
        return godot::ResourceLoader::get_singleton()->load(path, "", godot::ResourceLoader::CACHE_MODE_REUSE);
    }
    godot::Error request(const godot::String& path) override
    {
        return godot::ResourceLoader::get_singleton()->load_threaded_request(
            path, "", false, godot::ResourceLoader::CACHE_MODE_REUSE);
    }
    godot::ResourceLoader::ThreadLoadStatus poll(const godot::String& path, float& progress) override
    {
        godot::Array values;
        const auto status = godot::ResourceLoader::get_singleton()->load_threaded_get_status(path, values);
        if (!values.is_empty()) progress = static_cast<float>(static_cast<double>(values[0]));
        return status;
    }
    godot::Ref<godot::Resource> get(const godot::String& path) override
    {
        return godot::ResourceLoader::get_singleton()->load_threaded_get(path);
    }
};
}
std::unique_ptr<ResourceBackend> make_resource_backend() { return std::make_unique<GodotResourceBackend>(); }
}
