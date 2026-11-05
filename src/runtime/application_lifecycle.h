#pragma once
#include "save/save_types.h"

namespace meowody {
struct ApplicationState {
    bool focused = true;
    bool system_paused = false;
    bool minimized = false;
    bool background = false;
    bool game_paused = false;
    bool quitting = false;
};
enum class ApplicationEventKind { FocusChanged, Suspended, Resumed, LowMemory, Quitting };
struct ApplicationEvent {
    ApplicationEventKind kind;
    ApplicationState state;
    double background_seconds = 0;
};
enum class ApplicationRequestKind { Back, Quit };
struct ApplicationRequest {
    ApplicationRequestKind kind;
    void consume() const { consumed = true; }
    bool is_consumed() const { return consumed; }
private:
    mutable bool consumed = false;
public:
    explicit ApplicationRequest(ApplicationRequestKind value) : kind(value) {}
};
enum class ApplicationSaveReason { Suspend, LowMemory, Quit };
struct ApplicationSaveFailed {
    ApplicationSaveReason reason;
    SaveStatus status;
};
}
