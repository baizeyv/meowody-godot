//
// Created by baizeyv on 9/1/2026.
//

#ifndef GDMEOWODYFRAMEWORK_I_YIELD_H
#define GDMEOWODYFRAMEWORK_I_YIELD_H

namespace meowody
{
	class IYield
	{
	  public:
		virtual ~IYield() = default;

		bool update(double p_delta_time);

		void reset() noexcept;

	  protected:
		virtual bool tick(double p_delta_time) = 0;

		virtual void on_reset() noexcept {}

	  private:
		/**
		 * * 是否已经跳过一帧
		 */
		bool m_skipped = false;
	};
}  // namespace meowody

#endif	// GDMEOWODYFRAMEWORK_I_YIELD_H
