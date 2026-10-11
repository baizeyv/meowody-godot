---
name: meowody-framework
description: Develop, fix, review, or document the Meowody Godot 4.7 C++ GDExtension framework, its App services, native UI nodes, reflect codegen and GDScript editor plugins. Apply in a Meowody workspace; generic Godot projects use other conventions.
---

# Meowody framework development

Locate the requested repository using SConstruct, src/runtime/app.h and meow/project.godot. Root README.md is the single handbook. Read relevant chapters and actual headers before editing; a small task does not require loading the whole manual.

## Reference routing

Use README explicit anchors or rg searches:

| Task | Anchors | Source |
| --- | --- | --- |
| Initialization / builds | app, build-options | src/runtime/app.h, app.cpp, SConstruct |
| Reflect properties / methods / signals | reflection | src/reflection; reflect_codegen |
| Main thread / background tasks / input | main-thread, task, input | src/runtime/main_thread, task, input |
| Events / reactive / coroutine / callbacks | core, event, reactive, coroutine | matching src/runtime directories |
| Animation / IO / assets / persistence | tween, download, resource, save, pool | matching centers and types |
| Time / audio / HTTP / scenes / UI | time, audio, net, scene, ui | matching centers and types |
| MVP / business Systems | mvp, ui | src/runtime/system/system_center.h, src/runtime/ui/ui_presenter.h and ui_center; business examples in README |
| Generation / scene binding / reload | binding | addons/meowody_binding; src/editor/binding |
| Atlas / MultiPatch | atlas, multipatch | addons/meowody_atlas and meowody_multipatch; src/runtime/ui |
| Native controls | nodes, node-examples | src/runtime/ui/nodes; addons/meowody_nodes |
| Debugging / native crashes | debug | src/log; src/runtime/debug; addons/meowody_debug |
| Godot MCP / live editor | mcp | meow/addons/meowody_mcp, tools/godot_mcp, tests/run_mcp_editor_test.py |
| Performance | performance, build-options | tests/*perf*, reflect_codegen; measured runtime hot paths |
| Validation | verification, troubleshooting | tests drivers and generated fixtures |

The addons paths above are under meow/ on disk.

## Framework constraints

- Framework root differs from Godot root: res:// means meow/, and C++ include paths start at src/.
- Use meowody::App::resource->, App::save-> and other current Proxy entries. Proxy only exposes operator->; removed Runtime::resources() interfaces are invalid.
- Use GDREFLECT_CLASS and GDPROPERTY for ordinary runtime classes. Codegen generates missing accessor declarations, bodies and bindings. Business setters may provide validation, signals and resource notifications: preserve that behavior. Do not add explicit _bind_methods to reflected runtime classes.
- Put business calls to generated accessors in cpp implementations; codegen disables generated declarations while scanning headers, so inline header calls may fail before those methods exist.
- Do not edit build/generated. Binding users own h/cpp; the generator owns includes.gen.h and bindings.gen.inc. Each native node has its own h/cpp; shaders are separate gdshader runtime assets.
- Editor UI is GDScript with short-lived C++ backend calls. Do not hold native backend instances or native Callables across DLL reload. Native scene references are stored by Godot; NativeSceneRegistration and runtime rebinding are obsolete.
- Native controls must disconnect constructor/resource native Callable connections in their destructors before DLL unload. Use named engine Callables for engine-owned tween callbacks such as queue_free. Binding regression holds LongPress/Repeat/Snap/Loop through three DLL replacements, checks one connection and frees them before exit. An editor already using the old DLL without cleanup must restart once before testing the fix.
- MVP uses optional non-template UIPresenter factories in UICenter. Model/System are ordinary business types. SystemCenter uses an unordered_map for lookup and a separate reverse registration order. Persisted Models inherit non-template Model and override vector<ModelField> save_fields() using model_field(name, field reference). SaveCenter::save_model/load_model take Model&; shared type overloads and .cpp loops replace per-Model templates; Reactive values are restored with notifications after whole-input type validation. Register Systems once via App::system->register_systems<T...>() or register_system<T>(args...), then explicitly initialize() before opening business UI; get<T>() does not initialize. Optional System base provides initialize()->SystemStatus and shutdown() noexcept; plain classes still register without hooks. Startup failure cleans the failing instance then earlier started instances in reverse order and permits retry. shutdown closes once even with external owners; clear releases registrations and starts a new registration cycle. No registration during lifecycle callbacks or after startup; get works in callbacks. App shuts Systems down after UI and before underlying services; no automatic updates. Presenter tracks UI-owned subscriptions; shared System work can outlive a panel. Factory shared_ptr captures extend System lifetime; preserve explicit scene/game ownership.
- Model save_version defaults to 0; legacy snapshots without metadata are v0. migrate_save(from, Dictionary&) advances exactly one version on a deep copy; callbacks must only transform that copy. Newer versions are rejected; failures must not be followed by saving defaults. load_model returns ModelLoadResult and never rewrites storage; persist/flush explicitly after successful migrated loads. MODEL_VERSION_KEY is reserved. Dictionary/Resource APIs do not auto-migrate.
- MainThreadCenter::post and TaskCenter::submit/handles are thread-safe; their update/shutdown/destruction belong to the creating main thread. Workers only use ordinary C++ data; completion captures ObjectID/weak_ptr and checks lifetime. TaskValue is a fixed std::variant (no RTTI), cancellation is cooperative, shutdown joins before releasing Systems, and then MainThread callbacks are discarded. Exception conversion exists only when exceptions are enabled; release jobs return TaskError explicitly. Native/isolated regression: tests/async_centers_test.cpp and tests/run_centers_test.py.
- InputCenter routes polls through held move-only InputContext tokens. Priority/latest activation wins; exclusive contexts mask lower contexts, additive contexts claim matching actions. Context/focus/binding changes suppress edges for the current process frame. InputMap rebind/import validates first, snapshots plain values via SaveCenter, and does not intercept raw Godot/Control input. All input operations and token disposal stay on main thread.
- App drives services and deferred tween/coroutine/save phases. Avoid duplicate updates and access during static global construction. Follow each module's cancellation, callback and ownership semantics.
- Runtime atlas assets are native SpriteAtlas atlas.tres; UISpriteAtlas is editor configuration. Preserve stable source references and logical regions/margins during repacks. MultiPatch sidecars reference source textures; avoid competing image importers or unstable packed UVs.
- Reused list cells replace their complete display state and subscriptions. Loop recentering shifts animation coordinates too. Keep native drag/drop delivery and input cancellation on release, hiding, disabling and focus loss.
- Binding failures retain a closable progress dialog; full success closes automatically. Keep UI task state outside the unloadable DLL.

## Godot MCP

- Local stdio server tools/godot_mcp/dist/index.js is bound by --project to the Godot root (meow/). Build with npm ci --ignore-scripts and npm run build; local .codex/config.toml is machine-specific and ignored. Preserve other MCP servers. Client MCP settings need Restart to load newly configured tools; do not infer an active chat connection from a config file alone.
- meowody_mcp is a GDScript EditorPlugin, loopback WebSocket on an ephemeral port with a random per-editor token and project identity. Its private endpoint lives under .godot/meowody_mcp and is removed on orderly shutdown. Never print credentials or retain native DLL callbacks/backends. No runtime autoload.
- Begin with editor_get_status and editor_get_scene_tree. Use live editor tools for open scenes; preserve unsaved edits, current-scene guards, native UndoRedo and ownership. Properties use Godot literals, explicit load("res://...") and node("scene/path") wrappers. New native types still require framework generation/build. Reject edits during Meowody binding work, scene escapes and foreign projects; keep existing/junction path checks.
- tests/run_mcp_editor_test.py exercises real MCP stdio SDK and an isolated Godot editor with the actual extension. --visual verifies viewport PNG content; headless screenshot errors must return promptly. Only inspect or change the user's live scene when the task authorizes it.

## Performance invariants

- Start with README performance and real before/after probes. Core, Save, Service and Arc benchmarks measure separate workloads; new-count excludes engine allocations and cannot predict total FPS. Keep callback mutation, cancellation and GPU-output checks alongside timing.
- Event/coroutine registration cutoffs preserve deferred additions and slot reuse without snapshots. Reactive callable addresses stay stable; an executing self-cancelled slot cannot recycle until the outermost invocation returns. The Reactive/Dependency source must survive synchronous notification. The extra registration allocation is an explicit safety tradeoff.
- SaveCenter public reads deep-copy results. Internal pending partitions may share only private, unmodified slot snapshots; snapshot new caller payloads before staging and preserve byte-limit/checksum/durability checks.
- Input active contexts stay ordered by priority/latest activation; preserve same-name activations, exclusive/additive masks and same-frame edge suppression. Arc PackedArray pointers are valid only until a resize/COW detach; preserve trimmed/nested atlas clipping and test actual GPU pixels.
- Reflect scans a single unsaved translation unit with full function bodies and original-header diagnostics; do not disable diagnostics or filter away unmarked dependency headers. Release optimization follows godot-cpp optimize=speed/speed_trace/size rather than overriding all builds with size flags.

## Build and verification

Normal debug builds use scons at repository root without fixed -j 4/-j 8. Redirect logs into the workspace, wait for completion, then read the final useful output. New native registrations may require reopening Godot.

Choose the smallest appropriate README verification entry: reflect tests, binding driver (binding/debug/crash suites), atlas, MultiPatch, or Common Nodes. Use the actual Godot executable. GPU pixel checks require visual mode. Performance drivers and workload limits are documented at README performance; capture original binaries/revisions before rebuilding for comparisons. Keep all test drivers, native tests and GDScript fixtures under tests/. All Godot suites must use tests/godot_test_workspace.py prepare_workspace() and the single tests/workspace/meow project; never create random build test projects. Run suites serially and close previous test editors before resetting. Binding build copies live in tests/workspace; atlas cold-import checks clear caches in the same project. Logs/screenshots survive until the next suite; copy comparison reports elsewhere before resetting. Drivers must preserve the user's open editor; on managed Windows, isolate APPDATA, LOCALAPPDATA and TEMP/TMP.

Intentional CrashHandler tests are only appropriate when crash handling is in scope, and only against the driver's isolated process. Documentation-only edits need link/API/skill validation rather than native rebuilds.

Keep README examples synchronized with API changes and retain stable chapter anchors. If docs conflict with source, inspect source and correct the docs rather than restoring obsolete APIs.
