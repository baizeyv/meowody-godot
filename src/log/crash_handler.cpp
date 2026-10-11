#include "crash_handler.h"
#if !defined(_WIN32) || !defined(DEBUG_ENABLED)
namespace meowody {
void CrashHandler::initialize() {}
void CrashHandler::shutdown() {}
}
#endif
