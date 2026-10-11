#include "save_center.h"
#include "save_codec.h"
#include "atomic_file.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

namespace meowody {
using namespace godot;
SaveCenter::SaveCenter(String directory, SaveOptions options) : m_directory(directory.simplify_path()), m_options(std::move(options)) {}
SaveStatus SaveCenter::configure(String directory, SaveOptions options) {
    if (!m_pending.empty()) return {SaveError::IO, "Flush pending saves before changing configuration"};
    auto previous_directory = m_directory; auto previous_options = m_options;
    m_directory = directory.simplify_path(); m_options = std::move(options);
    auto status = validate("default");
    if (!status.ok()) { m_directory = previous_directory; m_options = previous_options; }
    return status;
}
static bool valid_name(const String& name) {
    if (name.is_empty() || name.length() > 100 || name == "." || name == "..") return false;
    for (int i = 0; i < name.length(); ++i) {
        auto c = name[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}
SaveStatus SaveCenter::validate(const String& part, const String& slot) const {
    if (m_closed) return {SaveError::Closed, "SaveCenter is closed"};
    if (!valid_name(part) || !valid_name(slot) || !m_directory.begins_with("user://"))
        return {SaveError::InvalidArgument, "Use user:// directory and 1-100 character ASCII names: letters, digits, _ or -"};
    if (m_options.max_file_bytes < 128 || m_options.max_file_bytes > 1024LL * 1024 * 1024)
        return {SaveError::InvalidArgument, "max_file_bytes must be between 128 bytes and 1 GiB"};
    if (!m_options.encryption_key.is_empty() && m_options.encryption_key.size() != 32)
        return {SaveError::InvalidKey, "Encryption requires a 32-byte key"};
    return {};
}
String SaveCenter::part_path(const String& part) const {
    if (!valid_name(part)) return {};
    // Canonical lowercase prevents case-insensitive filesystems aliasing partitions.
    return m_directory.path_join(part.to_lower() + String(".sav"));
}
SaveResult SaveCenter::read_file(const String& path) const {
    SaveResult result;
    if (!FileAccess::file_exists(path)) { result.error = SaveError::Missing; result.message = "Save file does not exist"; return result; }
    auto raw = FileAccess::open(path, FileAccess::READ);
    if (raw.is_null()) { result.error = SaveError::IO; result.message = "Cannot read save file"; return result; }
    if (raw->get_length() > uint64_t(m_options.max_file_bytes + 1024)) {
        result.error = SaveError::Corrupt; result.message = "Save file exceeds configured limit"; return result;
    }
    raw.unref();
    auto file = m_options.encryption_key.is_empty() ? FileAccess::open(path, FileAccess::READ) :
        FileAccess::open_encrypted(path, FileAccess::READ, m_options.encryption_key);
    auto corrupt = [&]() { result.error = SaveError::Corrupt; result.message = "Save file is damaged, incompatible, or encrypted with another key"; return result; };
    if (file.is_null() || file->get_length() < 44 || file->get_length() > uint64_t(m_options.max_file_bytes)) return corrupt();
    if (file->get_32() != 0x4D534156 || file->get_32() != 1) return corrupt();
    uint32_t length = file->get_32();
    auto expected = file->get_buffer(32);
    if (uint64_t(length) + 44 != file->get_length()) return corrupt();
    auto bytes = file->get_buffer(length);
    if (bytes.size() != length || detail::digest(bytes) != expected) return corrupt();
    Variant decoded = UtilityFunctions::bytes_to_var(bytes);
    if (decoded.get_type() != Variant::DICTIONARY || !detail::safe_variant(decoded)) return corrupt();
    result.data = decoded;
    auto keys = result.data.keys();
    for (int i = 0; i < keys.size(); ++i)
        if (keys[i].get_type() != Variant::STRING || !valid_name(keys[i]) || result.data[keys[i]].get_type() != Variant::DICTIONARY) return corrupt();
    return result;
}
SaveResult SaveCenter::read_part(const String& part, bool deep_copy_pending) const {
    auto status = validate(part);
    if (!status.ok()) { SaveResult r; r.error = status.error; r.message = status.message; return r; }
    auto pending = m_pending.find(part.to_lower());
    if (pending != m_pending.end()) { SaveResult r; r.data = pending->second.duplicate(deep_copy_pending); return r; }
    return read_file(part_path(part));
}
SaveStatus SaveCenter::write_part(const String& part, const Dictionary& slots) {
    auto status = validate(part); if (!status.ok()) return status;
    auto bytes = UtilityFunctions::var_to_bytes(slots);
    if (bytes.size() + 44 > m_options.max_file_bytes) return {SaveError::InvalidArgument, "Partition exceeds max_file_bytes"};
    if (DirAccess::make_dir_recursive_absolute(m_directory) != OK) return {SaveError::IO, "Cannot create save directory"};
    String destination = part_path(part), temporary = destination + String(".tmp");
    auto file = m_options.encryption_key.is_empty() ? FileAccess::open(temporary, FileAccess::WRITE) :
        FileAccess::open_encrypted(temporary, FileAccess::WRITE, m_options.encryption_key);
    if (file.is_null()) return {SaveError::IO, "Cannot create temporary save"};
    bool written = file->store_32(0x4D534156) && file->store_32(1) && file->store_32(bytes.size()) &&
        file->store_buffer(detail::digest(bytes)) && file->store_buffer(bytes);
    file->flush();
    written = written && file->get_error() == OK;
    file->close(); file.unref(); // Encrypted buffers are finalized on close.
    if (written) {
        auto verified = read_file(temporary);
        written = verified.ok() && verified.data == slots;
    }
    if (!written) { DirAccess::remove_absolute(temporary); return {SaveError::IO, "Temporary save verification failed; original preserved"}; }
    status = detail::replace_durable(temporary, destination);
    if (status.ok()) ++m_commits;
    if (!status.ok()) DirAccess::remove_absolute(temporary);
    return status;
}
}
