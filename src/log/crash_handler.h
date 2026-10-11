#pragma once
namespace meowody {
// Windows debug builds. Installed/uninstalled with the GDExtension lifetime.
class CrashHandler {
public:
    static void initialize();
    static void shutdown();
};
}
