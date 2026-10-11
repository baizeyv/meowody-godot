#include "debug_center.h"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/engine_debugger.hpp>
#include <godot_cpp/classes/time.hpp>

#include "debug_overlay.h"
#include "log/log.hpp"
#include "runtime/app.h"

namespace meowody
{
	using namespace godot;
	DebugCenter::DebugCenter()
	{
		install_commands();
#ifdef DEBUG_ENABLED
		set_enabled(true);
#endif
	}
	DebugCenter::~DebugCenter() { shutdown(); }
	void DebugCenter::set_enabled(bool value)
	{
		if (stopped || enabled == value) return;
		enabled = value;
		if (value)
			log_connection = Log::subscribe(
				[this](const LogRecord& record) { incoming.push(record); });
		else
		{
			log_connection.disconnect();
			incoming.clear();
			queue.clear();
			close();
			if (auto* overlay = Object::cast_to<DebugOverlay>(
					ObjectDB::get_instance(ObjectID(overlay_id))))
				overlay->set_visible(false);
		}
	}
	void DebugCenter::ensure_overlay()
	{
		if (!enabled || Engine::get_singleton()->is_editor_hint() ||
			DisplayServer::get_singleton()->get_name() == "headless")
			return;
		auto* runtime = App::get_singleton();
		if (!runtime || !runtime->is_inside_tree()) return;
		auto* overlay = Object::cast_to<DebugOverlay>(
			ObjectDB::get_instance(ObjectID(overlay_id)));
		if (!overlay)
		{
			overlay = memnew(DebugOverlay);
			runtime->add_child(overlay);
			overlay_id = uint64_t(overlay->get_instance_id());
			overlay->setup(this);
		}
		overlay->set_visible(true);
	}
	bool DebugCenter::captures_input() const
	{
		return is_open() && overlay_id &&
			   ObjectDB::get_instance(ObjectID(overlay_id));
	}
	void DebugCenter::open()
	{
		if (!enabled || stopped) return;
		opened = true;
	}
	void DebugCenter::close()
	{
		opened = false;
		if (auto* overlay = Object::cast_to<DebugOverlay>(
				ObjectDB::get_instance(ObjectID(overlay_id))))
			overlay->show_modal(false);
	}
	bool DebugCenter::register_command(DebugCommand&& command)
	{
		if (stopped || command.name.is_empty() || !command.execute ||
			commands.contains(command.name))
			return false;
		for (int i = 0; i < command.name.length(); ++i)
		{
			auto c = command.name[i];
			if (!(c == '.' || c == '_' || c == '-' || (c >= 'a' && c <= 'z') ||
				  (c >= '0' && c <= '9')))
				return false;
		}
		auto name = command.name;
		commands.emplace(name,
						 std::make_shared<DebugCommand>(std::move(command)));
		return true;
	}
	bool DebugCenter::unregister_command(const String& name)
	{
		return !builtins.contains(name) && commands.erase(name) > 0;
	}
	bool DebugCenter::execute_command(const String& text)
	{
		if (!enabled || stopped || text.length() > 16384 || queue.size() >= 128)
			return false;
		queue.push_back(text);
		return true;
	}
	PackedStringArray DebugCenter::complete(const String& line) const
	{
		PackedStringArray args, result;
		if (!parse_debug_command(line, args).ok) return result;
		if (args.size() <= 1 && !line.ends_with(" "))
		{
			for (const auto& [name, c] : commands)
				if (name.begins_with(line)) result.push_back(name);
		}
		else if (args.size() && commands.contains(args[0]))
		{
			auto c = commands.at(args[0]);
			args.remove_at(0);
			if (c->complete) return c->complete({line, args});
		}
		return result;
	}
	void DebugCenter::print(const String& text, bool error)
	{
		if (stopped) return;
		output.push_back(text.substr(0, 16384));
		if (output.size() > 1000) output.pop_front();
		Log::print(error ? Log::Level::Error : Log::Level::Info, text,
				   StringName("Console"));
	}
	bool DebugCenter::register_panel(const String& id, const String& title,
									 Delegate<Control*()> factory)
	{
		if (stopped || id.is_empty() || !factory || panels.contains(id))
			return false;
		panels.emplace(
			id, std::make_shared<Panel>(Panel{title, std::move(factory)}));
		++panel_revision;
		return true;
	}
	bool DebugCenter::unregister_panel(const String& id)
	{
		if (!panels.erase(id)) return false;
		++panel_revision;
		return true;
	}
	void DebugCenter::update(double)
	{
		if (!enabled || stopped || busy) return;
		busy = true;
		struct Guard
		{
			bool& value;
			~Guard() { value = false; }
		} guard{busy};
		auto pending = std::move(queue);
		queue.clear();
		for (const auto& line : pending)
		{
			if (stopped || !enabled) break;
			history.push_back(line);
			if (history.size() > 100) history.pop_front();
			print("> " + line);
			PackedStringArray args;
			auto			  parsed = parse_debug_command(line, args);
			if (!parsed.ok)
			{
				print(parsed.text, true);
				continue;
			}
			if (!args.size()) continue;
			auto found = commands.find(args[0]);
			if (found == commands.end())
			{
				print("Unknown command: " + args[0], true);
				continue;
			}
			auto command = found->second;
			args.remove_at(0);
			try
			{
				auto result = command->execute({line, args});
				if (!result.text.is_empty()) print(result.text, !result.ok);
			} catch (const std::exception& e)
			{
				print(String("Command failed: ") + e.what(), true);
			} catch (...)
			{
				print("Command failed with unknown exception", true);
			}
		}
		if (stopped || !enabled) return;
		auto* debugger = EngineDebugger::get_singleton();
		const bool transport_active = !Engine::get_singleton()->is_editor_hint() &&
			debugger && debugger->is_active();
		Array  batch;
		size_t transport_bytes = 0, transport_dropped = 0;
		for (const auto& r : incoming.records())
		{
			logs.push(r);
			if (!transport_active) continue;
			size_t bytes = r.text.utf8().length() + r.tag.utf8().length() + 256;
			for (int i = 0; i < r.stack.size(); ++i)
				bytes += r.stack[i].utf8().length();
			if (batch.size() < 127 && transport_bytes + bytes < 60 * 1024)
			{
				Array item;
				item.push_back(int64_t(r.sequence));
				item.push_back(r.timestamp);
				item.push_back(r.level);
				item.push_back(r.tag);
				item.push_back(r.text);
				item.push_back(r.stack);
				item.push_back(r.rich);
				batch.push_back(item);
				transport_bytes += bytes;
			}
			else
				++transport_dropped;
		}
		if (transport_dropped)
		{
			Array item;
			item.push_back(int64_t(0));
			item.push_back(Time::get_singleton()->get_unix_time_from_system());
			item.push_back(2);
			item.push_back(String("Transport"));
			item.push_back(String::num_uint64(transport_dropped) +
						   " records omitted from editor transport budget; see "
						   "runtime log buffer");
			item.push_back(PackedStringArray());
			batch.push_back(item);
		}
		if (transport_active && !batch.is_empty())
			debugger->send_message("meowody_log:records", batch);
		if (incoming.dropped) logs.dropped += incoming.dropped;
		incoming.clear();
		ensure_overlay();
		if (auto* overlay = Object::cast_to<DebugOverlay>(
				ObjectDB::get_instance(ObjectID(overlay_id))))
			overlay->refresh();
	}
	void DebugCenter::shutdown()
	{
		if (stopped) return;
		close();
		stopped = true;
		enabled = false;
		log_connection.disconnect();
		if (auto* overlay = Object::cast_to<DebugOverlay>(
				ObjectDB::get_instance(ObjectID(overlay_id))))
		{
			overlay->detach();
			overlay->queue_free();
		}
		overlay_id = 0;
		panels.clear();
		commands.clear();
		queue.clear();
		incoming.clear();
		logs.clear();
	}
}  // namespace meowody
