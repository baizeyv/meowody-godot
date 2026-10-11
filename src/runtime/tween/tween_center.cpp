#include <algorithm>

#include "tween_internal.h"
namespace meowody
{
	using namespace tween;
	using namespace tween::detail;
	namespace
	{
		bool targets(const State& s, uint64_t id)
		{
			if (s.target == id || s.owner == id) return true;
			for (auto& child : s.children)
				if (targets(*child.value, id)) return true;
			return false;
		}
		bool live(const State& s)
		{
			return s.status == Status::Pending || s.status == Status::Playing ||
				   s.status == Status::Paused;
		}
		bool bound_to(const State& s, uint64_t id)
		{
			if (s.owner == id) return true;
			for (auto& child : s.children)
				if (bound_to(*child.value, id)) return true;
			return false;
		}
	}  // namespace
	TweenCenter::TweenCenter() : core(std::make_shared<Core>()) {}
	TweenCenter::~TweenCenter() { shutdown(); }
	std::shared_ptr<State> TweenCenter::make()
	{
		auto s	= std::make_shared<State>();
		s->core = core;
		s->id	= core->next_id++;
		if (core->stopped)
			fail(*s, "TweenCenter is closed");
		else
			core->active.push_back(s);
		return s;
	}
	void TweenCenter::update()
	{
		if (core->stopped || core->updating) return;
		core->updating = true;
		auto snapshot  = core->active;
		for (auto& s : snapshot)
		{
			if (core->stopped) break;
			if (s->nested || s->status == Status::Completed ||
				s->status == Status::Cancelled || s->status == Status::Failed)
				continue;
			if (!owners_valid(*s))
			{
				command(s, {Op::Kill});
				continue;
			}
			if (s->status == Status::Pending)
			{
				if (!compile(*s)) continue;
				s->status = Status::Playing;
				claim(s);
				if (s->status == Status::Playing && !core->stopped)
				{
					driver_start(s);
					if (s->length == 0) advance_timeline(s, 0);
				}
			}
		}
		std::erase_if(core->active, [](auto& s) {
			return s->nested || s->status == Status::Completed ||
				   s->status == Status::Cancelled ||
				   s->status == Status::Failed;
		});
		core->updating = false;
	}
	void TweenCenter::shutdown()
	{
		if (core->stopped) return;
		core->stopped = true;
		for (auto& s : core->active)
		{
			if (s->status == Status::Completed ||
				s->status == Status::Cancelled || s->status == Status::Failed)
				continue;
			s->status = Status::Cancelled;
			mark_children(*s, s->status);
			++s->revision;
			s->commands.clear();
			driver_stop(*s);
		}
		core->active.clear();
	}
	bool TweenCenter::is_closed() const { return core->stopped; }
	std::vector<TweenHandle> TweenCenter::find(godot::Object* obj) const
	{
		std::vector<TweenHandle> out;
		if (!obj) return out;
		auto id = uint64_t(obj->get_instance_id());
		for (auto& s : core->active)
			if (!s->nested && live(*s))
			{
				bool found = !s->compiled && targets(*s, id);
				found |= bound_to(*s, id);
				for (auto& t : s->tracks)
					found |= t.active && t.leaf->target == id;
				if (found) out.emplace_back(s);
			}
		return out;
	}
	std::vector<TweenHandle> TweenCenter::find(
		const godot::StringName& tag) const
	{
		std::vector<TweenHandle> out;
		for (auto& s : core->active)
			if (!s->nested && live(*s) && s->tag == tag) out.emplace_back(s);
		return out;
	}
#define TWEEN_BULK(name, method, type)   \
	size_t TweenCenter::name(type key)   \
	{                                    \
		auto list = find(key);           \
		for (auto& h : list) h.method(); \
		return list.size();              \
	}
	TWEEN_BULK(kill, kill, godot::Object*)
	TWEEN_BULK(kill, kill, const godot::StringName&)
		TWEEN_BULK(pause, pause, godot::Object*)
			TWEEN_BULK(pause, pause, const godot::StringName&)
				TWEEN_BULK(resume, play, godot::Object*)
					TWEEN_BULK(resume, play, const godot::StringName&)
#undef TWEEN_BULK
						void TweenCenter::kill_all()
	{
		auto snapshot = core->active;
		for (auto& s : snapshot)
			if (!s->nested) command(s, {Op::Kill});
	}
	size_t TweenCenter::active_count() const
	{
		return std::count_if(
			core->active.begin(), core->active.end(), [](auto& s) {
				return !s->nested && (s->status == Status::Pending ||
									  s->status == Status::Playing ||
									  s->status == Status::Paused);
			});
	}
}  // namespace meowody
