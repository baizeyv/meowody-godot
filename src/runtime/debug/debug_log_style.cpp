#include "debug_log_style.h"
namespace meowody
{
	std::vector<DebugLogSpan> debug_log_spans(const LogRecord& r)
	{
		using namespace godot;
		static const char* colors[] = {"808080", "55AAFF", "FFCC55", "FF5555",
									   "FF00FF"};
		std::vector<Color> stack{
			Color::html(colors[r.level >= 0 && r.level < 5 ? r.level : 1])};
		std::vector<DebugLogSpan> spans;
		String					  pending;
		auto					  flush = [&] {
			if (!pending.is_empty())
			{
				spans.push_back({pending, stack.back()});
				pending = "";
			}
		};
		for (int64_t i = 0; i < r.text.length();)
		{
			if (r.rich && r.text[i] == '[')
			{
				auto end = r.text.find("]", i);
				if (end >= 0)
				{
					auto tag = r.text.substr(i + 1, end - i - 1);
					if (tag.begins_with("color=") && stack.size() < 32)
					{
						auto value = tag.substr(6);
						// Unknown tags stay literal. No URLs, images or
						// resource loading.
						Color invalid(-1, -1, -1, -1);
						auto  color = Color::from_string(value, invalid);
						if (color != invalid)
						{
							flush();
							stack.push_back(color);
							i = end + 1;
							continue;
						}
					}
					else if (tag == "/color" && stack.size() > 1)
					{
						flush();
						stack.pop_back();
						i = end + 1;
						continue;
					}
				}
			}
			pending += r.text.substr(i++, 1);
		}
		flush();
		return spans;
	}
}  // namespace meowody
