//
// Created by baizeyv on 9/1/2026.
//

#include "i_yield.h"

namespace meowody
{
	bool IYield::update(const double p_delta_time)
	{
		if (!m_skipped)
		{
			m_skipped = true;
			return false;
		}

		return tick(p_delta_time);
	}

	void IYield::reset() noexcept
	{
		m_skipped = false;
		on_reset();
	}
}  // namespace meowody