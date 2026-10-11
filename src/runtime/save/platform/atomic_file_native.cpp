#include "../atomic_file.h"
#include <godot_cpp/classes/project_settings.hpp>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#endif

namespace meowody::detail {
SaveStatus replace_durable(const godot::String& temporary, const godot::String& destination) {
    auto* settings = godot::ProjectSettings::get_singleton();
    auto from = settings->globalize_path(temporary);
    auto to = settings->globalize_path(destination);
#ifdef _WIN32
    auto wide_from = from.utf16();
    auto wide_to = to.utf16();
    HANDLE file = CreateFileW(reinterpret_cast<LPCWSTR>(wide_from.get_data()), GENERIC_WRITE,
        FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return {SaveError::IO, "Cannot open temporary file for durable flush"};
    bool flushed = FlushFileBuffers(file) != 0;
    CloseHandle(file);
    if (!flushed) return {SaveError::IO, "Cannot flush temporary file"};
    if (!MoveFileExW(reinterpret_cast<LPCWSTR>(wide_from.get_data()), reinterpret_cast<LPCWSTR>(wide_to.get_data()),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
        return {SaveError::IO, "Atomic replacement failed; original file preserved"};
#else
    int file = ::open(from.utf8().get_data(), O_WRONLY);
    if (file < 0) return {SaveError::IO, "Cannot open temporary file for durable flush"};
    bool flushed = ::fsync(file) == 0;
    ::close(file);
    if (!flushed) return {SaveError::IO, "Cannot flush temporary file"};
    if (::rename(from.utf8().get_data(), to.utf8().get_data()) != 0)
        return {SaveError::IO, "Atomic replacement failed; original file preserved"};
    int directory = ::open(to.get_base_dir().utf8().get_data(), O_RDONLY | O_DIRECTORY);
    bool durable = directory >= 0 && ::fsync(directory) == 0;
    if (directory >= 0) ::close(directory);
    if (!durable) return {SaveError::IO, "Replacement completed but directory flush failed; durability uncertain"};
#endif
    return {};
}
}
