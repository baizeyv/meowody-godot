#pragma once
#include <filesystem>
#include <string>
#include <vector>

namespace creator {
struct Engine {
    std::string path, version, api;
};
struct Job {
    std::string operation, cache, engine, current_engine, version, ref, destination, source, commit, pin, expected_version;
    bool local = true;
};
struct Reply {
    bool ok = false;
    std::string message, text, extra, source, commit;
    Engine engine;
    std::vector<Engine> engines;
};
std::string utf8(const std::filesystem::path &path);
bool empty_destination(const std::string &path);
bool copy_source(const std::filesystem::path &source, const std::filesystem::path &destination, std::string &error);
Reply execute(const Job &job, const std::string &log_path);
}
