#include "resource_center.h"
namespace meowody {
LoadResult<> ResourceCenter::load_raw(const godot::String& address)
{
    if (m_closed) return {{}, LoadError::Closed, godot::OK, "ResourceCenter is closed"};
    auto error = m_catalog->prepare(address, false);
    if (error != LoadError::None) return {{}, error, godot::OK, "Address is unknown, not initialized, or requires asynchronous download"};
    if (auto resource = cached(address); resource.is_valid()) return {resource};
    auto resource = m_backend->load(m_catalog->path(address));
    return resource.is_valid() ? LoadResult<>{resource} : LoadResult<>{{}, LoadError::LoadFailed, godot::OK, "ResourceLoader returned no resource"};
}
}
