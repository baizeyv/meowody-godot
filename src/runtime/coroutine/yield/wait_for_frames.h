//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_WAIT_FOR_FRAMES_H
#define GDMEOWODYFRAMEWORK_WAIT_FOR_FRAMES_H

#include "runtime/coroutine/i_yield.h"

namespace meowody
{
	class WaitForFrames final : public IYield
	{
	  public:
		explicit WaitForFrames(int p_frames = 0);

	  protected:
		bool tick(double p_delta_time) override;

		void on_reset() noexcept override;

	  private:
		/**
		 * * 剩余帧数
		 */
		int m_remaining_frames = 0;

		int m_frame_count = 0;
	};

	[[nodiscard]]
	WaitForFrames wait_for_frames(int p_frames);
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_WAIT_FOR_FRAMES_H
