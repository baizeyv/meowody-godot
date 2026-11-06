#include "runtime/resource/resource_center.h"
#include <godot_cpp/godot.hpp>
#include <godot_cpp/classes/ref_counted.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/sprite2d.hpp>
#include <godot_cpp/classes/audio_stream.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
using namespace godot;
using namespace meowody;

class MeowodyResourceProbe : public RefCounted {
    GDCLASS(MeowodyResourceProbe, RefCounted)
    DownloadCenter downloads;
    std::unique_ptr<ResourceCenter> resources;
    ResourceOperation initialization;
    LoadHandle<> first, second;
    LoadHandle<PackedScene> wrong;
    LoadHandle<PackedScene> scene;
    LoadHandle<AudioStream> audio;
    BatchLoadHandle group;
    int callbacks = 0;
protected:
    static void _bind_methods() {
        ClassDB::bind_method(D_METHOD("begin", "source"), &MeowodyResourceProbe::begin);
        ClassDB::bind_method(D_METHOD("tick"), &MeowodyResourceProbe::tick);
        ClassDB::bind_method(D_METHOD("initialized"), &MeowodyResourceProbe::initialized);
        ClassDB::bind_method(D_METHOD("ready"), &MeowodyResourceProbe::ready);
        ClassDB::bind_method(D_METHOD("message"), &MeowodyResourceProbe::message);
        ClassDB::bind_method(D_METHOD("fallback"), &MeowodyResourceProbe::fallback);
        ClassDB::bind_method(D_METHOD("cancel_initialization"), &MeowodyResourceProbe::cancel_initialization);
        ClassDB::bind_method(D_METHOD("start_load"), &MeowodyResourceProbe::start_load);
        ClassDB::bind_method(D_METHOD("loaded"), &MeowodyResourceProbe::loaded);
        ClassDB::bind_method(D_METHOD("verify", "expected"), &MeowodyResourceProbe::verify);
    }
public:
    void begin(bool source) {
        resources = source ? std::make_unique<ResourceCenter>() : std::make_unique<ResourceCenter>(downloads);
        ResourceOptions options; options.mode = source ? ResourceMode::Source : ResourceMode::Packed;
        options.catalog_path = "res://test_catalog.json";
        options.cache_directory = "user://test_resources";
        initialization = resources->initialize(options);
    }
    void tick() {
        downloads.update(); resources->update();
    }
    bool initialized() const { return initialization.finished(); }
    bool ready() const { return initialization.succeeded(); }
    String message() const { return initialization.message(); }
    bool fallback() const { return initialization.used_fallback(); }
    void cancel_initialization() { initialization.cancel(); }
    void start_load() {
        first = resources->load_async<>("item", [&](const LoadResult<>&) { callbacks += 100; });
        first.cancel();
        second = resources->load_async<>("item", [&](const LoadResult<>& result) { if (result) ++callbacks; });
        wrong = resources->load_async<PackedScene>("item");
        scene = resources->load_async<PackedScene>("scene");
        audio = resources->load_async<AudioStream>("sound");
        group = resources->preload_label("startup");
    }
    bool loaded() const { return second.finished() && wrong.finished() && group.finished() && scene.finished() && audio.finished(); }
    bool verify(const String& expected) {
        auto result = second.result();
        bool passed = result && result.resource->get_name() == expected && callbacks == 1 && first.cancelled() && wrong.result().error == LoadError::TypeMismatch && group.succeeded();
        passed &= scene.succeeded() && audio.succeeded();
        if (scene.succeeded()) {
            auto* root = scene.result().resource->instantiate();
            auto* sprite = root ? Object::cast_to<Sprite2D>(root) : nullptr;
            passed &= sprite && sprite->get_texture().is_valid() && sprite->get_texture()->get_width() == 4;
            if (root) memdelete(root);
        }
        passed &= resources->load<>("missing").error == LoadError::UnknownAddress;
        passed &= resources->load<>("res://item.tres").error == LoadError::UnknownAddress;
        passed &= resources->has_cached_dependencies("item");
        auto copy = second;
        second.release();
        passed &= copy.result().error == LoadError::Released;
        result.resource.unref();
        group = {}; resources->release_group("label:startup");
        passed &= resources->get_cached<>("item").is_null();
        if (!passed) UtilityFunctions::printerr("[ResourcesTest] verification failed: ", second.result().message, " callbacks=", callbacks);
        return passed;
    }
};
static void initialize_probe(ModuleInitializationLevel level) { if (level == MODULE_INITIALIZATION_LEVEL_SCENE) ClassDB::register_class<MeowodyResourceProbe>(); }
extern "C" GDExtensionBool GDE_EXPORT resources_test_init(GDExtensionInterfaceGetProcAddress get_proc, GDExtensionClassLibraryPtr library, GDExtensionInitialization* initialization) {
    GDExtensionBinding::InitObject init(get_proc, library, initialization);
    init.register_initializer(initialize_probe);
    init.set_minimum_library_initialization_level(MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}
