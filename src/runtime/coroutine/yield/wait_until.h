//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_WAIT_UNTIL_H
#define GDMEOWODYFRAMEWORK_WAIT_UNTIL_H

#include <concepts>
#include <type_traits>
#include <utility>

#include "runtime/core/delegate.hpp"
#include "runtime/coroutine/i_yield.h"

namespace meowody
{
	class WaitUntil final : public IYield
	{
	  public:
		using Condition = Delegate<bool()>;
		using Update     = Delegate<void(double)>;

		explicit WaitUntil(Condition p_condition);

		WaitUntil(Condition p_condition, Update p_update);

	  protected:
		bool tick(double p_delta_time) override;

	  private:
		Condition m_condition;
		Update m_update;
	};

	template <typename F>
		requires std::invocable<std::decay_t<F>&>
	[[nodiscard]]
	WaitUntil wait_until(F&& p_condition)
	{
		using Condition = std::decay_t<F>;

		static_assert(std::is_convertible_v<std::invoke_result_t<Condition&>, bool>,
					  "wait_until condition must return bool.");

		return WaitUntil(WaitUntil::Condition(
			[condition = Condition(std::forward<F>(p_condition))]() mutable {
				return static_cast<bool>(condition());
			}));
	}

	template <typename ConditionFn, typename UpdateFn>
		requires std::invocable<std::decay_t<ConditionFn>&> &&
				 (std::invocable<std::decay_t<UpdateFn>&, double> ||
				  std::invocable<std::decay_t<UpdateFn>&>)
	[[nodiscard]]
	WaitUntil wait_until(ConditionFn&& p_condition, UpdateFn&& p_update)
	{
		using Condition = std::decay_t<ConditionFn>;
		using Update	= std::decay_t<UpdateFn>;

		static_assert(std::is_convertible_v<std::invoke_result_t<Condition&>, bool>,
					  "wait_until condition must return bool.");

		auto condition = WaitUntil::Condition(
			[condition = Condition(std::forward<ConditionFn>(p_condition))]()
				mutable { return static_cast<bool>(condition()); });

		auto update = WaitUntil::Update(
			[update = Update(std::forward<UpdateFn>(p_update))](
				double p_delta_time) mutable {
				if constexpr (std::invocable<Update&, double>)
				{
					update(p_delta_time);
				}
				else
				{
					update();
				}
			});

		return WaitUntil(std::move(condition), std::move(update));
	}
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_WAIT_UNTIL_H
