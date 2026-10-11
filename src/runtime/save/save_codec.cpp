#include "save_codec.h"
#include <godot_cpp/classes/hashing_context.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <vector>
#include <set>

namespace meowody::detail {
using namespace godot;
PackedByteArray digest(const PackedByteArray& bytes) {
    Ref<HashingContext> hash; hash.instantiate();
    hash->start(HashingContext::HASH_SHA256); hash->update(bytes); return hash->finish();
}
bool safe_variant(const Variant& value, int depth) {
    if (depth > 48) return false;
    switch (value.get_type()) {
        case Variant::OBJECT: case Variant::CALLABLE: case Variant::SIGNAL: case Variant::RID: return false;
        case Variant::ARRAY: {
            Array a = value; for (int i = 0; i < a.size(); ++i) if (!safe_variant(a[i], depth + 1)) return false;
            return true;
        }
        case Variant::DICTIONARY: {
            Dictionary d = value; Array keys = d.keys();
            for (int i = 0; i < keys.size(); ++i)
                if (!safe_variant(keys[i], depth + 1) || !safe_variant(d[keys[i]], depth + 1)) return false;
            return true;
        }
        default: return true;
    }
}
static bool stored(const Dictionary& property) {
    int64_t usage = property.get("usage", 0);
    String name = property.get("name", "");
    return (usage & PROPERTY_USAGE_STORAGE) && !(usage & PROPERTY_USAGE_READ_ONLY) && name != "script";
}
static SaveResult capture(Object* object, std::set<uint64_t>& visiting, int depth) {
    SaveResult result;
    if (!object || depth > 32 || !visiting.insert(object->get_instance_id()).second) {
        result.error = SaveError::UnsupportedData; result.message = "Null or cyclic exported object graph"; return result;
    }
    auto properties = object->get_property_list();
    for (int i = 0; i < properties.size(); ++i) {
        Dictionary p = properties[i]; if (!stored(p)) continue;
        StringName name = p["name"]; Variant value = object->get(name);
        Dictionary entry; entry["type"] = int(value.get_type());
        if (value.get_type() == Variant::OBJECT) {
            Object* child = value;
            if (child) {
                auto nested = capture(child, visiting, depth + 1);
                if (!nested.ok()) { visiting.erase(object->get_instance_id()); return nested; }
                entry["class"] = child->get_class(); entry["value"] = nested.data;
            } else entry["value"] = Variant();
        } else {
            if (!safe_variant(value)) {
                result.error = SaveError::UnsupportedData;
                result.message = String("Unsupported exported property: ") + String(name);
                visiting.erase(object->get_instance_id()); return result;
            }
            entry["value"] = value;
        }
        result.data[name] = entry;
    }
    visiting.erase(object->get_instance_id()); return result;
}
SaveResult export_properties(Object* object) { std::set<uint64_t> visiting; return capture(object, visiting, 0); }
struct Assignment { Object* object; StringName name; Variant value; };
static bool prepare(Object* object, const Dictionary& data, std::vector<Assignment>& assignments, int depth) {
    if (!object || depth > 32) return false;
    auto properties = object->get_property_list();
    for (int i = 0; i < properties.size(); ++i) {
        Dictionary p = properties[i]; if (!stored(p)) continue;
        StringName name = p["name"]; if (!data.has(name)) continue; // New properties retain initial values.
        if (data[name].get_type() != Variant::DICTIONARY) return false;
        Dictionary entry = data[name];
        if (!entry.has("type") || entry["type"].get_type() != Variant::INT || !entry.has("value")) return false;
        Variant current = object->get(name), value = entry["value"];
        int type = entry["type"];
        if (type != current.get_type()) return false;
        if (type == Variant::OBJECT) {
            if (value.get_type() == Variant::NIL) { assignments.push_back({object, name, Variant()}); continue; }
            Object* child = current;
            if (!child || value.get_type() != Variant::DICTIONARY || !entry.has("class") ||
                String(entry["class"]) != String(child->get_class()) || !prepare(child, value, assignments, depth + 1)) return false;
        } else {
            if (value.get_type() != type || !safe_variant(value)) return false;
            assignments.push_back({object, name, value});
        }
    }
    return true;
}
SaveStatus import_properties(Object* object, const Dictionary& properties) {
    std::vector<Assignment> assignments;
    if (!prepare(object, properties, assignments, 0)) return {SaveError::Corrupt, "Exported property schema or nested defaults do not match"};
    for (const auto& a : assignments) a.object->set(a.name, a.value);
    return {};
}
}
