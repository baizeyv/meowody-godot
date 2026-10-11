#include "runtime/save/model_fields.h"
#include "runtime/save/save_center.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <cstdlib>

using namespace godot;
using namespace meowody;
struct RankingModel : meowody::Model {
    Reactive<int> season{1};
    Reactive<Array> rows{Array{}};
    std::vector<meowody::ModelField> save_fields() override {
        return {model_field("season", season), model_field("rows", rows)};
    }
};
struct InvalidModel : meowody::Model {
    int first = 1;
    int second = 2;
    std::vector<meowody::ModelField> save_fields() override {
        return {model_field("same", first), model_field("same", second)};
    }
};
struct UnsignedModel : meowody::Model {
    uint64_t value = std::numeric_limits<uint64_t>::max();
    std::vector<meowody::ModelField> save_fields() override { return {model_field("value", value)}; }
};
struct MigratingRankingModel : RankingModel {
    mutable int steps = 0;
    bool fail_second = false;
    bool invalid_result = false;
    bool invalid_metadata = false;
    bool unsupported_data = false;
    int64_t save_version() const override { return 2; }
    SaveStatus migrate_save(int64_t from, Dictionary &data) const override {
        ++steps;
        if (from == 0) {
            data["season"] = data.get("old_season", 1);
            data.erase("old_season");
            return {};
        }
        if (from == 1) {
            Array entries = data.get("rows", Array{});
            for (int64_t i = 0; i < entries.size(); ++i) {
                Dictionary row = entries[i];
                row["score"] = row.get("points", 0);
                row.erase("points");
            }
            if (invalid_result) data["rows"] = "invalid";
            if (invalid_metadata) data[MODEL_VERSION_KEY] = 999;
            if (unsupported_data) data["unsupported"] = godot::Callable{};
            if (fail_second) return {SaveError::Corrupt, "bad ranking row"};
            return {};
        }
        return {SaveError::MigrationFailed, "missing step"};
    }
};
struct MissingMigrationModel : RankingModel {
    int64_t save_version() const override { return 1; }
};
struct InvalidVersionModel : RankingModel {
    int64_t save_version() const override { return -1; }
};
struct ReservedFieldModel : RankingModel {
    std::vector<ModelField> save_fields() override { return {model_field(MODEL_VERSION_KEY, season)}; }
};
static void check(bool condition) { if (!condition) std::abort(); }
static void migration_tests(SaveCenter &saves) {
    Dictionary legacy, row;
    row["player"] = "Alice"; row["points"] = 900;
    Array rows; rows.append(row);
    legacy["old_season"] = 5; legacy["rows"] = rows;
    check(saves.save("migration", "legacy", legacy).ok());
    check(saves.flush_pending().ok());
    MigratingRankingModel model;
    int notifications = 0;
    auto connection = model.season.subscribe([&](const int &) { ++notifications; });
    auto result = saves.load_model("migration", "legacy", model);
    check(result.ok() && result.migrated && result.saved_version == 0 && result.current_version == 2);
    check(model.steps == 2 && model.season.get() == 5 && notifications == 1);
    check(int64_t(Dictionary(model.rows.get()[0])["score"]) == 900);
    // Loading is read-only: neither pending state nor original disk data is upgraded.
    check(!saves.is_pending("migration"));
    check(!saves.load("migration", "legacy").data.has(MODEL_VERSION_KEY));
    SaveCenter disk(saves.directory());
    check(Dictionary(Array(disk.load("migration", "legacy").data["rows"])[0]).has("points"));
    check(saves.save_model("migration", "current", model).ok());
    check(saves.flush_pending().ok());
    model.steps = 0;
    result = saves.load_model("migration", "current", model);
    check(result.ok() && !result.migrated && model.steps == 0 && result.saved_version == 2);
    check(int64_t(saves.load("migration", "current").data[MODEL_VERSION_KEY]) == 2);
    MigratingRankingModel failing;
    failing.fail_second = true;
    result = saves.load_model("migration", "legacy", failing);
    check(result.error == SaveError::MigrationFailed && !result.migrated && failing.season.get() == 1);
    check(failing.rows.get().is_empty() && !saves.is_pending("migration"));
    check(Dictionary(Array(saves.load("migration", "legacy").data["rows"])[0]).has("points"));
    failing.fail_second = false; failing.invalid_result = true;
    result = saves.load_model("migration", "legacy", failing);
    check(result.error == SaveError::Corrupt && failing.season.get() == 1);
    failing.invalid_result = false; failing.invalid_metadata = true;
    check(saves.load_model("migration", "legacy", failing).error == SaveError::MigrationFailed);
    failing.invalid_metadata = false; failing.unsupported_data = true;
    check(saves.load_model("migration", "legacy", failing).error == SaveError::UnsupportedData);
    check(failing.season.get() == 1 && failing.rows.get().is_empty());
    InvalidVersionModel invalid_model;
    check(saves.save_model("migration", "invalid_model", invalid_model).error == SaveError::InvalidArgument);
    check(saves.load_model("migration", "legacy", invalid_model).error == SaveError::InvalidArgument);
    ReservedFieldModel reserved;
    check(saves.save_model("migration", "reserved", reserved).error == SaveError::InvalidArgument);
    check(saves.load_model("migration", "legacy", reserved).error == SaveError::InvalidArgument);
    MissingMigrationModel missing;
    check(saves.load_model("migration", "legacy", missing).error == SaveError::MigrationFailed);
    RankingModel outdated;
    check(saves.load_model("migration", "current", outdated).error == SaveError::UnsupportedVersion);
    check(outdated.season.get() == 1);
    Dictionary invalid = legacy.duplicate(true); invalid[MODEL_VERSION_KEY] = "2";
    check(saves.save("migration", "bad_version", invalid).ok());
    check(saves.load_model("migration", "bad_version", model).error == SaveError::Corrupt);
    invalid[MODEL_VERSION_KEY] = -1;
    check(saves.save("migration", "negative_version", invalid).ok());
    check(saves.load_model("migration", "negative_version", model).error == SaveError::Corrupt);
    Dictionary v1 = legacy.duplicate(true);
    v1["season"] = 6; v1.erase("old_season"); v1[MODEL_VERSION_KEY] = 1;
    check(saves.save("migration", "v1", v1).ok());
    model.steps = 0;
    result = saves.load_model("migration", "v1", model);
    check(result.ok() && result.migrated && result.saved_version == 1 && model.steps == 1);
}
static void run(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    SaveCenter saves("user://model_test");
    RankingModel model;
    InvalidModel invalid_model;
    UnsignedModel unsigned_model;
    check(saves.save_model("ranking", "invalid", invalid_model).error == SaveError::InvalidArgument);
    check(saves.save_model("ranking", "unsigned", unsigned_model).error == SaveError::UnsupportedData);
    int notifications = 0;
    auto connection = model.season.subscribe([&](const int &) { ++notifications; });
    model.season.set(3);
    Dictionary row; row["player"] = "Alice"; row["score"] = 900;
    Array rows; rows.append(row); model.rows.set(rows);
    check(saves.save_model("ranking", "main", model).ok());
    check(saves.flush_pending().ok());
    model.season.set(4); model.rows.set(Array{});
    const int before = notifications;
    check(saves.load_model("ranking", "main", model).ok());
    check(model.season.get() == 3 && model.rows.get().size() == 1 && notifications == before + 1);
    check(int64_t(Dictionary(model.rows.get()[0])["score"]) == 900);
    Dictionary invalid; invalid["season"] = 7; invalid["rows"] = "wrong";
    check(saves.save("ranking", "bad", invalid).ok());
    check(saves.load_model("ranking", "bad", model).error == SaveError::Corrupt);
    check(model.season.get() == 3 && notifications == before + 1);
    Dictionary overflow; overflow["season"] = int64_t(1) << 40;
    check(saves.save("ranking", "overflow", overflow).ok());
    check(saves.load_model("ranking", "overflow", model).error == SaveError::Corrupt);
    Dictionary old; old["season"] = 8; old["retired"] = true;
    check(saves.save("ranking", "old", old).ok());
    check(saves.load_model("ranking", "old", model).ok());
    check(model.season.get() == 8 && model.rows.get().size() == 1);
    check(saves.load_model("ranking", "missing", model).error == SaveError::Missing);
    check(model.season.get() == 8);
    migration_tests(saves);
    UtilityFunctions::print("[ModelSaveTest] PASS");
}
extern "C" {
GDExtensionBool GDE_EXPORT model_save_test_init(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization *init) {
    GDExtensionBinding::InitObject object(proc, library, init);
    object.register_initializer(run);
    object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return object.init();
}
}
