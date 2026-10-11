#include "model_fields.h"

namespace meowody {
SaveStatus Model::migrate_save(int64_t, godot::Dictionary &) const {
    return {SaveError::MigrationFailed, "No migration provided for this Model version"};
}

// Each supported type shares one read/write pair across all Model classes.
#define MODEL_FIELD(Type, Kind, Min, Max) \
ModelField model_field(const char *name, Type &value) { \
    return {name, &value, Kind, \
        [](const void *p) -> godot::Variant { return *static_cast<const Type *>(p); }, \
        [](void *p, const godot::Variant &v) { *static_cast<Type *>(p) = static_cast<Type>(v); }, Min, Max}; \
} \
ModelField model_field(const char *name, Reactive<Type> &value) { \
    return {name, &value, Kind, \
        [](const void *p) -> godot::Variant { return static_cast<const Reactive<Type> *>(p)->get(); }, \
        [](void *p, const godot::Variant &v) { static_cast<Reactive<Type> *>(p)->set(static_cast<Type>(v)); }, Min, Max}; \
}

MODEL_FIELD(bool, godot::Variant::BOOL, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(int8_t, godot::Variant::INT, std::numeric_limits<int8_t>::min(), std::numeric_limits<int8_t>::max())
MODEL_FIELD(uint8_t, godot::Variant::INT, 0, std::numeric_limits<uint8_t>::max())
MODEL_FIELD(int16_t, godot::Variant::INT, std::numeric_limits<int16_t>::min(), std::numeric_limits<int16_t>::max())
MODEL_FIELD(uint16_t, godot::Variant::INT, 0, std::numeric_limits<uint16_t>::max())
MODEL_FIELD(int32_t, godot::Variant::INT, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max())
MODEL_FIELD(uint32_t, godot::Variant::INT, 0, std::numeric_limits<uint32_t>::max())
MODEL_FIELD(int64_t, godot::Variant::INT, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(uint64_t, godot::Variant::INT, 0, std::numeric_limits<int64_t>::max())
MODEL_FIELD(float, godot::Variant::FLOAT, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(double, godot::Variant::FLOAT, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::String, godot::Variant::STRING, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::StringName, godot::Variant::STRING_NAME, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector2, godot::Variant::VECTOR2, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector2i, godot::Variant::VECTOR2I, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Rect2, godot::Variant::RECT2, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Rect2i, godot::Variant::RECT2I, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector3, godot::Variant::VECTOR3, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector3i, godot::Variant::VECTOR3I, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector4, godot::Variant::VECTOR4, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Vector4i, godot::Variant::VECTOR4I, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Transform2D, godot::Variant::TRANSFORM2D, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Plane, godot::Variant::PLANE, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Quaternion, godot::Variant::QUATERNION, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::AABB, godot::Variant::AABB, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Basis, godot::Variant::BASIS, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Transform3D, godot::Variant::TRANSFORM3D, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Projection, godot::Variant::PROJECTION, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Color, godot::Variant::COLOR, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::NodePath, godot::Variant::NODE_PATH, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Array, godot::Variant::ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Dictionary, godot::Variant::DICTIONARY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedByteArray, godot::Variant::PACKED_BYTE_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedInt32Array, godot::Variant::PACKED_INT32_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedInt64Array, godot::Variant::PACKED_INT64_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedFloat32Array, godot::Variant::PACKED_FLOAT32_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedFloat64Array, godot::Variant::PACKED_FLOAT64_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedStringArray, godot::Variant::PACKED_STRING_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedVector2Array, godot::Variant::PACKED_VECTOR2_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedVector3Array, godot::Variant::PACKED_VECTOR3_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedVector4Array, godot::Variant::PACKED_VECTOR4_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::PackedColorArray, godot::Variant::PACKED_COLOR_ARRAY, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
MODEL_FIELD(godot::Variant, godot::Variant::NIL, std::numeric_limits<int64_t>::min(), std::numeric_limits<int64_t>::max())
#undef MODEL_FIELD

namespace detail {
namespace {
bool valid_value(const ModelField &field, const godot::Variant &value) {
    if (value.get_type() == godot::Variant::OBJECT) return false;
    if (field.type != godot::Variant::NIL && value.get_type() != field.type) return false;
    if (field.type == godot::Variant::INT) {
        const int64_t integer = value;
        return integer >= field.minimum && integer <= field.maximum;
    }
    return true;
}
SaveStatus validate_fields(std::span<const ModelField> fields) {
    godot::Dictionary names;
    for (const auto &field : fields) {
        if (field.name.is_empty() || field.name == MODEL_VERSION_KEY || names.has(field.name) || !field.target || !field.read || !field.write)
            return {SaveError::InvalidArgument, "Model fields require unique nonempty nonreserved names and valid accessors"};
        names[field.name] = true;
    }
    return {};
}
}

SaveResult export_model(std::span<const ModelField> fields) {
    SaveResult result;
    auto status = validate_fields(fields);
    if (!status.ok()) { result.error = status.error; result.message = status.message; return result; }
    for (const auto &field : fields) {
        auto value = field.read(field.target);
        if (!valid_value(field, value)) {
            result.error = SaveError::UnsupportedData;
            result.message = "Model field cannot be represented as save data: " + field.name;
            return result;
        }
        result.data[field.name] = value;
    }
    return result;
}

SaveStatus import_model(std::span<const ModelField> fields, const godot::Dictionary &data) {
    auto status = validate_fields(fields);
    if (!status.ok()) return status;
    // Validate all fields before changing state or notifying Reactive subscribers.
    for (const auto &field : fields) {
        if (data.has(field.name) && !valid_value(field, data[field.name]))
            return {SaveError::Corrupt, "Saved model field type or range mismatch: " + field.name};
    }
    for (const auto &field : fields) {
        if (data.has(field.name)) field.write(field.target, data[field.name]);
    }
    return {};
}
} // namespace detail
} // namespace meowody
