#pragma once
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource.hpp>

namespace meowody {
enum class SaveError { None, InvalidArgument, Missing, Corrupt, IO, InvalidKey, UnsupportedData, Closed, UnsupportedVersion, MigrationFailed };
struct SaveStatus {
    SaveError error = SaveError::None;
    godot::String message;
    bool ok() const { return error == SaveError::None; }
    explicit operator bool() const { return ok(); }
};
struct SaveResult : SaveStatus {
    godot::Dictionary data;
    bool used_default = false;
};
struct ModelLoadResult : SaveStatus {
    int64_t saved_version = 0;
    int64_t current_version = 0;
    bool migrated = false;
};
template<class T> struct SavedResource : SaveStatus {
    godot::Ref<T> data;
    bool used_default = false;
};
struct SaveOptions {
    bool defer_writes = true;
    // Empty means plaintext. Supply exactly 32 bytes for encrypted files.
    godot::PackedByteArray encryption_key;
    int64_t max_file_bytes = 64 * 1024 * 1024;
};
}
