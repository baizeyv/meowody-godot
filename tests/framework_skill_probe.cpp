#include "skill_examples.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <chrono>

using namespace godot;
using namespace meowody;
using namespace meowody_skill_examples;

class FrameworkSkillProbe : public RefCounted {
    GDCLASS(FrameworkSkillProbe, RefCounted)
    bool okay = true;
    void check(bool value, const char *message) {
        UtilityFunctions::print("[FrameworkSkill] ", value ? "PASS " : "FAIL ", message);
        if (!value) okay = false;
    }
protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("run"), &FrameworkSkillProbe::run); }
public:
    bool run() {
        SaveCenter saves("user://skill-migration");
        Dictionary old; old["score"] = int64_t(7);
        check(saves.save("progress", "main", old).ok() && saves.flush_pending().ok(), "legacy fixture persisted");
        ProgressModel model;
        auto loaded = saves.load_model("progress", "main", model);
        check(loaded.ok() && loaded.migrated && model.coins.get() == 7, "v0 migration updates Model");
        check(!saves.load("progress", "main").data.has("coins"), "load does not rewrite the original snapshot");
        check(restore_and_commit_migration(saves, model).ok(), "explicit migration commit");
        auto committed = saves.load("progress", "main");
        check(committed.data.has("coins") && int64_t(committed.data[MODEL_VERSION_KEY]) == 1, "committed version and fields");
        old["score"] = "invalid";
        saves.save("progress", "main", old); saves.flush_pending();
        auto invalid = restore_and_commit_migration(saves, model);
        check(!invalid.ok() && model.coins.get() == 7 && saves.load("progress", "main").data["score"] == Variant("invalid"), "migration failure preserves Model and file");
        Dictionary newer; newer[MODEL_VERSION_KEY] = int64_t(2); newer["coins"] = int64_t(99);
        saves.save("progress", "main", newer); saves.flush_pending();
        check(restore_and_commit_migration(saves, model).error == SaveError::UnsupportedVersion && model.coins.get() == 7,
              "newer version is rejected without resetting state");

        SaveCenter business("user://skill-business");
        SystemCenter systems;
        auto progress = systems.register_system<ProgressSystem>(business);
        check(progress && systems.initialize().ok(), "business system initializes with missing save");
        auto *label = memnew(Label);
        {
            ScorePresenter presenter(progress);
            presenter.on_attach(*label);
            progress->model.coins.set(5);
            check(label->get_text() == "5", "Presenter refreshes from Model");
            presenter.disconnect();
            progress->model.coins.set(6);
            check(label->get_text() == "5", "closing Presenter disconnects its subscription");
        }
        {
            ScorePresenter reopened(progress);
            reopened.on_attach(*label);
            check(label->get_text() == "6", "new Presenter refreshes cached View");
            memdelete(label);
            progress->model.coins.set(8);
            check(true, "freed View is guarded by ObjectID");
        }
        systems.shutdown(); systems.clear(); progress.reset();

        uint64_t history_id;
        { DraftHistory history; history_id = history.get().get_instance_id(); }
        check(ObjectDB::get_instance(history_id) == nullptr, "native editor helper releases owned UndoRedo");

        MainThreadCenter main;
        TaskCenter tasks(main, 1);
        auto state = std::make_shared<SumState>();
        SumJob job;
        job.start(tasks, state, {1, 2, 3});
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (state->total != 6 && std::chrono::steady_clock::now() < deadline) {
            main.update(128, 0); std::this_thread::yield();
        }
        check(state->total == 6, "worker result delivered to main-thread state");
        job.start(tasks, state, {40, 50});
        job.cancel();
        tasks.shutdown(); main.update(128, 0);
        check(state->total == 6, "cancelled task does not update its receiver");
        main.shutdown();

        MainThreadCenter abandoned_main;
        TaskCenter abandoned_tasks(abandoned_main, 1);
        auto abandoned = std::make_shared<SumState>();
        SumJob detached;
        detached.start(abandoned_tasks, abandoned, {12});
        std::weak_ptr<SumState> weak = abandoned; abandoned.reset();
        deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!abandoned_main.pending_count() && std::chrono::steady_clock::now() < deadline) std::this_thread::yield();
        check(weak.expired() && abandoned_main.pending_count() > 0, "completed task does not retain its destroyed receiver");
        abandoned_main.update(128, 0);
        abandoned_tasks.shutdown(); abandoned_main.shutdown();
        return okay;
    }
};

static void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<FrameworkSkillProbe>();
}
extern "C" GDExtensionBool GDE_EXPORT framework_skill_test_init(GDExtensionInterfaceGetProcAddress proc,
        GDExtensionClassLibraryPtr library, GDExtensionInitialization *init) {
    GDExtensionBinding::InitObject object(proc, library, init);
    object.register_initializer(initialize);
    object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return object.init();
}
