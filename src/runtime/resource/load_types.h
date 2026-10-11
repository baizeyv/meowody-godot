#pragma once

#include <concepts>
#include <godot_cpp/classes/resource.hpp>

namespace meowody
{
enum class LoadStatus { Pending, Loading, Succeeded, Failed, Cancelled, Downloading };
enum class LoadError { None, InvalidPath, RequestFailed, LoadFailed, TypeMismatch, Cancelled, Closed, InvalidHandle,
                       DownloadFailed, RemoteRequiresAsync, UnsupportedRemoteFormat, RemoteDependencies, PackMountFailed };

inline bool is_load_finished(LoadStatus status)
{
    return status == LoadStatus::Succeeded || status == LoadStatus::Failed || status == LoadStatus::Cancelled;
}

template <typename T = godot::Resource> requires std::derived_from<T, godot::Resource>
struct LoadResult
{
    godot::Ref<T> resource;
    LoadError error = LoadError::None;
    // OK means no precise engine error was supplied, not necessarily success.
    godot::Error godot_error = godot::OK;
    godot::String message;
    explicit operator bool() const { return error == LoadError::None && resource.is_valid(); }
};

namespace detail {
template <typename T>
LoadResult<T> typed_result(const LoadResult<>& raw)
{
    LoadResult<T> result{godot::Ref<T>(raw.resource), raw.error, raw.godot_error, raw.message};
    if (raw.resource.is_valid() && result.resource.is_null() && result.error == LoadError::None)
    {
        result.error = LoadError::TypeMismatch;
        result.message = "Resource type does not match requested C++ type";
    }
    return result;
}
}
}
