#include "runtime/save/model_fields.h"

struct VersionedLeaderboardModel : meowody::Model {
    meowody::Reactive<int> season{1};
    meowody::Reactive<godot::Array> entries{godot::Array{}};

    std::vector<meowody::ModelField> save_fields() override {
        return {
            meowody::model_field("season", season),
            meowody::model_field("entries", entries)};
    }
    int64_t save_version() const override { return 2; }

    meowody::SaveStatus migrate_save(int64_t from, godot::Dictionary &data) const override {
        if (from == 0) {
            const godot::Variant rows = data.get("rows", godot::Array{});
            if (rows.get_type() != godot::Variant::ARRAY)
                return {meowody::SaveError::Corrupt, "Legacy rows must be an Array"};
            data["season"] = data.get("old_season", 1);
            data["entries"] = rows;
            data.erase("old_season");
            data.erase("rows");
            return {};
        }
        if (from == 1) {
            const godot::Variant stored = data.get("entries", godot::Array{});
            if (stored.get_type() != godot::Variant::ARRAY)
                return {meowody::SaveError::Corrupt, "Entries must be an Array"};
            godot::Array rows = stored;
            for (int64_t i = 0; i < rows.size(); ++i) {
                const godot::Variant value = rows[i];
                if (value.get_type() != godot::Variant::DICTIONARY)
                    return {meowody::SaveError::Corrupt, "Ranking row must be a Dictionary"};
                godot::Dictionary row = value;
                const godot::Variant points = row.get("points", godot::Variant());
                if (points.get_type() != godot::Variant::INT)
                    return {meowody::SaveError::Corrupt, "Legacy points must be an integer"};
                row["score"] = points;
                row.erase("points");
            }
            data["entries"] = rows;
            return {};
        }
        return {meowody::SaveError::MigrationFailed, "Missing leaderboard migration step"};
    }
};

#include "runtime/app.h"
void example() {
VersionedLeaderboardModel model;
auto result = meowody::App::save->load_model("leaderboard", "default", model);
if (!result.ok()) {
    // result.error/message：交给业务显示或记录错误。
    // Missing 时可以使用默认值；其它错误不能直接保存默认值覆盖旧数据。
    return;
}
if (result.migrated) {
    // 只有读取、迁移、校验和导入全部成功，才显式保存升级后的版本。
    auto saved = meowody::App::save->save_model("leaderboard", "default", model);
    if (!saved.ok()) return;
    auto committed = meowody::App::save->flush_pending();
    if (!committed.ok()) return; // 按业务提示重试；暂存数据按原 SaveCenter 规则保留。
}

}
