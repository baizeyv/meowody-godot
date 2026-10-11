//
// Created by baizeyv on 8/31/2026.
//

#ifndef GDMEOWODYFRAMEWORK_REACTIVE_DEPENDENCY_HPP
#define GDMEOWODYFRAMEWORK_REACTIVE_DEPENDENCY_HPP

#include <algorithm>
#include <vector>

namespace meowody
{
	class Dependency;

	class TrackingContext
	{
	  public:
		static thread_local Dependency* current;
	};

	class ScopedTrackingContext
	{
	  public:
		explicit ScopedTrackingContext(Dependency* current) noexcept :
			previous_(TrackingContext::current)
		{
			TrackingContext::current = current;
		}

		~ScopedTrackingContext() { TrackingContext::current = previous_; }

		ScopedTrackingContext(const ScopedTrackingContext&) = delete;

		ScopedTrackingContext& operator=(const ScopedTrackingContext&) = delete;

	  private:
		Dependency* previous_ = nullptr;
	};

	class Dependency
	{
	  public:
		virtual ~Dependency() { detach(); }

		void track()
		{
			Dependency* current = TrackingContext::current;

			if (current == nullptr || current == this)
			{
				return;
			}

			add_observer(current);
			current->add_dependency(this);
		}

		void notify()
		{
			const auto observers = observers_;

			for (Dependency* observer : observers)
			{
				if (observer == nullptr || !contains(observers_, observer))
				{
					continue;
				}

				observer->dependency_changed(this);
			}
		}

		virtual void dependency_changed(Dependency* source) = 0;

	  protected:
		void clear_dependencies() noexcept
		{
			// Reciprocal unlinking has no callbacks; retain capacity for the next tracking pass.
			while (!dependencies_.empty())
			{
				Dependency* dependency = dependencies_.back();
				dependencies_.pop_back();
				if (dependency == nullptr)
				{
					continue;
				}

				dependency->remove_observer(this);
			}
		}

	  private:
		static bool contains(const std::vector<Dependency*>& values,
							 Dependency*					 value) noexcept
		{
			return std::find(values.begin(), values.end(), value) !=
				   values.end();
		}

		static void erase_one(std::vector<Dependency*>& values,
							  Dependency*				value) noexcept
		{
			const auto it = std::find(values.begin(), values.end(), value);

			if (it != values.end())
			{
				values.erase(it);
			}
		}

		void add_observer(Dependency* observer)
		{
			if (observer == nullptr || contains(observers_, observer))
			{
				return;
			}

			observers_.push_back(observer);
		}

		void remove_observer(Dependency* observer) noexcept
		{
			erase_one(observers_, observer);
		}

		void add_dependency(Dependency* dependency)
		{
			if (dependency == nullptr || contains(dependencies_, dependency))
			{
				return;
			}

			dependencies_.push_back(dependency);
		}

		void remove_dependency(Dependency* dependency) noexcept
		{
			erase_one(dependencies_, dependency);
		}

		void detach() noexcept
		{
			while (!observers_.empty())
			{
				Dependency* observer = observers_.back();
				observers_.pop_back();
				if (observer == nullptr)
				{
					continue;
				}

				observer->remove_dependency(this);
			}

			clear_dependencies();
		}

	  private:
		std::vector<Dependency*> observers_;

		std::vector<Dependency*> dependencies_;
	};

	inline thread_local Dependency* TrackingContext::current = nullptr;
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_REACTIVE_DEPENDENCY_HPP
