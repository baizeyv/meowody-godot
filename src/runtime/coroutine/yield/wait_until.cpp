//
// Created by baizeyv on 9/1/2026.
//

#include "wait_until.h"

namespace meowody
{
	WaitUntil::WaitUntil(Condition p_condition) :
		m_condition(std::move(p_condition))
	{
	}

	WaitUntil::WaitUntil(Condition p_condition, Update p_update) :
		m_condition(std::move(p_condition)),
		m_update(std::move(p_update))
	{
	}

	bool WaitUntil::tick(const double p_delta_time)
	{
		if (m_update)
		{
			m_update(p_delta_time);
		}

		return m_condition && m_condition();
	}
}  // namespace meowody
