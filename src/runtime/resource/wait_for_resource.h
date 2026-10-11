#ifndef GDMEOWODYFRAMEWORK_WAIT_FOR_RESOURCE_H
#define GDMEOWODYFRAMEWORK_WAIT_FOR_RESOURCE_H

#include "resource_center.h"
#include "runtime/coroutine/i_yield.h"

namespace meowody
{
template <typename Handle>
class WaitForResource final : public IYield
{
public:
    explicit WaitForResource(Handle handle) : m_handle(std::move(handle)) {}
protected:
    bool tick(double) override { return m_handle.finished(); }
private:
    Handle m_handle;
};

template <typename T>
auto wait_for_resource(LoadHandle<T> handle) { return WaitForResource<LoadHandle<T>>(std::move(handle)); }
inline auto wait_for_resources(BatchLoadHandle handle) { return WaitForResource<BatchLoadHandle>(std::move(handle)); }
}
#endif
