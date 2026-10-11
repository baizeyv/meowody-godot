"""Measure actual native ArcProgress draw CPU work; optionally capture GPU regression pixels.

Run against baseline/current DLL with identical --iterations. Compare the JSON medians
and optional pixels.bin outputs; geometry timings include RenderingServer submission.
"""
import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
from godot_test_workspace import prepare_workspace


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--godot", type=Path, required=True)
    parser.add_argument("--dll", type=Path)
    parser.add_argument("--visual", action="store_true")
    parser.add_argument("--iterations", type=int, default=200)
    args = parser.parse_args()
    if args.iterations < 1:
        parser.error("iterations must be positive")
    root = Path(__file__).resolve().parents[1]
    scratch = prepare_workspace()
    (scratch / ".godot").mkdir(parents=True)
    shutil.copy2(args.dll or root / "meow/bin/libmeowody.windows.template_debug.x86_64.dll", scratch / "native.dll")
    (scratch / "native.gdextension").write_text(
        '[configuration]\nentry_symbol="meowody_library_init"\ncompatibility_minimum="4.7"\n'
        '[libraries]\nwindows="res://native.dll"\n', encoding="utf-8")
    (scratch / ".godot/extension_list.cfg").write_text("res://native.gdextension\n", encoding="utf-8")
    (scratch / "project.godot").write_text(
        'config_version=5\n[application]\nconfig/name="Native UI performance"\n'
        '[display]\nwindow/size/viewport_width=1200\nwindow/size/viewport_height=600\n'
        '[rendering]\nrenderer/rendering_method="gl_compatibility"\n', encoding="utf-8")
    (scratch / "test.gd").write_text('''extends SceneTree
func _initialize():
    call_deferred("run_test")
func run_test():
    var viewport = SubViewport.new()
    viewport.size = Vector2i(1200,300)
    viewport.world_2d = World2D.new()
    viewport.render_target_update_mode = SubViewport.UPDATE_ALWAYS
    root.add_child(viewport)
    var image = Image.create(64,64,false,Image.FORMAT_RGBA8)
    for y in 64:
        for x in 64:
            image.set_pixel(x,y,Color(float(x)/63,float(y)/63,0.7,1))
    var page = ImageTexture.create_from_image(image)
    var atlas = AtlasTexture.new()
    atlas.atlas = page
    atlas.region = Rect2(8,6,42,40)
    atlas.margin = Rect2(5,7,14,18)
    var nested = AtlasTexture.new()
    nested.atlas = atlas
    nested.region = Rect2(2,4,44,42)
    nested.margin = Rect2(9,6,18,16)
    var gradient = Gradient.new()
    gradient.colors = PackedColorArray([Color.RED,Color.GREEN,Color.BLUE])
    gradient.offsets = PackedFloat32Array([0,0.4,1])
    var cases = [null, page, atlas, nested]
    var names = ["solid", "texture", "atlas", "nested_atlas"]
    var nodes = []
    var metrics = {}
    for index in cases.size():
        var arc = ClassDB.instantiate("ArcProgress")
        viewport.add_child(arc)
        arc.set_process(false)
        arc.size = Vector2(280,280)
        arc.position = Vector2(index*300+10,10)
        arc.thickness = 45
        arc.value = 0.77
        arc.start_angle = -130
        arc.end_angle = 197
        arc.rounded_caps = false
        arc.color = Color.WHITE
        arc.texture = cases[index]
        arc.gradient = gradient
        nodes.append(arc)
        var samples = []
        for sample in 6:
            var started = Time.get_ticks_usec()
            for iteration in ITERATIONS:
                RenderingServer.canvas_item_clear(arc.get_canvas_item())
                arc.notification(CanvasItem.NOTIFICATION_DRAW)
            if sample > 0:
                samples.append(float(Time.get_ticks_usec()-started)/ITERATIONS)
        samples.sort()
        metrics[names[index]] = samples[2]
        arc.queue_redraw()
    if VISUAL:
        await process_frame
        await RenderingServer.frame_post_draw
        var pixels = viewport.get_texture().get_image()
        pixels.save_png("res://pixels.png")
        var file = FileAccess.open("res://pixels.bin", FileAccess.WRITE)
        file.store_buffer(pixels.get_data())
        file.close()
    var output = FileAccess.open("res://metrics.json", FileAccess.WRITE)
    output.store_string(JSON.stringify(metrics))
    output.close()
    for node in nodes:
        node.free()
    viewport.free()
    print("[UIPerf] PASS ", JSON.stringify(metrics))
    quit(0)
'''.replace("ITERATIONS", str(args.iterations)).replace("VISUAL", "true" if args.visual else "false"), encoding="utf-8")
    env = os.environ.copy()
    for name in ("profile", "temp"):
        (scratch / name).mkdir()
        (scratch / name / ".gdignore").touch()
    env["APPDATA"] = env["LOCALAPPDATA"] = str(scratch / "profile")
    env["TEMP"] = env["TMP"] = str(scratch / "temp")
    command = [str(args.godot.resolve()), "--path", str(scratch), "--script", "res://test.gd"]
    if not args.visual:
        command.append("--headless")
    with (scratch / "runtime.log").open("w", encoding="utf-8") as log:
        result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT, timeout=90)
    output = (scratch / "runtime.log").read_text(encoding="utf-8", errors="replace")
    errors = [line for line in output.splitlines() if line.startswith(("ERROR:", "SCRIPT ERROR:")) and "root certificate store" not in line]
    if result.returncode or "[UIPerf] PASS" not in output or errors:
        raise RuntimeError(output)
    print("Microseconds/draw:", json.loads((scratch / "metrics.json").read_text(encoding="utf-8")))
    print("Artifacts:", scratch)


if __name__ == "__main__":
    main()
