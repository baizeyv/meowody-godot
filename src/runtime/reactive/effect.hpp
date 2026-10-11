//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_EFFECT_HPP
#define GDMEOWODYFRAMEWORK_EFFECT_HPP

#include <utility>

#include "runtime/core/delegate.hpp"
#include "runtime/reactive/dependency.hpp"

namespace meowody
{
	class Effect : public Dependency
	{
	  public:
		template <typename F>
		explicit Effect(F&& fn) : callback_(std::forward<F>(fn))
		{
			run();
		}

		void dependency_changed(Dependency*) override { run(); }

	  private:
		void run()
		{
			clear_dependencies();

			const ScopedTrackingContext tracking(this);

			callback_();
		}

	  private:
		Delegate<void()> callback_;
	};

	template <typename F>
	auto observe(F&& fn)
	{
		return Effect(std::forward<F>(fn));
	}
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_EFFECT_HPP
