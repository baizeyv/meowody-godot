#include "model_fields.h"
#include "save_center.h"
#include "save_codec.h"
#include "log/log.hpp"
#include <godot_cpp/classes/dir_access.hpp>

namespace meowody {
using namespace godot;
SaveResult SaveCenter::inspect(const String& part, bool disk) const {
    auto status = validate(part);
    if (!status.ok()) { SaveResult r; r.error = status.error; r.message = status.message; return r; }
    return disk ? read_file(part_path(part)) : read_part(part);
}
static void warning(const String& part, const String& slot, const SaveStatus& status) {
    Log::warn(StringName("SaveCenter")) << part << "/" << slot << ": " << status.message << "; using initial data";
}
SaveStatus SaveCenter::save(const String& part, const String& slot, const Dictionary& data) {
    auto status = validate(part, slot); if (!status.ok()) return status;
    if (!detail::safe_variant(data)) return {SaveError::UnsupportedData, "Dictionary contains objects, handles, or an excessively deep/cyclic container"};
    auto container = read_part(part, false);
    if (!container.ok() && container.error != SaveError::Missing) return {container.error, container.message};
    container.data[slot] = data.duplicate(true);
    return stage_part(part, container.data);
}
SaveResult SaveCenter::load(const String& part, const String& slot, const Dictionary& defaults) const {
    auto status = validate(part, slot);
    auto result = status.ok() ? read_part(part, false) : SaveResult{};
    if (!status.ok()) { result.error = status.error; result.message = status.message; }
    if (result.ok()) {
        if (result.data.has(slot)) { result.data = Dictionary(result.data[slot]).duplicate(true); return result; }
        result.error = SaveError::Missing; result.message = "Save slot does not exist";
    }
    result.data = defaults.duplicate(true); result.used_default = true;
    warning(part, slot, result); return result;
}
SaveStatus SaveCenter::save_model(const String& part, const String& slot, Model& model) {
    const int64_t version = model.save_version();
    if (version < 0) return {SaveError::InvalidArgument, "Model version must be nonnegative"};
    const auto fields = model.save_fields();
    auto exported = detail::export_model(fields);
    if (!exported.ok()) return {exported.error, exported.message};
    exported.data[MODEL_VERSION_KEY] = version;
    return save(part, slot, exported.data);
}
ModelLoadResult SaveCenter::load_model(const String& part, const String& slot, Model& model) const {
    ModelLoadResult result;
    auto fail = [&](SaveStatus status) {
        result.error = status.error; result.message = status.message;
        return result;
    };
    result.current_version = model.save_version();
    if (result.current_version < 0)
        return fail({SaveError::InvalidArgument, "Model version must be nonnegative"});
    auto loaded = load(part, slot);
    if (!loaded.ok()) return fail({loaded.error, loaded.message});
    if (loaded.data.has(MODEL_VERSION_KEY)) {
        const Variant version = loaded.data[MODEL_VERSION_KEY];
        if (version.get_type() != Variant::INT || int64_t(version) < 0)
            return fail({SaveError::Corrupt, "Saved Model version must be a nonnegative integer"});
        result.saved_version = version;
    }
    if (result.saved_version > result.current_version)
        return fail({SaveError::UnsupportedVersion, "Saved Model version is newer than supported"});
    // load() already owns an isolated slot snapshot, including nested containers.
    Dictionary data = loaded.data;
    data.erase(MODEL_VERSION_KEY);
    for (int64_t version = result.saved_version; version < result.current_version; ++version) {
        auto status = model.migrate_save(version, data);
        if (!status.ok())
            return fail({SaveError::MigrationFailed, "Model migration from version " + String::num_int64(version) + ": " + status.message});
        if (data.has(MODEL_VERSION_KEY))
            return fail({SaveError::MigrationFailed, "Migration must not write reserved Model version metadata"});
    }
    // Reject unsupported nested data produced by migration before touching the Model.
    if (!detail::safe_variant(data))
        return fail({SaveError::UnsupportedData, "Migration produced unsupported save data"});
    const auto fields = model.save_fields();
    auto status = detail::import_model(fields, data);
    if (!status.ok()) return fail(status);
    result.migrated = result.saved_version != result.current_version;
    return result;
}
SaveStatus SaveCenter::save_object(const String& part, const String& slot, Object* object) {
    auto status = validate(part, slot); if (!status.ok()) return status;
    auto exported = detail::export_properties(object);
    if (!exported.ok()) return {exported.error, exported.message};
    Dictionary envelope; envelope["class"] = object->get_class(); envelope["properties"] = exported.data;
    return save(part, slot, envelope);
}
SaveStatus SaveCenter::load_into(const String& part, const String& slot, Object* object) const {
    if (!object) return {SaveError::InvalidArgument, "Initial object must not be null"};
    auto loaded = load(part, slot); if (!loaded.ok()) return {loaded.error, loaded.message};
    SaveStatus status;
    if (loaded.data.get("class", Variant()) != Variant(object->get_class()) ||
        loaded.data.get("properties", Variant()).get_type() != Variant::DICTIONARY)
        status = {SaveError::Corrupt, "Saved object class does not match initial object"};
    else status = detail::import_properties(object, loaded.data["properties"]);
    if (!status.ok()) warning(part, slot, status);
    return status;
}
bool SaveCenter::has_slot(const String& part, const String& slot) const {
    if (!validate(part, slot).ok()) return false;
    auto r = read_part(part, false); return r.ok() && r.data.has(slot);
}
PackedStringArray SaveCenter::list_slots(const String& part) const {
    PackedStringArray slots; auto r = read_part(part, false);
    if (r.ok()) { auto keys = r.data.keys(); for (int i = 0; i < keys.size(); ++i) slots.append(keys[i]); slots.sort(); }
    return slots;
}
PackedStringArray SaveCenter::list_parts() const {
    PackedStringArray parts; if (!validate("default").ok()) return parts;
    for (auto& [name, data] : m_pending) parts.append(name);
    auto dir = DirAccess::open(m_directory); if (dir.is_null()) return parts;
    auto files = dir->get_files();
    for (int i = 0; i < files.size(); ++i) if (files[i].ends_with(".sav") && !parts.has(files[i].get_basename())) parts.append(files[i].get_basename());
    parts.sort(); return parts;
}
SaveStatus SaveCenter::erase_slot(const String& part, const String& slot) {
    auto status = validate(part, slot); if (!status.ok()) return status;
    auto r = read_part(part, false); if (!r.ok()) return {r.error, r.message};
    r.data.erase(slot); return stage_part(part, r.data);
}
SaveStatus SaveCenter::reset_part(const String& part) { return stage_part(part, {}); }
}
