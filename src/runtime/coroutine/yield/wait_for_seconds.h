//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_WAIT_FOR_SECONDS_H
#define GDMEOWODYFRAMEWORK_WAIT_FOR_SECONDS_H

#include "runtime/coroutine/i_yield.h"

namespace meowody
{
	class WaitForSeconds final : public IYield
	{
	  public:
		explicit WaitForSeconds(double p_seconds = 0.0);

	  protected:
		bool tick(double p_delta_time) override;

		void on_reset() noexcept override;

	  private:
		/**
		 * * 剩余时长
		 */
		double m_remaining = 0.0;

		double m_duration = 0.0;
	};

	[[nodiscard]]
	WaitForSeconds wait_for_seconds(double p_seconds);
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_WAIT_FOR_SECONDS_H
