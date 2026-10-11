#pragma once
#include "download_types.h"
#include <godot_cpp/classes/file_access.hpp>

namespace meowody::detail
{
struct DownloadMetadata
{
    godot::String identity, validator, validator_header, sha256;
    std::int64_t total = -1;
};
class DownloadStore
{
public:
    godot::String path;
    godot::String identity;
    DownloadMetadata metadata;
    godot::Ref<godot::FileAccess> file;
    godot::String part_path() const { return path + godot::String(".part"); }
    godot::String resume_path() const { return path + godot::String(".resume.cfg"); }
    bool cached(const DownloadOptions& options) const;
    godot::Error prepare(bool resume, std::int64_t max_bytes, std::int64_t& offset);
    godot::Error open_body(bool append, const DownloadMetadata& meta);
    godot::Error commit(const DownloadOptions& options, std::int64_t expected_size, DownloadError& error);
    void close();
    bool remove();
private:
    static bool read_metadata(const godot::String& path, DownloadMetadata& meta);
    static godot::Error save_metadata(const godot::String& path, const DownloadMetadata& meta);
};
}
