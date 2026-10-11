#include "load_handle.h"
namespace meowody {
void detail::LoadState::cancel()
{
    suppress_callback = true;
    if (is_load_finished(status)) return;
    status = LoadStatus::Cancelled;
    progress = 1.0f;
    result = {{}, LoadError::Cancelled, godot::OK, "Load cancelled"};
}

}
