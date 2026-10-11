//
// Created by baizeyv on 9/1/2026.
//

#include "coroutine.h"

namespace meowody
{
	bool Coroutine::move_next(const double p_delta_time)
	{
		while (!m_steps.empty())
		{
			if (!m_steps.front()(p_delta_time))
			{
				return false;
			}

			m_steps.pop_front();
		}

		return true;
	}

	bool Coroutine::finished() const noexcept
	{
		return m_steps.empty();
	}

	bool Coroutine::empty() const noexcept
	{
		return m_steps.empty();
	}

	void Coroutine::clear() noexcept
	{
		m_steps.clear();
	}

	Coroutine& Coroutine::push_step(Step p_step)
	{
		assert(p_step.valid());
		m_steps.emplace_back(std::move(p_step));
		return *this;
	}

	Coroutine& Coroutine::yield(std::shared_ptr<Coroutine> p_coroutine)
	{
		assert(p_coroutine != nullptr);

		return push_step(Step(
			[coroutine = std::move(p_coroutine)](double p_delta_time) mutable {
				return coroutine->move_next(p_delta_time);
			}));
	}

	std::shared_ptr<Coroutine> Coroutine::make()
	{
		return std::make_shared<Coroutine>();
	}
}  // namespace meowody
