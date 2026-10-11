#pragma once
#include <vector>

#include "tween.h"
namespace meowody
{
	class TweenCenter
	{
		std::shared_ptr<tween::detail::Core> core;

	  public:
		TweenCenter();
		~TweenCenter();
		TweenCenter(const TweenCenter&)								  = delete;
		TweenCenter&					operator=(const TweenCenter&) = delete;
		void							update();
		void							shutdown();
		bool							is_closed() const;
		std::vector<tween::TweenHandle> find(godot::Object* target) const;
		std::vector<tween::TweenHandle> find(const godot::StringName& id) const;
		size_t							kill(godot::Object*);
		size_t							kill(const godot::StringName&);
		size_t							pause(godot::Object*);
		size_t							pause(const godot::StringName&);
		size_t							resume(godot::Object*);
		size_t							resume(const godot::StringName&);
		void							kill_all();
		size_t							active_count() const;
		// Internal factory used by the non-template front end.
		std::shared_ptr<tween::detail::State> make();
	};
}  // namespace meowody
