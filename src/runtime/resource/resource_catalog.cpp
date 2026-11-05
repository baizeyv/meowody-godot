#include "resource_catalog.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#include <godot_cpp/classes/resource_uid.hpp>
#include <algorithm>

namespace meowody::detail {
using namespace godot;
namespace {
Dictionary read_json(const String& path) {
    auto file = FileAccess::open(path, FileAccess::READ);
    if (file.is_null()) return {};
    auto value = JSON::parse_string(file->get_as_text());
    return value.get_type() == Variant::DICTIONARY ? Dictionary(value) : Dictionary();
}
bool relative_file(const String& path) {
    return !path.is_empty() && !path.is_absolute_path() && !path.contains(":") && !path.contains("\\") &&
        !path.split("/").has("..") && !path.begins_with("/");
}
bool hash_valid(const String& hash) {
    if (hash.length() != 64) return false;
    for (int i = 0; i < hash.length(); ++i) if (!((hash[i] >= '0' && hash[i] <= '9') || (hash[i] >= 'a' && hash[i] <= 'f'))) return false;
    return true;
}
std::vector<String> strings(const Variant& value) {
    std::vector<String> result;
    if (value.get_type() != Variant::ARRAY) return result;
    Array array = value;
    for (int i = 0; i < array.size(); ++i) if (array[i].get_type() == Variant::STRING) result.push_back(array[i]);
    return result;
}
DownloadOptions pack_options(const String& hash, int64_t size) {
    DownloadOptions options;
    options.expected_sha256 = hash;
    options.max_bytes = std::max<int64_t>(size, 1);
    return options;
}
bool mounted_in_process(const String& path) {
    PackedStringArray mounted = ProjectSettings::get_singleton()->get_meta("meowody_resource_packs", PackedStringArray());
    return mounted.has(ProjectSettings::get_singleton()->globalize_path(path));
}
}
ResourceOperation ResourceCatalog::initialize(const ResourceOptions& options) {
    if (m_init && ((!is_load_finished(m_init->status) || m_init->status == LoadStatus::Succeeded) || m_init->error == LoadError::RestartRequired)) return ResourceOperation(m_init);
    m_options = options;
    m_init = std::make_shared<ResourceOperationState>();
    if (m_closed) { fail(LoadError::Closed, "ResourceCenter is closed"); return ResourceOperation(m_init); }
    m_init->status = LoadStatus::Loading;
    m_operations.clear(); m_packs.clear(); m_entries.clear();
    m_pointer = {}; m_catalog_download = {}; m_fallback_stage = 0;
    auto settings = read_json(options.settings_path);
    m_source = options.mode == ResourceMode::Source || (options.mode == ResourceMode::Auto && OS::get_singleton()->has_feature("editor") && String(settings.get("play_mode", "source")) == "source");
    if (m_source) {
        if (settings.get("entries", Variant()).get_type() != Variant::ARRAY) { fail(LoadError::InvalidCatalog, "Missing source entries"); return ResourceOperation(m_init); }
        Array entries = settings.get("entries", Array());
        for (int i = 0; i < entries.size(); ++i) {
            if (entries[i].get_type() != Variant::DICTIONARY) { fail(LoadError::InvalidCatalog, "Invalid source entry"); return ResourceOperation(m_init); }
            Dictionary item = entries[i];
            String uid = item.get("uid", "");
            auto* registry = ResourceUID::get_singleton();
            auto id = uid.begins_with("uid://") ? registry->text_to_id(uid) : ResourceUID::INVALID_ID;
            if (registry->has_id(id)) item["path"] = registry->get_id_path(id);
            entries[i] = item;
        }
        settings["entries"] = entries;
        auto error = parse(settings);
        if (!error.is_empty()) fail(LoadError::InvalidCatalog, error);
        else m_init->status = LoadStatus::Succeeded;
        return ResourceOperation(m_init);
    }
    m_builtin = read_json(options.catalog_path);
    m_remote = m_builtin.get("remote_root", "");
    String platform = m_builtin.get("platform", ""), compatibility = m_builtin.get("compatibility", "");
    auto* os = OS::get_singleton();
    String host = os->has_feature("android") ? "android" : os->has_feature("ios") ? "ios" : os->has_feature("windows") ? "windows" : "";
    if (platform != host || host.is_empty()) { fail(LoadError::IncompatibleContent, "Built-in catalog does not match this platform"); return ResourceOperation(m_init); }
    m_cached_catalog = options.cache_directory.path_join((platform + ":" + compatibility + ":" + m_remote).sha256_text()).path_join("catalog.json");
    if (m_remote.begins_with("https://") || m_remote.begins_with("http://")) {
        DownloadOptions fresh; fresh.use_cache = false; fresh.resume = false; fresh.max_bytes = 1024 * 1024;
        fresh.timeout_seconds = 5.0; fresh.max_retries = 0;
        m_pointer = m_downloads.download(m_remote.path_join("latest.json"), fresh);
        m_selecting = true;
    } else fallback("No remote endpoint configured");
    return ResourceOperation(m_init);
}
String ResourceCatalog::parse(const Dictionary& catalog) {
    m_packs.clear(); m_entries.clear();
    if (int(catalog.get("schema", 0)) != 1 || catalog.get("entries", Variant()).get_type() != Variant::ARRAY) return "Unsupported or missing catalog schema/entries";
    if (!m_source) {
        if (catalog.get("packs", Variant()).get_type() != Variant::DICTIONARY) return "Missing pack table";
        String platform = catalog.get("platform", ""), compatibility = catalog.get("compatibility", "");
        if (platform != String(m_builtin.get("platform", "")) || compatibility.is_empty() || compatibility != String(m_builtin.get("compatibility", ""))) return "Platform or application content compatibility mismatch";
        Dictionary packs = catalog["packs"];
        Array ids = packs.keys();
        for (int i = 0; i < ids.size(); ++i) {
            String id = ids[i];
            if (packs[id].get_type() != Variant::DICTIONARY) return "Invalid pack record";
            Dictionary record = packs[id];
            Pack pack;
            pack.file = record.get("file", ""); pack.sha256 = record.get("sha256", "");
            pack.size = record.get("size", int64_t(0)); pack.dependencies = strings(record.get("dependencies", Array()));
            pack.local = record.get("local", "");
            if (!hash_valid(pack.sha256) || pack.file != "packs/" + pack.sha256 + ".pck" || pack.size <= 0 || (!pack.local.is_empty() && pack.local != "res://resources/" + pack.file)) return "Invalid pack path, hash, or size";
            if (record.get("dependencies", Variant()).get_type() != Variant::ARRAY) return "Invalid dependency list";
            Array dependency_values = record["dependencies"];
            for (int j = 0; j < dependency_values.size(); ++j) if (dependency_values[j].get_type() != Variant::STRING) return "Invalid dependency identifier";
            pack.url = m_remote.path_join(pack.file);
            m_packs[id] = std::move(pack);
        }
        for (const auto& [id, pack] : m_packs) {
            std::vector<String> ordered; std::set<String> visiting;
            if (!closure(id, ordered, visiting)) return "Missing or cyclic pack dependency";
        }
    }
    Array entries = catalog["entries"];
    for (int i = 0; i < entries.size(); ++i) {
        if (entries[i].get_type() != Variant::DICTIONARY) return "Invalid entry record";
        Dictionary record = entries[i];
        if (record.get("address", Variant()).get_type() != Variant::STRING || record.get("path", Variant()).get_type() != Variant::STRING || record.get("labels", Array()).get_type() != Variant::ARRAY) return "Invalid address, path or labels";
        Array labels = record.get("labels", Array());
        for (int j = 0; j < labels.size(); ++j) if (labels[j].get_type() != Variant::STRING) return "Invalid resource label";
        String address = record.get("address", "");
        Entry entry; entry.path = record.get("path", ""); entry.pack = record.get("pack", "");
        entry.group = record.get("group", ""); entry.labels = strings(record.get("labels", Array()));
        if (address.is_empty() || address.contains("://") || m_entries.contains(address) || !entry.path.begins_with("res://") || entry.path.simplify_path() != entry.path || (!m_source && !entry.pack.is_empty() && !m_packs.contains(entry.pack))) return "Invalid or duplicate resource address/path/pack";
        m_entries.emplace(address, std::move(entry));
    }
    return {};
}
bool ResourceCatalog::closure(const String& id, std::vector<String>& output, std::set<String>& visiting) const {
    if (std::find(output.begin(), output.end(), id) != output.end()) return true;
    auto found = m_packs.find(id);
    if (found == m_packs.end() || !visiting.insert(id).second) return false;
    for (const auto& dependency : found->second.dependencies) if (!closure(dependency, output, visiting)) return false;
    visiting.erase(id); output.push_back(id); return true;
}
std::vector<String> ResourceCatalog::dependencies(const String& address) const {
    std::vector<String> result; std::set<String> visiting;
    auto entry = m_entries.find(address);
    if (entry != m_entries.end() && !m_source && !entry->second.pack.is_empty()) closure(entry->second.pack, result, visiting);
    return result;
}
String ResourceCatalog::pack_path(const Pack& pack) const {
    if (!pack.verified_path.is_empty() && FileAccess::file_exists(pack.verified_path) && FileAccess::get_size(pack.verified_path) == pack.size) return pack.verified_path;
    if (!pack.local.is_empty() && FileAccess::file_exists(pack.local) && FileAccess::get_size(pack.local) == pack.size && FileAccess::get_sha256(pack.local) == pack.sha256) pack.verified_path = pack.local;
    else pack.verified_path = m_downloads.get_cached_path(pack.url, pack_options(pack.sha256, pack.size));
    return pack.verified_path;
}
LoadError ResourceCatalog::advance(Operation& operation, bool start_download) {
    operation.state->downloaded_bytes = operation.state->total_bytes = 0;
    bool done = true;
    for (const auto& id : operation.packs) {
        auto& pack = m_packs.at(id);
        operation.state->total_bytes += pack.size;
        if (!pack_path(pack).is_empty()) { operation.state->downloaded_bytes += pack.size; continue; }
        if (!start_download) return LoadError::RemoteRequiresAsync;
        if (!pack.download.valid()) pack.download = m_downloads.download(pack.url, pack_options(pack.sha256, pack.size));
        if (pack.download.finished() && !pack.download.result()) { pack.download = {}; return LoadError::DownloadFailed; }
        operation.state->downloaded_bytes += pack.download.downloaded_bytes();
        done = false;
    }
    if (done) operation.state->status = LoadStatus::Succeeded;
    return LoadError::None;
}
bool ResourceCatalog::choose(const Dictionary& catalog) {
    auto error = parse(catalog);
    if (!error.is_empty()) { m_init->message = error; return false; }
    m_candidate = catalog;
    if (catalog.get("startup", Variant()).get_type() != Variant::ARRAY) { m_init->message = "Missing startup pack list"; return false; }
    Operation operation{m_init, {}};
    for (const auto& id : strings(catalog.get("startup", Array()))) {
        std::set<String> visiting;
        if (!closure(id, operation.packs, visiting)) { m_init->message = "Invalid startup pack"; return false; }
    }
    m_operations = {operation}; m_selecting = false; return true;
}
void ResourceCatalog::fallback(const String& reason) {
    m_init->fallback = true; m_init->message = reason;
    if (m_fallback_stage == 0) {
        m_fallback_stage = 1;
        auto cached = read_json(m_cached_catalog);
        if (!cached.is_empty() && choose(cached)) return;
    }
    if (m_fallback_stage == 1) { m_fallback_stage = 2; if (choose(m_builtin)) return; }
    fail(LoadError::InvalidCatalog, "No usable content version: " + reason);
}
void ResourceCatalog::fail(LoadError error, const String& message) {
    m_init->status = LoadStatus::Failed; m_init->error = error; m_init->message = message;
    m_selecting = false;
}
void ResourceCatalog::update() {
    if (m_closed || !m_init) return;
    if (m_init->status == LoadStatus::Cancelled) { m_operations.clear(); m_selecting = false; return; }
    if (m_selecting && m_pointer.finished()) {
        if (!m_pointer.result()) { fallback("Remote version check failed"); return; }
        if (!m_catalog_download.valid()) {
            auto pointer = read_json(m_pointer.result().path);
            String file = pointer.get("catalog", ""), hash = pointer.get("sha256", "");
            if (!relative_file(file) || !hash_valid(hash)) { fallback("Invalid version pointer"); return; }
            auto options = pack_options(hash, 16 * 1024 * 1024);
            m_catalog_download = m_downloads.download(m_remote.path_join(file), options);
        }
        if (m_catalog_download.finished()) {
            if (!m_catalog_download.result() || !choose(read_json(m_catalog_download.result().path))) fallback("Remote catalog unavailable or incompatible");
        }
    }
    for (auto& operation : m_operations) {
        if (is_load_finished(operation.state->status)) continue;
        auto error = advance(operation, true);
        if (error != LoadError::None) {
            if (operation.state == m_init) { fallback("Required startup content could not be downloaded"); return; }
            operation.state->status = LoadStatus::Failed; operation.state->error = error; operation.state->message = "Dependency download failed";
        } else if (operation.state == m_init && m_init->status == LoadStatus::Succeeded) {
            auto absolute = ProjectSettings::get_singleton()->globalize_path(m_cached_catalog);
            auto result = DirAccess::make_dir_recursive_absolute(absolute.get_base_dir());
            auto file = result == OK ? FileAccess::open(absolute + ".tmp", FileAccess::WRITE) : Ref<FileAccess>();
            if (file.is_null() || !file->store_string(JSON::stringify(m_candidate))) { fail(LoadError::LoadFailed, "Cannot persist selected catalog"); return; }
            file->flush(); file.unref();
            if (DirAccess::rename_absolute(absolute + ".tmp", absolute) != OK) { fail(LoadError::LoadFailed, "Cannot commit selected catalog"); return; }
        }
    }
    std::erase_if(m_operations, [](const Operation& op) { return is_load_finished(op.state->status); });
}
bool ResourceCatalog::idle() const { return !m_selecting && m_operations.empty(); }
String ResourceCatalog::path(const String& address) const {
    auto entry = m_entries.find(address); return entry == m_entries.end() ? String() : entry->second.path;
}
String ResourceCatalog::address_for_path(const String& path) const {
    for (const auto& [address, entry] : m_entries) if (entry.path == path) return address;
    return {};
}
std::vector<String> ResourceCatalog::select(const String& name, bool label) const {
    std::vector<String> result;
    for (const auto& [address, entry] : m_entries) if (label ? std::find(entry.labels.begin(), entry.labels.end(), name) != entry.labels.end() : entry.group == name) result.push_back(address);
    return result;
}
int64_t ResourceCatalog::download_size(const String& address) const {
    if (!ready() || path(address).is_empty()) return -1;
    int64_t bytes = 0; for (const auto& id : dependencies(address)) { const auto& pack = m_packs.at(id); if (pack_path(pack).is_empty()) bytes += pack.size; }
    return bytes;
}
bool ResourceCatalog::cached(const String& address) const { return download_size(address) == 0; }
float ResourceCatalog::download_progress(const String& address) const {
    double total = 0, downloaded = 0;
    for (const auto& id : dependencies(address)) {
        const auto& pack = m_packs.at(id);
        total += pack.size;
        downloaded += !pack_path(pack).is_empty() ? pack.size : pack.download.downloaded_bytes();
    }
    return total > 0 ? float(std::clamp(downloaded / total, 0.0, 1.0)) : 0.0f;
}
bool ResourceCatalog::clear_cached(const String& address) {
    if (!ready() || path(address).is_empty()) return false;
    auto packs = dependencies(address);
    for (const auto& id : packs) if (m_packs.at(id).mounted || mounted_in_process(pack_path(m_packs.at(id)))) return false;
    bool result = true;
    for (const auto& id : packs) {
        auto& pack = m_packs.at(id);
        if (pack.download.valid() && !pack.download.finished()) return false;
        auto options = pack_options(pack.sha256, pack.size);
        if (m_downloads.has_cached(pack.url, options)) result &= m_downloads.remove_cached(pack.url, options);
        pack.download = {};
        pack.verified_path = String();
    }
    return result;
}
ResourceOperation ResourceCatalog::download_dependencies(const String& address) {
    auto state = std::make_shared<ResourceOperationState>();
    if (!ready() || path(address).is_empty()) { state->status = LoadStatus::Failed; state->error = ready() ? LoadError::UnknownAddress : LoadError::NotInitialized; }
    else { state->status = LoadStatus::Downloading; m_operations.push_back({state, dependencies(address)}); }
    return ResourceOperation(state);
}
LoadError ResourceCatalog::prepare(const String& address, bool start_download) {
    if (!ready()) return m_init && m_init->status == LoadStatus::Failed ? m_init->error : LoadError::NotInitialized;
    if (path(address).is_empty()) return LoadError::UnknownAddress;
    if (m_source) return LoadError::None;
    auto state = std::make_shared<ResourceOperationState>();
    Operation operation{state, dependencies(address)};
    auto error = advance(operation, start_download);
    if (error != LoadError::None) return error;
    if (state->status != LoadStatus::Succeeded) return LoadError::RemoteRequiresAsync;
    for (const auto& id : operation.packs) {
        auto& pack = m_packs.at(id);
        if (!pack.mounted) {
            auto path = pack_path(pack);
            auto* settings = ProjectSettings::get_singleton();
            if (!mounted_in_process(path)) {
                if (!settings->load_resource_pack(path, true)) { fail(LoadError::RestartRequired, "Pack mount failed; restart required"); return LoadError::RestartRequired; }
                PackedStringArray mounted = settings->get_meta("meowody_resource_packs", PackedStringArray());
                mounted.append(settings->globalize_path(path));
                settings->set_meta("meowody_resource_packs", mounted);
            }
            pack.mounted = true;
        }
    }
    return LoadError::None;
}
void ResourceCatalog::shutdown() {
    m_closed = true;
    if (m_init && !is_load_finished(m_init->status)) ResourceOperation(m_init).cancel();
    for (auto& operation : m_operations) ResourceOperation(operation.state).cancel();
    m_operations.clear(); m_selecting = false;
}
}
