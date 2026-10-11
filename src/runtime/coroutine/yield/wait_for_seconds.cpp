//
// Created by baizeyv on 9/1/2026.
//

#include "wait_for_seconds.h"

#include <algorithm>

namespace meowody
{
	WaitForSeconds::WaitForSeconds(const double p_seconds) :
		m_remaining((std::max)(0.0, p_seconds)),
		m_duration((std::max)(0.0, p_seconds))
	{
	}

	bool WaitForSeconds::tick(const double p_delta_time)
	{
		m_remaining -= p_delta_time;
		return m_remaining <= 0.0;
	}

	void WaitForSeconds::on_reset() noexcept
	{
		m_remaining = m_duration;
	}

	WaitForSeconds wait_for_seconds(const double p_seconds)
	{
		return WaitForSeconds(p_seconds);
	}
}  // namespace meowody