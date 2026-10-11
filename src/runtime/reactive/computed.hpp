//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_COMPUTED_HPP
#define GDMEOWODYFRAMEWORK_COMPUTED_HPP

#include <type_traits>
#include <utility>

#include "runtime/core/delegate.hpp"
#include "runtime/reactive/dependency.hpp"

namespace meowody
{
	template <typename T>
	class Computed : public Dependency
	{
	  public:
		class ValueProxy
		{
		  public:
			explicit ValueProxy(Computed* owner) noexcept : owner_(owner) {}

			operator const T&() const { return owner_->get(); }

			const T& get() const { return owner_->get(); }

		  private:
			Computed* owner_ = nullptr;
		};

	  public:
		ValueProxy value;

	  public:
		template <typename F>
		explicit Computed(F&& getter) :
			value(this), getter_(std::forward<F>(getter))
		{
		}

		const T& get()
		{
			track();

			if (dirty_)
			{
				recompute();
			}

			return value_;
		}

		const T& get() const noexcept { return value_; }

		void dependency_changed(Dependency*) override
		{
			if (!dirty_)
			{
				dirty_ = true;
				notify();
			}
		}

	  private:
		void recompute()
		{
			clear_dependencies();

			const ScopedTrackingContext tracking(this);

			value_ = getter_();

			dirty_ = false;
		}

	  private:
		Delegate<T()> getter_;

		T value_{};

		bool dirty_ = true;
	};

	template <typename F>
	auto computed(F&& getter)
	{
		using Value = std::invoke_result_t<std::decay_t<F>&>;

		return Computed<Value>(std::forward<F>(getter));
	}
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_COMPUTED_HPP
