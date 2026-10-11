//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_REACTIVE_VALUE_HPP
#define GDMEOWODYFRAMEWORK_REACTIVE_VALUE_HPP

#include <concepts>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "runtime/core/connection.hpp"
#include "runtime/core/delegate.hpp"
#include "runtime/reactive/dependency.hpp"

namespace meowody
{
	template <typename T>
	class Reactive : public Dependency
	{
	  private:
		using SubscriberCallback = Delegate<void(const T&)>;

		struct SubscriberSlot
		{
			std::unique_ptr<SubscriberCallback> callback;

			std::uint32_t generation = 1;

			std::uint32_t next_free = invalid_index();

			std::uint64_t connected_at = 0;
			std::uint32_t executing = 0;

			bool active = false;
		};

	  public:
		class ValueProxy
		{
		  public:
			explicit ValueProxy(Reactive* owner) noexcept : owner_(owner) {}

			operator const T&() const { return owner_->get(); }

			const T* operator->() const { return &owner_->get(); }

			const T& get() const { return owner_->get(); }

		  private:
			Reactive* owner_ = nullptr;
		};

	  public:
		ValueProxy value;

	  public:
		Reactive() :
			value(this), lifetime_(std::make_shared<ConnectionLifetime>())
		{
		}

		explicit Reactive(const T& initial) :
			value(this),
			lifetime_(std::make_shared<ConnectionLifetime>()),
			value_(initial)
		{
		}

		explicit Reactive(T&& initial) :
			value(this),
			lifetime_(std::make_shared<ConnectionLifetime>()),
			value_(std::move(initial))
		{
		}

		const T& get()
		{
			track();

			return value_;
		}

		const T& get() const noexcept { return value_; }

		Reactive& operator=(const T& new_value)
		{
			set(new_value);

			return *this;
		}

		Reactive& operator=(T&& new_value)
		{
			set(std::move(new_value));

			return *this;
		}

		template <typename U>
			requires std::assignable_from<T&, U&&>
		void set(U&& new_value)
		{
			if constexpr (requires(const T&							 current,
								   const std::remove_reference_t<U>& incoming) {
							  {
								  current == incoming
							  } -> std::convertible_to<bool>;
						  })
			{
				if (value_ == new_value)
				{
					return;
				}
			}

			value_ = std::forward<U>(new_value);

			notify();
			notify_subscribers();
		}

		template <typename F>
		Connection subscribe(F&& fn)
		{
			using Function = std::decay_t<F>;

			static_assert(
				std::is_invocable_v<Function&, const T&>,
				"Reactive subscriber must be callable as void(const T&).");

			const auto index = allocate_slot();

			SubscriberSlot& slot = subscribers_[index];

			slot.callback = std::make_unique<SubscriberCallback>(std::forward<F>(fn));
			slot.connected_at = ++subscription_sequence_;

			slot.active = true;

			return Connection(lifetime_, this, index, slot.generation,
							  &disconnect_impl);
		}

		void dependency_changed(Dependency*) override {}

	  private:
		static constexpr std::uint32_t invalid_index() noexcept
		{
			return (std::numeric_limits<std::uint32_t>::max)();
		}

		std::uint32_t allocate_slot()
		{
			if (free_head_ != invalid_index())
			{
				const auto index = free_head_;

				SubscriberSlot& slot = subscribers_[index];

				free_head_ = slot.next_free;

				slot.next_free = invalid_index();

				return index;
			}

			subscribers_.push_back({});

			return static_cast<std::uint32_t>(subscribers_.size() - 1);
		}

		void notify_subscribers()
		{
			const auto count = static_cast<std::uint32_t>(subscribers_.size());
			const auto cutoff = subscription_sequence_;

			for (std::uint32_t index = 0; index < count; ++index)
			{
				if (!subscribers_[index].active || subscribers_[index].connected_at > cutoff)
				{
					continue;
				}

				// The callable stays at a stable address; running slots cannot be reset or reused.
				auto* callback = subscribers_[index].callback.get();
				++subscribers_[index].executing;
				struct Guard
				{
					Reactive* owner;
					std::uint32_t index;
					~Guard() { owner->finish_invocation(index); }
				} guard{this, index};
				(*callback)(value_);
			}
		}

		void finish_invocation(std::uint32_t index) noexcept
		{
			auto& slot = subscribers_[index];
			if (--slot.executing == 0 && !slot.active) recycle_slot(index);
		}

		void recycle_slot(std::uint32_t index) noexcept
		{
			auto& slot = subscribers_[index];
			// Update bookkeeping before destroying captures that might disconnect other subscriptions.
			auto retired = std::move(slot.callback);
			slot.next_free = free_head_;
			free_head_ = index;
		}

		static void disconnect_impl(void* owner, std::uint32_t index,
									std::uint32_t generation) noexcept
		{
			static_cast<Reactive*>(owner)->disconnect(index, generation);
		}

		void disconnect(std::uint32_t index, std::uint32_t generation) noexcept
		{
			if (index >= subscribers_.size())
			{
				return;
			}

			SubscriberSlot& slot = subscribers_[index];

			if (!slot.active || slot.generation != generation)
			{
				return;
			}

			slot.active = false;

			++slot.generation;

			if (slot.generation == 0)
			{
				slot.generation = 1;
			}

			if (slot.executing == 0) recycle_slot(index);
		}

	  private:
		std::shared_ptr<ConnectionLifetime> lifetime_;

		T value_{};

		std::vector<SubscriberSlot> subscribers_;

		std::uint32_t free_head_ = invalid_index();
		std::uint64_t subscription_sequence_ = 0;
	};
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_REACTIVE_VALUE_HPP
