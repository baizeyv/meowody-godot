#include "CrashProbe.h"
#include "log/log.hpp"
#include <windows.h>
namespace {
volatile uintptr_t fault_address = 0x18;
__declspec(noinline) void crash_probe_fault_site() {
    *reinterpret_cast<volatile int *>(fault_address) = 42;
}
}
bool CrashProbe::handled_fault() {
    __try { crash_probe_fault_site(); }
    __except(EXCEPTION_EXECUTE_HANDLER) { return true; }
    return false;
}
void CrashProbe::crash() {
    meowody::Log::info(godot::StringName("CrashProbe")) << "CrashProbeMarker: last operation before access violation";
    crash_probe_fault_site();
    meowody::Log::info(godot::StringName("CrashProbe")) << "Unexpected return from the fault site";
}
