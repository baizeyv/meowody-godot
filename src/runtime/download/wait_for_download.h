#pragma once
#include "download_handle.h"
#include "runtime/coroutine/i_yield.h"

namespace meowody
{
class WaitForDownload final : public IYield
{
public:
    explicit WaitForDownload(DownloadHandle handle) : m_handle(std::move(handle)) {}
protected:
    bool tick(double) override { return m_handle.finished(); }
private:
    DownloadHandle m_handle;
};
inline auto wait_for_download(DownloadHandle handle) { return WaitForDownload(std::move(handle)); }
}
