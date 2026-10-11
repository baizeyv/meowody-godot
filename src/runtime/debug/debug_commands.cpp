#include <cmath>
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/json.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/classes/time.hpp>

#include "debug_center.h"
#include "runtime/app.h"

namespace meowody
{
	using namespace godot;
	String DebugCenter::stats() const
	{
		if (!App::get_singleton()) return "Runtime unavailable";
		String text =
			"Resources pending: " +
			String::num_uint64(App::resource->pending_count()) +
			"  LRU: " + String::num_uint64(App::resource->lru_size());
		text += "\nDownloads: " +
				String::num_uint64(App::download->pending_count()) +
				"  HTTP: " + String::num_uint64(App::net->pending_count());
		text += "\nUI active/cache: " +
				String::num_uint64(App::ui->active_count()) + " / " +
				String::num_uint64(App::ui->cache_size());
		text += "\nTweens: " +
				String::num_uint64(App::tween->active_count()) +
				"  Coroutines: " +
				String::num_uint64(App::coroutine->active_count());
		text += "\nSave pending/commits: " +
				String::num_uint64(App::save->pending_count()) + " / " +
				String::num_uint64(App::save->commit_count());
		auto pools = App::pool->names();
		for (int i = 0; i < pools.size(); ++i)
		{
			auto s = App::pool->stats(pools[i]);
			text += "\nPool " + pools[i] + ": " + String::num_uint64(s.idle) +
					" idle / " + String::num_uint64(s.active) + " active";
		}
		auto channels = App::audio->channels();
		for (int i = 0; i < channels.size(); ++i)
			text +=
				"\nAudio " + channels[i] + ": " +
				String::num_uint64(App::audio->active_count(channels[i])) +
				" voices";
		return text;
	}
	String DebugCenter::export_save(const String& part, const String& slot,
									bool disk, bool raw)
	{
		auto& saves		= App::save;
		auto  inspected = saves->inspect(part, raw || disk);
		if (!inspected.ok() && !raw) return "Error: " + inspected.message;
		if (raw)
		{
			auto from = saves->part_path(part);
			if (from.is_empty() || !FileAccess::file_exists(from))
				return "Error: no disk file";
			if (DirAccess::make_dir_recursive_absolute(
					"user://debug_exports") != OK)
				return "Error: export directory";
			auto path =
				String("user://debug_exports/") + part.to_lower() + "_" +
				String::num_uint64(Time::get_singleton()->get_ticks_usec()) +
				".sav";
			return DirAccess::copy_absolute(from, path) == OK
					   ? path
					   : String("Error: copy failed");
		}
		Variant data = inspected.data;
		if (!slot.is_empty())
		{
			if (!inspected.data.has(slot)) return "Error: slot not found";
			data = inspected.data[slot];
		}
		auto path = debug_export_text(
			JSON::stringify(debug_typed_value(data), "  "), "json");
		return path.is_empty() ? String("Error: export failed") : path;
	}
	void DebugCenter::install_commands()
	{
		auto add =
			[&](const char* name, const char* usage, const char* description,
				Delegate<DebugCommandResult(const DebugCommandContext&)> fn) {
				DebugCommand c;
				c.name		  = name;
				c.usage		  = usage;
				c.description = description;
				c.execute	  = std::move(fn);
				register_command(c);
				builtins.insert(name);
			};
		add("help", "help [command]", "List commands", [this](const auto& c) {
			String s;
			for (auto& [name, cmd] : commands)
				if (c.args.is_empty() || c.args[0] == name)
					s += cmd->usage + " - " + cmd->description + "\n";
			return DebugCommandResult::success(s);
		});
		add("clear", "clear", "Clear command output", [this](const auto&) {
			output.clear();
			return DebugCommandResult::success();
		});
		add("fps", "fps", "Current frames per second", [](const auto&) {
			return DebugCommandResult::success(String::num(
				Engine::get_singleton()->get_frames_per_second(), 1));
		});
		add("runtime.stats", "runtime.stats", "Runtime counters",
			[this](const auto&) {
				return DebugCommandResult::success(stats());
			});
		add("time", "time", "Local, UTC and network clock", [](const auto&) {
			return DebugCommandResult::success(
				App::time->format() + "\nUTC: " +
				App::time->format(TimeZone::UTC) + "\nNetwork synced: " +
				String(App::time->network_synced() ? "yes" : "no"));
		});
		add("pause", "pause", "Pause SceneTree (Runtime continues)",
			[](const auto&) {
				auto* t = Object::cast_to<SceneTree>(
					Engine::get_singleton()->get_main_loop());
				if (t) t->set_pause(true);
				return DebugCommandResult::success(
					"SceneTree paused; Runtime continues");
			});
		add("resume", "resume", "Resume SceneTree", [](const auto&) {
			auto* t = Object::cast_to<SceneTree>(
				Engine::get_singleton()->get_main_loop());
			if (t) t->set_pause(false);
			return DebugCommandResult::success("SceneTree resumed");
		});
		add("timescale", "timescale <0..10>", "Set engine time scale",
			[](const auto& c) {
				if (c.args.size() != 1 || !c.args[0].is_valid_float())
					return DebugCommandResult::failure(
						"Usage: timescale <0..10>");
				double v = c.args[0].to_float();
				if (!std::isfinite(v) || v < 0 || v > 10)
					return DebugCommandResult::failure("Expected finite 0..10");
				Engine::get_singleton()->set_time_scale(v);
				return DebugCommandResult::success("Time scale updated");
			});
		add("audio.volume", "audio.volume <channel> <0..1>",
			"Set channel volume", [](const auto& c) {
				if (c.args.size() != 2 || !c.args[1].is_valid_float())
					return DebugCommandResult::failure(
						"Usage: audio.volume <channel> <0..1>");
				double v = c.args[1].to_float();
				if (!std::isfinite(v) || v < 0 || v > 1 ||
					!App::audio->set_volume(c.args[0], v))
					return DebugCommandResult::failure(
						"Invalid channel or volume");
				return DebugCommandResult::success("Volume updated");
			});
		add("audio.mute", "audio.mute <channel> <true|false>", "Mute channel",
			[](const auto& c) {
				if (c.args.size() != 2 ||
					(c.args[1] != "true" && c.args[1] != "false") ||
					!App::audio->set_muted(c.args[0], c.args[1] == "true"))
					return DebugCommandResult::failure(
						"Usage: audio.mute <channel> <true|false>");
				return DebugCommandResult::success("Mute updated");
			});
		add("save.list", "save.list [part]", "List partitions or slots",
			[](const auto& c) {
				if (!c.args.is_empty()) {
					auto result = App::save->inspect(c.args[0]);
					if (!result.ok()) return DebugCommandResult::failure(result.message);
				}
				auto a = c.args.is_empty()
								 ? App::save->list_parts()
								 : App::save->list_slots(c.args[0]);
				return DebugCommandResult::success(String("\n").join(a));
			});
		add("save.show", "save.show <part> [slot] [disk]",
			"Inspect without changing data", [](const auto& c) {
				if (c.args.is_empty())
					return DebugCommandResult::failure("Expected part");
				auto r = App::save->inspect(
					c.args[0], c.args.size() > 2 && c.args[2] == "disk");
				if (!r.ok()) return DebugCommandResult::failure(r.message);
				Variant v = r.data;
				if (c.args.size() > 1)
				{
					if (!r.data.has(c.args[1]))
						return DebugCommandResult::failure("Slot not found");
					v = r.data[c.args[1]];
				}
				return DebugCommandResult::success(
					JSON::stringify(debug_typed_value(v), "  "));
			});
		add("save.export",
			"save.export <part> raw | save.export <part> json --plaintext "
			"[slot]",
			"Export raw file or explicit plaintext snapshot",
			[this](const auto& c) {
				if (c.args.size() == 2 && c.args[1] == "raw")
					return DebugCommandResult::success(
						export_save(c.args[0], {}, true, true));
				if (c.args.size() >= 3 && c.args[1] == "json" &&
					c.args[2] == "--plaintext")
					return DebugCommandResult::success(export_save(
						c.args[0], c.args.size() > 3 ? c.args[3] : String(),
						false, false));
				return DebugCommandResult::failure(
					"JSON exposes decrypted data. Use save.export <part> json "
					"--plaintext [slot], or <part> raw");
			});
	}
}  // namespace meowody
