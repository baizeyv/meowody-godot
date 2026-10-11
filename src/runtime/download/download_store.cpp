#include "download_store.h"
#include <godot_cpp/classes/config_file.hpp>
#include <godot_cpp/classes/dir_access.hpp>

namespace meowody::detail
{
bool DownloadStore::read_metadata(const godot::String& path, DownloadMetadata& meta)
{
    if (!godot::FileAccess::file_exists(path)) return false;
    godot::Ref<godot::ConfigFile> cfg;
    cfg.instantiate();
    if (cfg->load(path) != godot::OK) return false;
    meta.identity = cfg->get_value("download", "identity", "");
    meta.validator = cfg->get_value("download", "validator", "");
    meta.validator_header = cfg->get_value("download", "validator_header", "");
    meta.sha256 = cfg->get_value("download", "sha256", "");
    meta.total = cfg->get_value("download", "total", static_cast<int64_t>(-1));
    return !meta.identity.is_empty();
}
godot::Error DownloadStore::save_metadata(const godot::String& path, const DownloadMetadata& meta)
{
    godot::Ref<godot::ConfigFile> cfg;
    cfg.instantiate();
    cfg->set_value("download", "identity", meta.identity);
    cfg->set_value("download", "validator", meta.validator);
    cfg->set_value("download", "validator_header", meta.validator_header);
    cfg->set_value("download", "sha256", meta.sha256);
    cfg->set_value("download", "total", meta.total);
    const auto error = cfg->save(path + godot::String(".tmp"));
    if (error != godot::OK) return error;
    return godot::DirAccess::rename_absolute(path + godot::String(".tmp"), path);
}
bool DownloadStore::cached(const DownloadOptions& options) const
{
    DownloadMetadata completed;
    if (!godot::FileAccess::file_exists(path) || !read_metadata(path + godot::String(".cfg"), completed) || completed.identity != identity ||
        completed.total < 0 || completed.total > options.max_bytes || completed.total != godot::FileAccess::get_size(path)) return false;
    if (completed.sha256.is_empty()) return false;
    const auto digest = godot::FileAccess::get_sha256(path);
    return digest == completed.sha256 && (options.expected_sha256.is_empty() || digest == options.expected_sha256.to_lower());
}
godot::Error DownloadStore::prepare(bool resume, std::int64_t max_bytes, std::int64_t& offset)
{
    close();
    offset = 0;
    metadata = {};
    auto error = godot::DirAccess::make_dir_recursive_absolute(path.get_base_dir());
    if (error != godot::OK) return error;
    if (resume && read_metadata(resume_path(), metadata) && metadata.identity == identity &&
        !metadata.validator.is_empty() &&
        (metadata.validator_header == "etag" || metadata.validator_header == "last-modified") &&
        !metadata.validator.contains("\r") && !metadata.validator.contains("\n") &&
        godot::FileAccess::file_exists(part_path()))
    {
        offset = godot::FileAccess::get_size(part_path());
        if (offset >= 0 && offset <= max_bytes && (metadata.total < 0 || offset <= metadata.total)) return godot::OK;
    }
    metadata = {};
    offset = 0;
    return godot::OK;
}
godot::Error DownloadStore::open_body(bool append, const DownloadMetadata& meta)
{
    file = godot::FileAccess::open(part_path(), append ? godot::FileAccess::READ_WRITE : godot::FileAccess::WRITE);
    if (file.is_null()) return godot::FileAccess::get_open_error();
    if (append) file->seek_end();
    metadata = meta;
    const auto error = save_metadata(resume_path(), metadata);
    if (error != godot::OK) close();
    return error;
}
godot::Error DownloadStore::commit(const DownloadOptions& options, std::int64_t expected_size, DownloadError& error)
{
    godot::Error flush_error = godot::OK;
    if (file.is_valid()) { file->flush(); flush_error = file->get_error(); }
    close();
    if (flush_error != godot::OK || godot::FileAccess::get_size(part_path()) != expected_size)
    {
        error = DownloadError::FileIO;
        return flush_error != godot::OK ? flush_error : godot::ERR_FILE_CORRUPT;
    }
    metadata.sha256 = godot::FileAccess::get_sha256(part_path());
    if (metadata.sha256.is_empty()) { error = DownloadError::FileIO; return godot::ERR_FILE_CANT_READ; }
    if (!options.expected_sha256.is_empty() && metadata.sha256 != options.expected_sha256.to_lower())
    {
        // A corrupt complete partial must never be offered for resume.
        godot::DirAccess::remove_absolute(part_path());
        godot::DirAccess::remove_absolute(resume_path());
        error = DownloadError::Checksum;
        return godot::ERR_FILE_CORRUPT;
    }
    metadata.total = godot::FileAccess::get_size(part_path());
    auto result = godot::DirAccess::rename_absolute(part_path(), path);
    if (result == godot::OK) result = save_metadata(path + godot::String(".cfg"), metadata);
    if (result == godot::OK && godot::FileAccess::file_exists(resume_path())) godot::DirAccess::remove_absolute(resume_path());
    error = result == godot::OK ? DownloadError::None : DownloadError::FileIO;
    return result;
}
void DownloadStore::close()
{
    if (file.is_valid()) { file->flush(); file->close(); file.unref(); }
}
bool DownloadStore::remove()
{
    close();
    bool ok = true;
    for (const auto& suffix : {"", ".cfg", ".part", ".resume.cfg", ".cfg.tmp", ".resume.cfg.tmp"})
        if (godot::FileAccess::file_exists(path + godot::String(suffix)))
            ok = (godot::DirAccess::remove_absolute(path + godot::String(suffix)) == godot::OK) && ok;
    return ok;
}
}
