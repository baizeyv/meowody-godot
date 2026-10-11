#include "../log_record.h"
#include "../log.hpp"
#include "../crash_handler.h"
#include <godot_cpp/classes/dir_access.hpp>
#include <godot_cpp/classes/project_settings.hpp>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <dbghelp.h>
#include <tlhelp32.h>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <cstdarg>
#include <cstring>

namespace meowody {
namespace {
// A private symbol session avoids corrupting Godot's own DbgHelp session.
HANDLE symbols = nullptr;
std::mutex symbol_mutex, recent_mutex;
Connection recent_connection;
LPTOP_LEVEL_EXCEPTION_FILTER previous_filter = nullptr;
wchar_t log_directory[32768]{};
char recent[200][2048]{};
unsigned recent_count = 0, recent_next = 0;
volatile LONG reporting = 0;
DWORD main_thread = 0;

void write_text(HANDLE file, const char *format, ...) {
    char text[4096];
    va_list args; va_start(args, format);
    int length = vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    if (length < 0) return;
    DWORD size = DWORD((std::min)(size_t(length), sizeof(text) - 1)), written;
    if (file != INVALID_HANDLE_VALUE) WriteFile(file, text, size, &written, nullptr);
    HANDLE error = GetStdHandle(STD_ERROR_HANDLE);
    if (error && error != INVALID_HANDLE_VALUE) WriteFile(error, text, size, &written, nullptr);
}
void load_modules() {
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (snapshot == INVALID_HANDLE_VALUE) return;
    MODULEENTRY32W entry{}; entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) do {
        SymLoadModuleExW(symbols, nullptr, entry.szExePath, nullptr,
            reinterpret_cast<DWORD64>(entry.modBaseAddr), entry.modBaseSize, nullptr, 0);
    } while (Module32NextW(snapshot, &entry));
    CloseHandle(snapshot);
}
void format_frame(DWORD64 address, char *text, size_t capacity, bool resolve) {
    char module[MAX_PATH] = "unknown";
    MEMORY_BASIC_INFORMATION info{};
    DWORD64 base = 0;
    if (VirtualQuery(reinterpret_cast<void *>(address), &info, sizeof(info))) {
        base = reinterpret_cast<DWORD64>(info.AllocationBase);
        GetModuleFileNameA(reinterpret_cast<HMODULE>(info.AllocationBase), module, MAX_PATH);
    }
    alignas(SYMBOL_INFO) char storage[sizeof(SYMBOL_INFO) + 1024]{};
    auto *symbol = reinterpret_cast<SYMBOL_INFO *>(storage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO); symbol->MaxNameLen = 1023;
    DWORD64 displacement = 0;
    bool found = resolve && SymFromAddr(symbols, address, &displacement, symbol);
    IMAGEHLP_LINE64 line{}; line.SizeOfStruct = sizeof(line); DWORD offset = 0;
    bool located = resolve && SymGetLineFromAddr64(symbols, address, &offset, &line);
    int used = found
        ? snprintf(text, capacity, "0x%llX %s+0x%llX  %s+0x%llX%s", address, module, address - base,
            symbol->Name, displacement, (symbol->Flags & SYMFLAG_EXPORT) ? " (nearest export)" : "")
        : snprintf(text, capacity, "0x%llX %s+0x%llX  (symbols unavailable)", address, module, address - base);
    if (located && used > 0 && size_t(used) < capacity)
        snprintf(text + used, capacity - used, "  %s:%lu", line.FileName, line.LineNumber);
}
BOOL CALLBACK read_memory(HANDLE, DWORD64 address, PVOID buffer, DWORD size, LPDWORD read) {
    SIZE_T amount = 0;
    BOOL ok = ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<void *>(address), buffer, size, &amount);
    *read = DWORD(amount); return ok;
}
const char *exception_name(DWORD code) {
    switch (code) {
        case EXCEPTION_ACCESS_VIOLATION: return "ACCESS_VIOLATION";
        case EXCEPTION_STACK_OVERFLOW: return "STACK_OVERFLOW";
        case EXCEPTION_INT_DIVIDE_BY_ZERO: return "INTEGER_DIVIDE_BY_ZERO";
        case EXCEPTION_ILLEGAL_INSTRUCTION: return "ILLEGAL_INSTRUCTION";
        case EXCEPTION_IN_PAGE_ERROR: return "IN_PAGE_ERROR";
        default: return "UNHANDLED_EXCEPTION";
    }
}
LONG WINAPI crash_filter(EXCEPTION_POINTERS *exception) {
    if (InterlockedExchange(&reporting, 1)) return EXCEPTION_CONTINUE_SEARCH;
    SYSTEMTIME time{}; GetLocalTime(&time);
    wchar_t path[32768];
    swprintf_s(path, L"%s\\crash_%04u-%02u-%02u_%02u-%02u-%02u_%lu.log", log_directory,
        time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute, time.wSecond, GetCurrentProcessId());
    HANDLE file = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    const auto &record = *exception->ExceptionRecord;
    write_text(file, "========== NATIVE CRASH ==========\nException: 0x%08lX %s\nThread: %lu (%s)\nAddress: %p\n",
        record.ExceptionCode, exception_name(record.ExceptionCode), GetCurrentThreadId(),
        GetCurrentThreadId() == main_thread ? "Main Thread" : "Worker Thread", record.ExceptionAddress);
    if ((record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION || record.ExceptionCode == EXCEPTION_IN_PAGE_ERROR) && record.NumberParameters >= 2)
        write_text(file, "Attempted to %s address 0x%llX\n", record.ExceptionInformation[0] == 0 ? "read" : record.ExceptionInformation[0] == 1 ? "write" : "execute", record.ExceptionInformation[1]);
    write_text(file, "\nRecent logs (up to 200, each limited to 2047 UTF-8 bytes):\n");
    if (recent_mutex.try_lock()) {
        unsigned start = (recent_next + 200 - recent_count) % 200;
        for (unsigned i = 0; i < recent_count; ++i) write_text(file, "%s\n", recent[(start + i) % 200]);
        recent_mutex.unlock();
    } else write_text(file, "Recent log buffer was busy.\n");
    // Flush basic evidence before attempting best-effort symbolization/unwinding.
    if (file != INVALID_HANDLE_VALUE) FlushFileBuffers(file);
    bool locked = symbol_mutex.try_lock();
    bool resolve = locked && symbols;
    char frame_text[4096];
    write_text(file, "\nStack from exception CPU context:\n");
    format_frame(reinterpret_cast<DWORD64>(record.ExceptionAddress), frame_text, sizeof(frame_text), resolve);
    write_text(file, "#0 %s\n", frame_text);
    CONTEXT context = *exception->ContextRecord;
    STACKFRAME64 frame{};
    DWORD machine = 0;
#if defined(_M_X64) || defined(__x86_64__)
    machine = IMAGE_FILE_MACHINE_AMD64;
    frame.AddrPC.Offset = context.Rip; frame.AddrStack.Offset = context.Rsp; frame.AddrFrame.Offset = context.Rbp;
#elif defined(_M_IX86) || defined(__i386__)
    machine = IMAGE_FILE_MACHINE_I386;
    frame.AddrPC.Offset = context.Eip; frame.AddrStack.Offset = context.Esp; frame.AddrFrame.Offset = context.Ebp;
#elif defined(_M_ARM64)
    machine = IMAGE_FILE_MACHINE_ARM64;
    frame.AddrPC.Offset = context.Pc; frame.AddrStack.Offset = context.Sp; frame.AddrFrame.Offset = context.Fp;
#endif
    frame.AddrPC.Mode = frame.AddrStack.Mode = frame.AddrFrame.Mode = AddrModeFlat;
    DWORD64 last_pc = 0, last_sp = 0;
    int index = 1;
    for (int depth = 0; resolve && machine && depth < 64; ++depth) {
        if (!StackWalk64(machine, symbols, GetCurrentThread(), &frame, &context, read_memory, SymFunctionTableAccess64, SymGetModuleBase64, nullptr) || !frame.AddrPC.Offset) break;
        if (frame.AddrPC.Offset == last_pc && frame.AddrStack.Offset == last_sp) break;
        last_pc = frame.AddrPC.Offset; last_sp = frame.AddrStack.Offset;
        if (depth == 0 && frame.AddrPC.Offset == reinterpret_cast<DWORD64>(record.ExceptionAddress)) continue;
        format_frame(frame.AddrPC.Offset, frame_text, sizeof(frame_text), true);
        write_text(file, "#%d %s\n", index++, frame_text);
    }
    if (!resolve) write_text(file, "Symbols busy/unavailable; exception address retained.\n");
    if (locked) symbol_mutex.unlock();
    if (file != INVALID_HANDLE_VALUE) { FlushFileBuffers(file); CloseHandle(file); }
    // Never resume a faulting instruction or call Godot/Log from the crash context.
    return previous_filter ? previous_filter(exception) : EXCEPTION_CONTINUE_SEARCH;
}
}
void CrashHandler::initialize() {
    auto directory = godot::ProjectSettings::get_singleton()->globalize_path("user://logs");
    godot::DirAccess::make_dir_recursive_absolute(directory);
    auto utf = directory.utf16();
    wcsncpy_s(log_directory, reinterpret_cast<const wchar_t *>(utf.get_data()), _TRUNCATE);
    main_thread = GetCurrentThreadId();
    reporting = 0;
    {
        std::lock_guard lock(symbol_mutex);
        // A real, unique process handle lets StackWalk and module APIs coexist safely.
        symbols = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());
        auto search = godot::ProjectSettings::get_singleton()->globalize_path("res://bin") + ";" + godot::ProjectSettings::get_singleton()->globalize_path("res://.godot/binding-build");
        SymSetOptions(SymGetOptions() | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS | SYMOPT_FAIL_CRITICAL_ERRORS | SYMOPT_NO_PROMPTS);
        if (symbols && SymInitialize(symbols, search.utf8().get_data(), FALSE)) load_modules();
        else { if (symbols) CloseHandle(symbols); symbols = nullptr; }
    }
    recent_connection = Log::subscribe([](const LogRecord &record) {
        auto tag = record.tag.utf8(), text = record.text.utf8();
        std::lock_guard lock(recent_mutex);
        snprintf(recent[recent_next], sizeof(recent[0]), "%.3f [%d] [%s] %s", record.timestamp, record.level, tag.get_data(), text.get_data());
        auto *line = recent[recent_next];
        size_t end = strlen(line), start = end;
        while (start && (static_cast<unsigned char>(line[start - 1]) & 0xc0) == 0x80) --start;
        if (start && static_cast<unsigned char>(line[start - 1]) >= 0xc0) {
            unsigned char lead = static_cast<unsigned char>(line[start - 1]);
            size_t expected = lead < 0xe0 ? 2 : lead < 0xf0 ? 3 : 4;
            if (end - (start - 1) < expected) line[start - 1] = 0;
        }
        recent_next = (recent_next + 1) % 200;
        recent_count = (std::min)(recent_count + 1, 200u);
    });
    previous_filter = SetUnhandledExceptionFilter(crash_filter);
}
void CrashHandler::shutdown() {
    // Restore the predecessor before unloading any function pointer in this DLL.
    auto current = SetUnhandledExceptionFilter(previous_filter);
    if (current != crash_filter) SetUnhandledExceptionFilter(current);
    recent_connection.disconnect();
    std::lock_guard lock(symbol_mutex);
    if (symbols) { SymCleanup(symbols); CloseHandle(symbols); symbols = nullptr; }
}
namespace detail {
godot::PackedStringArray capture_log_stack() {
    godot::PackedStringArray result;
    std::lock_guard lock(symbol_mutex);
    void *frames[24]; auto count = CaptureStackBackTrace(2, 24, frames, nullptr);
    for (USHORT i = 0; i < count; ++i) {
        char text[4096];
        format_frame(reinterpret_cast<DWORD64>(frames[i]), text, sizeof(text), symbols != nullptr);
        result.push_back(godot::String::utf8(text));
    }
    return result;
}
}
}
