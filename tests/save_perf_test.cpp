#include "runtime/save/save_center.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/classes/json.hpp>
#include <chrono>
#include <algorithm>
#include <vector>
#include <cstdlib>

using namespace godot;
using namespace meowody;
static void check(bool value) { if (!value) std::abort(); }
static double measure(const std::function<void()> &operation, int iterations) {
    std::vector<double> samples;
    for (int sample = 0; sample < 5; ++sample) {
        auto start = std::chrono::steady_clock::now();
        for (int i = 0; i < iterations; ++i) operation();
        samples.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }
    std::sort(samples.begin(), samples.end()); return samples[2];
}
static void run(ModuleInitializationLevel level) {
    if (level != MODULE_INITIALIZATION_LEVEL_SCENE) return;
    SaveCenter saves("user://save_perf");
    Array rows;
    for (int i = 0; i < 2000; ++i) {
        Dictionary row; row["player"] = String("Player_") + String::num_int64(i); row["score"] = i;
        Array nested; nested.append(i); nested.append(i + 1); row["stats"] = nested; rows.append(row);
    }
    Dictionary large; large["rows"] = rows;
    Dictionary small; small["score"] = 7;
    check(saves.save("ranking", "large", large).ok());
    check(saves.save("ranking", "small", small).ok());
    Dictionary metrics;
    metrics["load_small_100_ms"] = measure([&] { check(int64_t(saves.load("ranking", "small").data["score"]) == 7); }, 100);
    metrics["save_small_20_ms"] = measure([&] { check(saves.save("ranking", "small", small).ok()); }, 20);
    Dictionary caller = small; caller["score"] = 9;
    check(int64_t(saves.load("ranking", "small").data["score"]) == 7);
    Dictionary source_row = rows[0]; Array source_stats = source_row["stats"];
    source_stats[0] = -3;
    check(int64_t(Array(Dictionary(Array(saves.load("ranking", "large").data["rows"])[0])["stats"])[0]) == 0);
    Dictionary loaded = saves.load("ranking", "large").data;
    Array returned_rows = loaded["rows"]; Dictionary returned_row = returned_rows[0]; returned_row["score"] = -1;
    check(int64_t(Dictionary(Array(saves.load("ranking", "large").data["rows"])[0])["score"]) == 0);
    Dictionary inspected = saves.inspect("ranking").data;
    Dictionary inspected_small = inspected["small"]; inspected_small["score"] = -2;
    check(int64_t(saves.load("ranking", "small").data["score"]) == 7);
    Array inspected_rows = Dictionary(inspected["large"])["rows"];
    Array inspected_stats = Dictionary(inspected_rows[0])["stats"]; inspected_stats[0] = -4;
    check(int64_t(Array(Dictionary(Array(saves.load("ranking", "large").data["rows"])[0])["stats"])[0]) == 0);
    check(saves.erase_slot("ranking", "small").ok());
    check(saves.has_slot("ranking", "large") && !saves.has_slot("ranking", "small"));
    check(saves.flush_pending().ok());
    SaveCenter disk(saves.directory());
    check(int64_t(Dictionary(Array(disk.load("ranking", "large").data["rows"])[0])["score"]) == 0);
    SaveOptions limited; limited.max_file_bytes = 128;
    SaveCenter limits("user://limit_test", limited);
    check(limits.save("p", "good", Dictionary{}).ok());
    check(limits.save("p", "bad", large).error == SaveError::InvalidArgument);
    check(limits.has_slot("p", "good") && !limits.has_slot("p", "bad"));
    UtilityFunctions::print("[SavePerf] ", JSON::stringify(metrics));
    UtilityFunctions::print("[SavePerfTest] PASS");
}
extern "C" {
GDExtensionBool GDE_EXPORT save_perf_init(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization *init) {
    GDExtensionBinding::InitObject object(proc, library, init);
    object.register_initializer(run); object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE); return object.init();
}
}
