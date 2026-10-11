//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_COROUTINE_H
#define GDMEOWODYFRAMEWORK_COROUTINE_H

#include <cassert>
#include <concepts>
#include <deque>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#include "runtime/core/delegate.hpp"
#include "runtime/coroutine/i_yield.h"

namespace meowody
{
	class Coroutine final
	{
	  public:
		using Step = Delegate<bool(double)>;

		Coroutine() = default;

		bool move_next(double p_delta_time);

		[[nodiscard]]
		bool finished() const noexcept;

		[[nodiscard]]
		bool empty() const noexcept;

		void clear() noexcept;

		Coroutine& push_step(Step p_step);

		template <typename F>
			requires std::invocable<std::decay_t<F>&>
		Coroutine& then(F&& p_fn)
		{
			using Function = std::decay_t<F>;

			return push_step(Step([callback = Function(std::forward<F>(p_fn))](
									  double) mutable {
				if constexpr (std::is_void_v<std::invoke_result_t<Function&>>)
				{
					std::invoke(callback);
				}
				else
				{
					static_cast<void>(std::invoke(callback));
				}

				return true;
			}));
		}

		template <typename Y>
			requires std::derived_from<std::remove_cvref_t<Y>, IYield>
		Coroutine& yield(Y&& p_yield)
		{
			using YieldType = std::remove_cvref_t<Y>;

			YieldType yield_condition(std::forward<Y>(p_yield));
			yield_condition.reset();

			return push_step(
				Step([yield_condition = std::move(yield_condition)](
						 double p_delta_time) mutable {
					return yield_condition.update(p_delta_time);
				}));
		}

		Coroutine& yield(std::shared_ptr<Coroutine> p_coroutine);

		static std::shared_ptr<Coroutine> make();

	  private:
		std::deque<Step> m_steps;
	};

	template <typename F>
		requires(
			!std::derived_from<std::remove_cvref_t<F>, IYield> &&
			!std::same_as<std::remove_cvref_t<F>, std::shared_ptr<Coroutine>> &&
			std::invocable<std::decay_t<F>&>)
	inline Coroutine& operator<<(Coroutine& p_coroutine, F&& p_fn)
	{
		return p_coroutine.then(std::forward<F>(p_fn));
	}

	template <typename Y>
		requires std::derived_from<std::remove_cvref_t<Y>, IYield>
	inline Coroutine& operator<<(Coroutine& p_coroutine, Y&& p_yield)
	{
		return p_coroutine.yield(std::forward<Y>(p_yield));
	}

	inline Coroutine& operator<<(Coroutine&					p_coroutine,
								 std::shared_ptr<Coroutine> p_nested_coroutine)
	{
		return p_coroutine.yield(std::move(p_nested_coroutine));
	}

	inline Coroutine& operator<<(Coroutine& p_coroutine, Coroutine::Step p_step)
	{
		return p_coroutine.push_step(std::move(p_step));
	}

	template <typename T>
	inline std::shared_ptr<Coroutine>& operator<<(
		std::shared_ptr<Coroutine>& p_coroutine, T&& p_value)
	{
		assert(p_coroutine != nullptr);
		*p_coroutine << std::forward<T>(p_value);
		return p_coroutine;
	}

	template <typename T>
	inline const std::shared_ptr<Coroutine>& operator<<(
		const std::shared_ptr<Coroutine>& p_coroutine, T&& p_value)
	{
		assert(p_coroutine != nullptr);
		*p_coroutine << std::forward<T>(p_value);
		return p_coroutine;
	}
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_COROUTINE_H
