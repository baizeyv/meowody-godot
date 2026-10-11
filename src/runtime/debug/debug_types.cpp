#include "debug_types.h"

#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace meowody
{
	using namespace godot;
	static size_t record_bytes(const LogRecord& r)
	{
		size_t n = r.text.utf8().length() + r.tag.utf8().length() +
				   r.source.utf8().length();
		for (int i = 0; i < r.stack.size(); ++i)
			n += r.stack[i].utf8().length();
		return n;
	}
	void DebugLogBuffer::push(LogRecord r)
	{
		if (r.level < 0 || r.level > 4)
		{
			++dropped;
			++revision;
			return;
		}
		auto utf = r.text.utf8();
		if (size_t(utf.length()) > max_message)
		{
			auto length = max_message;
			while (length && (uint8_t(utf[length]) & 0xc0) == 0x80) --length;
			r.text = String::utf8(utf.get_data(), length) + " [truncated]";
		}
		if (r.stack.size() > 24) r.stack.resize(24);
		auto n = record_bytes(r);
		while (!entries.empty() &&
			   (entries.size() >= max_records || bytes + n > max_bytes))
		{
			bytes -= record_bytes(entries.front());
			entries.pop_front();
			++dropped;
		}
		if (n > max_bytes || max_records == 0)
		{
			++dropped;
			++revision;
			return;
		}
		entries.push_back(std::move(r));
		bytes += n;
		++revision;
	}
	DebugCommandResult parse_debug_command(const String&	  line,
										   PackedStringArray& out)
	{
		out.clear();
		String	 token;
		char32_t quote	= 0;
		bool	 escape = false, started = false;
		for (int i = 0; i < line.length(); ++i)
		{
			auto c = line[i];
			if (escape)
			{
				token += String::chr(c);
				escape	= false;
				started = true;
				continue;
			}
			if (c == '\\')
			{
				escape	= true;
				started = true;
				continue;
			}
			if (quote)
			{
				if (c == quote)
					quote = 0;
				else
					token += String::chr(c);
				continue;
			}
			if (c == '\'' || c == '"')
			{
				quote	= c;
				started = true;
				continue;
			}
			if (c == ' ' || c == '\t' || c == '\n' || c == '\r')
			{
				if (started)
				{
					out.push_back(token);
					token	= String();
					started = false;
				}
			}
			else
			{
				token += String::chr(c);
				started = true;
			}
		}
		if (quote || escape)
		{
			out.clear();
			return DebugCommandResult::failure(
				"Unclosed quote or trailing escape");
		}
		if (started) out.push_back(token);
		return DebugCommandResult::success();
	}
	static Variant typed_value(const Variant& value, int depth, int& budget)
	{
		Dictionary r;
		r["type"] = Variant::get_type_name(value.get_type());
		if (--budget < 0 || depth > 24)
		{
			r["value"] = "[depth limit]";
			return r;
		}
		if (value.get_type() == Variant::DICTIONARY)
		{
			Dictionary d = value;
			Array	   items;
			auto	   keys = d.keys();
			for (int i = 0; i < keys.size() && i < 2000 && budget > 0; ++i)
			{
				Dictionary item;
				item["key"]	  = typed_value(keys[i], depth + 1, budget);
				item["value"] = typed_value(d[keys[i]], depth + 1, budget);
				items.push_back(item);
			}
			r["value"]	   = items;
			r["truncated"] = keys.size() > 2000 || budget <= 0;
		}
		else if (value.get_type() == Variant::ARRAY)
		{
			Array a = value, items;
			for (int i = 0; i < a.size() && i < 2000 && budget > 0; ++i)
				items.push_back(typed_value(a[i], depth + 1, budget));
			r["value"]	   = items;
			r["truncated"] = a.size() > 2000 || budget <= 0;
		}
		else if (value.get_type() == Variant::BOOL ||
				 value.get_type() == Variant::STRING ||
				 value.get_type() == Variant::NIL)
			r["value"] = value;
		else
			r["value"] = String(
				value);	 // Preserve int64 and non-JSON Godot types as text.
		return r;
	}
	Variant debug_typed_value(const Variant& value, int depth)
	{
		int budget = 10000;
		return typed_value(value, depth, budget);
	}
	String debug_export_text(const String& text, const String& extension)
	{
		static uint64_t serial = 0;
		if (DirAccess::make_dir_recursive_absolute("user://debug_exports") !=
			OK)
			return {};
		auto path =
			String("user://debug_exports/debug_") +
			String::num_uint64(Time::get_singleton()->get_ticks_usec()) + "_" +
			String::num_uint64(++serial) + "." + extension;
		auto f = FileAccess::open(path, FileAccess::WRITE);
		if (f.is_null()) return {};
		f->store_string(text);
		f->flush();
		return f->get_error() == OK ? path : String();
	}
}  // namespace meowody
