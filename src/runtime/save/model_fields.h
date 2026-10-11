#pragma once

#include "save_types.h"
#include "runtime/reactive/reactive_value.hpp"
#include <godot_cpp/variant/variant.hpp>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace meowody {
inline constexpr const char *MODEL_VERSION_KEY = "__meowody_model_version";
// Non-owning field references; create these only while their Model is alive.
struct ModelField {
    godot::String name;
    void *target;
    godot::Variant::Type type;
    godot::Variant (*read)(const void *);
    void (*write)(void *, const godot::Variant &);
    int64_t minimum = std::numeric_limits<int64_t>::min();
    int64_t maximum = std::numeric_limits<int64_t>::max();
};

class Model {
public:
    virtual ~Model() = default;
    virtual std::vector<ModelField> save_fields() = 0;
    virtual int64_t save_version() const { return 0; }
    // Transform only the supplied snapshot from from_version to from_version + 1.
    virtual SaveStatus migrate_save(int64_t from_version, godot::Dictionary &data) const;
};

ModelField model_field(const char *name, bool &value);
ModelField model_field(const char *name, Reactive<bool> &value);
ModelField model_field(const char *name, int8_t &value);
ModelField model_field(const char *name, Reactive<int8_t> &value);
ModelField model_field(const char *name, uint8_t &value);
ModelField model_field(const char *name, Reactive<uint8_t> &value);
ModelField model_field(const char *name, int16_t &value);
ModelField model_field(const char *name, Reactive<int16_t> &value);
ModelField model_field(const char *name, uint16_t &value);
ModelField model_field(const char *name, Reactive<uint16_t> &value);
ModelField model_field(const char *name, int32_t &value);
ModelField model_field(const char *name, Reactive<int32_t> &value);
ModelField model_field(const char *name, uint32_t &value);
ModelField model_field(const char *name, Reactive<uint32_t> &value);
ModelField model_field(const char *name, int64_t &value);
ModelField model_field(const char *name, Reactive<int64_t> &value);
ModelField model_field(const char *name, uint64_t &value);
ModelField model_field(const char *name, Reactive<uint64_t> &value);
ModelField model_field(const char *name, float &value);
ModelField model_field(const char *name, Reactive<float> &value);
ModelField model_field(const char *name, double &value);
ModelField model_field(const char *name, Reactive<double> &value);
ModelField model_field(const char *name, godot::String &value);
ModelField model_field(const char *name, Reactive<godot::String> &value);
ModelField model_field(const char *name, godot::StringName &value);
ModelField model_field(const char *name, Reactive<godot::StringName> &value);
ModelField model_field(const char *name, godot::Vector2 &value);
ModelField model_field(const char *name, Reactive<godot::Vector2> &value);
ModelField model_field(const char *name, godot::Vector2i &value);
ModelField model_field(const char *name, Reactive<godot::Vector2i> &value);
ModelField model_field(const char *name, godot::Rect2 &value);
ModelField model_field(const char *name, Reactive<godot::Rect2> &value);
ModelField model_field(const char *name, godot::Rect2i &value);
ModelField model_field(const char *name, Reactive<godot::Rect2i> &value);
ModelField model_field(const char *name, godot::Vector3 &value);
ModelField model_field(const char *name, Reactive<godot::Vector3> &value);
ModelField model_field(const char *name, godot::Vector3i &value);
ModelField model_field(const char *name, Reactive<godot::Vector3i> &value);
ModelField model_field(const char *name, godot::Vector4 &value);
ModelField model_field(const char *name, Reactive<godot::Vector4> &value);
ModelField model_field(const char *name, godot::Vector4i &value);
ModelField model_field(const char *name, Reactive<godot::Vector4i> &value);
ModelField model_field(const char *name, godot::Transform2D &value);
ModelField model_field(const char *name, Reactive<godot::Transform2D> &value);
ModelField model_field(const char *name, godot::Plane &value);
ModelField model_field(const char *name, Reactive<godot::Plane> &value);
ModelField model_field(const char *name, godot::Quaternion &value);
ModelField model_field(const char *name, Reactive<godot::Quaternion> &value);
ModelField model_field(const char *name, godot::AABB &value);
ModelField model_field(const char *name, Reactive<godot::AABB> &value);
ModelField model_field(const char *name, godot::Basis &value);
ModelField model_field(const char *name, Reactive<godot::Basis> &value);
ModelField model_field(const char *name, godot::Transform3D &value);
ModelField model_field(const char *name, Reactive<godot::Transform3D> &value);
ModelField model_field(const char *name, godot::Projection &value);
ModelField model_field(const char *name, Reactive<godot::Projection> &value);
ModelField model_field(const char *name, godot::Color &value);
ModelField model_field(const char *name, Reactive<godot::Color> &value);
ModelField model_field(const char *name, godot::NodePath &value);
ModelField model_field(const char *name, Reactive<godot::NodePath> &value);
ModelField model_field(const char *name, godot::Array &value);
ModelField model_field(const char *name, Reactive<godot::Array> &value);
ModelField model_field(const char *name, godot::Dictionary &value);
ModelField model_field(const char *name, Reactive<godot::Dictionary> &value);
ModelField model_field(const char *name, godot::PackedByteArray &value);
ModelField model_field(const char *name, Reactive<godot::PackedByteArray> &value);
ModelField model_field(const char *name, godot::PackedInt32Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedInt32Array> &value);
ModelField model_field(const char *name, godot::PackedInt64Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedInt64Array> &value);
ModelField model_field(const char *name, godot::PackedFloat32Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedFloat32Array> &value);
ModelField model_field(const char *name, godot::PackedFloat64Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedFloat64Array> &value);
ModelField model_field(const char *name, godot::PackedStringArray &value);
ModelField model_field(const char *name, Reactive<godot::PackedStringArray> &value);
ModelField model_field(const char *name, godot::PackedVector2Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedVector2Array> &value);
ModelField model_field(const char *name, godot::PackedVector3Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedVector3Array> &value);
ModelField model_field(const char *name, godot::PackedVector4Array &value);
ModelField model_field(const char *name, Reactive<godot::PackedVector4Array> &value);
ModelField model_field(const char *name, godot::PackedColorArray &value);
ModelField model_field(const char *name, Reactive<godot::PackedColorArray> &value);
ModelField model_field(const char *name, godot::Variant &value);
ModelField model_field(const char *name, Reactive<godot::Variant> &value);

namespace detail {
SaveResult export_model(std::span<const ModelField> fields);
SaveStatus import_model(std::span<const ModelField> fields, const godot::Dictionary &data);
}
} // namespace meowody
