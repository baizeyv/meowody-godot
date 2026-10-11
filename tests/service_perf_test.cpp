#include "runtime/resource/resource_center.h"
#include "runtime/net/net_center.h"
#include "runtime/time/time_center.h"
#include "runtime/audio/audio_center.h"
#include "runtime/core/request_history.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/audio_stream_wav.hpp>
#include <godot_cpp/classes/audio_stream_player.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <new>
#include <thread>

namespace {
struct HistoryState { bool done = false; int value = 0; };
struct HistoryProgress {
    bool found = false, done = false;
    int value = 0;
    bool finished() const { return done; }
    static HistoryProgress snapshot(const HistoryState &state) { return {true, state.done, state.value}; }
};
std::atomic<uint64_t> allocations{0};
thread_local bool measure_allocations = false;
}
void *operator new(std::size_t size) {
    if (measure_allocations) ++allocations;
    if (auto *memory = std::malloc(size ? size : 1)) return memory;
    throw std::bad_alloc();
}
void operator delete(void *memory) noexcept { std::free(memory); }
void operator delete(void *memory, std::size_t) noexcept { std::free(memory); }

using namespace godot;
using namespace meowody;
namespace {
class FakeResources final : public detail::ResourceBackend {
public:
    bool finished = false;
    int polls = 0, gets = 0;
    String resolve_uid(const String &path) override { return path; }
    Ref<Resource> load(const String &) override { return {}; }
    Error request(const String &) override { return OK; }
    ResourceLoader::ThreadLoadStatus poll(const String &, float &progress) override {
        ++polls; progress = finished ? 1 : .5f;
        return finished ? ResourceLoader::THREAD_LOAD_LOADED : ResourceLoader::THREAD_LOAD_IN_PROGRESS;
    }
    Ref<Resource> get(const String &) override { ++gets; Ref<Resource> resource; resource.instantiate(); return resource; }
};
class FakeClock final : public detail::TimeClock {
public:
    mutable int conversions = 0;
    double seconds = 1704067200.25; // 2024-01-01 UTC.
    double system_unix() const override { return seconds; }
    double monotonic() const override { return seconds; }
    DateTime calendar(double seconds, TimeZone zone) const override {
        ++conversions;
        return detail::calendar_from_unix(seconds, zone);
    }
};
template <class Callback>
void benchmark(const char *name, Callback callback) {
    constexpr int frames = 500;
    callback(); // Warm reusable containers.
    std::vector<double> samples;
    uint64_t count = 0;
    for (int sample = 0; sample < 7; ++sample) {
        allocations = 0;
        measure_allocations = true;
        const auto start = std::chrono::steady_clock::now();
        for (int frame = 0; frame < frames; ++frame) callback();
        const auto elapsed = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count();
        measure_allocations = false;
        count = allocations.load();
        samples.push_back(elapsed / frames);
    }
    std::sort(samples.begin(), samples.end());
    UtilityFunctions::print("[ServicePerf] ", name, " median_us=", samples[samples.size() / 2], " cpp_allocations_per_500_frames=", count);
}
}
class MeowodyServicePerfProbe : public RefCounted {
    GDCLASS(MeowodyServicePerfProbe, RefCounted)
protected:
    static void _bind_methods() { ClassDB::bind_method(D_METHOD("run", "download_url", "expected_hash"), &MeowodyServicePerfProbe::run); }
public:
    bool run(const String &download_url, const String &expected_hash) {
        bool okay = true;
        auto check = [&](bool passed, const char *message) {
            if (!passed) { okay = false; UtilityFunctions::printerr("[ServicePerf] FAIL: ", message); }
        };
        {
            detail::RequestHistory<HistoryState, HistoryProgress> history(HistoryProgress::snapshot);
            auto active = std::make_shared<HistoryState>();
            history.track("active", active);
            std::vector<std::shared_ptr<HistoryState>> completed;
            for (int i = 0; i < 129; ++i) {
                auto state = std::make_shared<HistoryState>(); state->done = true; state->value = i;
                history.track(String::num_int64(i), state); completed.push_back(state);
            }
            check(history.query("active").found && !history.query("0").found && history.query("128").found, "history retains active and newest 128 completed");
            completed.back()->value = 256; history.refresh(); completed.back().reset();
            check(history.query("128").value == 256, "history weak retention updates held terminal values");
            history.track("1", active);
            history.remember("new", {true, true, 1});
            check(history.query("1").found && history.query("2").found, "retracking terminal as active updates capacity count");
            history.remember("newer", {true, true, 2});
            check(history.query("1").found && !history.query("2").found, "retracked active entry cannot be pruned");
            active->done = true; history.refresh();
            check(history.query("1").found && !history.query("active").found, "refresh prunes oldest newly completed key");
            history.clear(); check(!history.query("1").found, "history clear");
            std::vector<double> samples;
            for (int sample = 0; sample < 7; ++sample) {
                detail::RequestHistory<HistoryState, HistoryProgress> batch(HistoryProgress::snapshot);
                std::vector<std::shared_ptr<HistoryState>> states;
                for (int i = 0; i < 2048; ++i) {
                    auto state = std::make_shared<HistoryState>();
                    batch.track(String::num_int64(i), state); states.push_back(state);
                }
                for (auto &state : states) state->done = true;
                const auto start = std::chrono::steady_clock::now(); batch.refresh();
                samples.push_back(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - start).count());
            }
            std::sort(samples.begin(), samples.end());
            UtilityFunctions::print("[ServicePerf] history_2048_complete median_us=", samples[samples.size() / 2]);
        }
        {
            NetCenter net([] { return true; });
            int notifications = 0;
            for (int i = 0; i < 128; ++i) net.subscribe([&](const NetSnapshot &) { ++notifications; });
            net.update(); check(notifications == 128, "initial network status delivered");
            benchmark("net_128_unchanged_listeners", [&] { net.update(); });
            check(notifications == 128, "unchanged network status silent");
            uint64_t removed = 0;
            int added = 0;
            net.subscribe([&](const NetSnapshot &) {
                net.unsubscribe(removed);
                net.subscribe([&](const NetSnapshot &) { ++added; });
                net.update(); // Reentrancy remains guarded.
            });
            removed = net.subscribe([&](const NetSnapshot &) { check(false, "removed listener delivered"); });
            net.configure_monitor("", 5, 2); net.update();
            check(added == 0, "new listener deferred to next status");
            int delivered = 0;
            net.get("bad url", [&](const NetResult &) {
                ++delivered;
                net.get("bad url", [&](const NetResult &) { ++delivered; });
            });
            net.update(); check(delivered == 1, "callback request deferred");
            net.update(); check(delivered == 2, "deferred request delivered");
        }
        {
            auto fake = std::make_unique<FakeResources>();
            auto *backend = fake.get();
            ResourceCenter resources(std::move(fake));
            for (int i = 0; i < 128; ++i) resources.load_async<>("res://pending_" + String::num_int64(i) + ".tres");
            benchmark("resource_128_pending", [&] { resources.update(); });
            backend->finished = true;
            int delivered = 0;
            resources.load_async<>("res://first.tres", [&](const LoadResult<> &) {
                ++delivered;
                resources.load_async<>("res://next.tres", [&](const LoadResult<> &) { ++delivered; });
                resources.update();
            });
            resources.update(); check(delivered == 1, "resource callback enqueue deferred");
            resources.update(); check(delivered == 2, "resource deferred completion");
            check(backend->gets == 130 && resources.is_idle(), "all resources collected exactly once");
        }
        {
            auto fake = std::make_unique<FakeClock>(); auto *clock = fake.get();
            TimeCenter time(std::move(fake));
            std::vector<DailySubscription> handles;
            for (int i = 0; i < 128; ++i) handles.push_back(time.daily(12, 0, [] {}, TimeZone::Local));
            clock->conversions = 0;
            benchmark("time_128_same_zone", [&] { time.update(); });
            UtilityFunctions::print("[ServicePerf] time_calendar_calls_per_3501_updates=", clock->conversions);
            time.clear_subscriptions(); handles.clear();
            int delivered = 0;
            DailySubscription second;
            handles.push_back(time.daily(0, 1, [&] {
                ++delivered; second.cancel();
                handles.push_back(time.daily(0, 1, [&] { ++delivered; }, TimeZone::UTC));
                time.update();
            }, TimeZone::UTC));
            second = time.daily(0, 1, [&] { check(false, "cancelled daily delivered"); }, TimeZone::UTC);
            clock->seconds += 60; time.update();
            check(delivered == 1, "daily mutation cancellation and deferred addition");
            clock->seconds += 86400; time.update();
            check(delivered == 3, "new daily subscription fires next day");
        }
        {
            AudioCenter audio;
            AudioChannelOptions options; options.max_voices = 128;
            check(audio.create_channel("Benchmark", options), "audio channel");
            Ref<AudioStreamWAV> stream; stream.instantiate();
            stream->set_format(AudioStreamWAV::FORMAT_8_BITS); stream->set_mix_rate(8000);
            PackedByteArray data; data.resize(8000); stream->set_data(data);
            stream->set_loop_mode(AudioStreamWAV::LOOP_FORWARD); stream->set_loop_end(8000);
            std::vector<AudioHandle> voices;
            for (int i = 0; i < 128; ++i) voices.push_back(audio.play("Benchmark", stream));
            check(voices.front().id() != 0, "audio voice created");
            benchmark("audio_128_steady_voices", [&] { audio.update(0); });
            auto *player = Object::cast_to<AudioStreamPlayer>(ObjectDB::get_instance(voices.front().id()));
            if (player) {
                player->set_volume_linear(.1f); player->set_stream_paused(true);
                audio.update(0);
                check(Math::is_equal_approx(player->get_volume_linear(), 1.f) && !player->get_stream_paused(), "external audio properties restored");
                voices.front().pause(); audio.set_paused("Benchmark", true); voices.front().resume();
                audio.update(0); check(player->get_stream_paused(), "channel pause masks voice resume");
                audio.set_paused("Benchmark", false); audio.update(0);
                check(!player->get_stream_paused(), "voice resumes after channel");
                audio.fade_volume("Benchmark", .25, .5); audio.update(.5);
                check(Math::is_equal_approx(player->get_volume_linear(), .25f), "final fade volume applied");
            }
        }
        {
            DownloadCenter downloads("user://service_downloads");
            DownloadOptions options;
            options.use_cache = false; options.max_retries = 1; options.retry_delay_seconds = .001;
            options.timeout_seconds = 3; options.max_bytes = 4 * 1024 * 1024;
            options.expected_sha256 = expected_hash;
            auto wait = [&](const DownloadHandle &handle, bool test_pause) {
                const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
                bool paused = false;
                while (!handle.finished() && std::chrono::steady_clock::now() < deadline) {
                    downloads.update();
                    if (test_pause && !paused && handle.downloaded_bytes() > 0 && !handle.finished()) {
                        handle.pause(); downloads.update();
                        const auto received = handle.downloaded_bytes(); downloads.update();
                        check(handle.paused() && handle.downloaded_bytes() == received, "download pause preserves partial bytes");
                        handle.resume(); paused = true;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
                downloads.update();
                check(handle.finished(), "download bounded completion");
            };
            int delivered = 0;
            auto good = downloads.download(download_url + String("/good"), options, [&](const DownloadResult &) { ++delivered; });
            wait(good, true);
            if (!good.succeeded()) UtilityFunctions::printerr("[ServicePerf] good download: ", good.result().message);
            check(good.succeeded() && delivered == 1 && FileAccess::get_sha256(good.result().path) == expected_hash, "multichunk paused/resumed download integrity");
            auto retried = downloads.download(download_url + String("/retry"), options, [&](const DownloadResult &) { ++delivered; });
            wait(retried, false);
            if (!retried.succeeded()) UtilityFunctions::printerr("[ServicePerf] retry download: ", retried.result().message);
            check(retried.succeeded() && retried.retries() == 1 && delivered == 2 && FileAccess::get_sha256(retried.result().path) == expected_hash, "truncated transfer retry/range resume integrity");
            options.max_retries = 0;
            auto truncated = downloads.download(download_url + String("/truncated"), options, [&](const DownloadResult &) { ++delivered; });
            wait(truncated, false);
            check(truncated.failed() && delivered == 3 && downloads.is_idle(), "truncated response fails without committed file");
        }
        return okay;
    }
};
static void initialize(ModuleInitializationLevel level) {
    if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<MeowodyServicePerfProbe>();
}
extern "C" {
GDExtensionBool GDE_EXPORT service_perf_init(GDExtensionInterfaceGetProcAddress proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization *init) {
    GDExtensionBinding::InitObject object(proc, library, init);
    object.register_initializer(initialize); object.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE); return object.init();
}
}
