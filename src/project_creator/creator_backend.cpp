#include "creator_backend.h"
#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <cstdint>
#include <cwctype>
#include <regex>
#include <set>
#include <sstream>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <bcrypt.h>
#endif

namespace creator {
namespace fs = std::filesystem;
std::string utf8(const fs::path &path) {
    auto value = path.generic_u8string();
    return {reinterpret_cast<const char *>(value.data()), value.size()};
}
bool empty_destination(const std::string &value) {
    if (value.empty()) return false;
    auto path = fs::u8path(value);
    std::error_code ec;
    bool exists = fs::exists(path, ec);
    if (ec) return false;
    return !exists || (fs::is_directory(path, ec) && fs::is_empty(path, ec) && !ec);
}
bool copy_source(const fs::path &source, const fs::path &destination, std::string &error) {
    static const std::set<std::string> skip_names = {".git", ".godot", "__pycache__", "node_modules", "dist"};
    static const std::set<std::string> skip_extensions = {".pyc", ".obj", ".o", ".os", ".dll", ".lib", ".exp", ".pdb", ".so", ".a", ".dylib", ".exe", ".bak", ".TMP"};
    std::error_code ec;
    fs::create_directories(destination, ec);
    if (!ec && !fs::is_directory(source, ec)) error = "Missing framework directory: " + utf8(source);
    if (ec || !error.empty()) { if (error.empty()) error = ec.message(); return false; }
    fs::recursive_directory_iterator iterator(source, ec), end;
    while (!ec && iterator != end) {
        const auto &entry = *iterator;
        auto path = entry.path();
        if (skip_names.count(utf8(path.filename())) || fs::is_symlink(entry.symlink_status(ec))) {
            if (entry.is_directory(ec)) iterator.disable_recursion_pending();
        } else if (!skip_extensions.count(utf8(path.extension()))) {
            auto target = destination / path.lexically_relative(source);
            if (entry.is_directory(ec)) fs::create_directories(target, ec);
            else if (entry.is_regular_file(ec)) fs::copy_file(path, target, fs::copy_options::overwrite_existing, ec);
        }
        iterator.increment(ec);
    }
    if (ec) error = ec.message();
    return !ec;
}

namespace {
using Log = std::function<void(const std::string &)>;
Reply fail(const std::string &message) { Reply reply; reply.message = message; return reply; }
std::string read(const fs::path &path) {
    std::ifstream stream(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}
bool write(const fs::path &path, const std::string &text) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
    return bool(stream);
}
bool mkdir(const fs::path &path, std::string &error) {
    std::error_code ec;
    fs::create_directories(path, ec);
    if (ec) error = ec.message();
    return !ec;
}
std::string trim(std::string text) {
    auto first = text.find_first_not_of(" \r\n\t");
    return first == std::string::npos ? "" : text.substr(first, text.find_last_not_of(" \r\n\t") - first + 1);
}
std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}
bool version(const std::string &text, Engine &engine) {
    std::smatch match;
    if (!std::regex_match(text, match, std::regex(R"(4\.(\d+)(\.\d+)?\.stable\.official\.[0-9a-f]+)"))) return false;
    engine.version = text;
    engine.api = "4." + match[1].str();
    return true;
}
std::vector<FrameworkRef> framework_refs(const std::string &output) {
    std::vector<FrameworkRef> result;
    std::set<std::string> seen;
    std::istringstream input(output);
    std::string hash, ref;
    while (input >> hash >> ref) {
        if (!std::regex_match(hash, std::regex("[0-9a-f]{40}")) || ref.ends_with("^{}") || !seen.insert(ref).second) continue;
        if (ref.starts_with("refs/heads/")) result.push_back({ref.substr(11), ref, "branch"});
        else if (ref.starts_with("refs/tags/")) result.push_back({ref.substr(10), ref, "tag"});
    }
    std::sort(result.begin(), result.end(), [](const FrameworkRef &a, const FrameworkRef &b) {
        if (a.kind != b.kind) return a.kind == "branch";
        if (a.kind == "branch" && (a.name == "main" || b.name == "main")) return a.name == "main" && b.name != "main";
        return a.kind == "tag" ? a.name > b.name : a.name < b.name;
    });
    return result;
}
bool choose_dependency(const std::string &refs, const std::string &engine_version, const std::string &api,
        const std::string &fallback_tag, const std::string &fallback_commit, Reply &reply, bool legacy_record = false) {
    std::map<std::string, std::string> tags, peeled;
    std::istringstream input(refs);
    std::string commit, ref;
    while (input >> commit >> ref) {
        if (!ref.starts_with("refs/tags/") || !std::regex_match(commit, std::regex("[0-9a-f]{40}"))) continue;
        auto tag = ref.substr(10);
        if (tag.ends_with("^{}")) peeled[tag.substr(0, tag.size() - 3)] = commit;
        else tags[tag] = commit;
    }
    for (const auto &[tag, hash] : peeled) tags[tag] = hash;
    auto end = engine_version.find(".stable.");
    if (end == std::string::npos) return false;
    std::string exact = "godot-" + engine_version.substr(0, end) + "-stable", minor = "godot-" + api + "-stable";
    for (const auto &candidate : {exact, minor}) if (tags.count(candidate)) {
        reply.cpp_tag = candidate; reply.cpp_commit = tags[candidate]; return true;
    }
    if (!fallback_tag.empty() && tags.count(fallback_tag) && tags[fallback_tag] == fallback_commit) {
        reply.cpp_tag = fallback_tag; reply.cpp_commit = fallback_commit; return true;
    }
    // Earlier framework revisions pinned this release before recording its tag/API range.
    // Recover only the verified v10.0.0 release and the APIs tested by this creator.
    if (legacy_record && fallback_tag.empty() && (api == "4.3" || api == "4.4" || api == "4.5" || api == "4.6" || api == "4.7") &&
            tags.count("10.0.0-stable") && tags["10.0.0-stable"] == fallback_commit) {
        reply.cpp_tag = "10.0.0-stable"; reply.cpp_commit = fallback_commit; return true;
    }
    return false;
}
#ifdef _WIN32
std::wstring wide(const std::string &text) { return fs::u8path(text).wstring(); }
std::string environment(const wchar_t *key) {
    DWORD size = GetEnvironmentVariableW(key, nullptr, 0);
    if (!size) return {};
    std::wstring value(size, L'\0');
    value.resize(GetEnvironmentVariableW(key, value.data(), size));
    return utf8(fs::path(value));
}
fs::path find_program(const wchar_t *name) {
    std::array<wchar_t, 32768> path{};
    DWORD size = SearchPathW(nullptr, name, L".exe", DWORD(path.size()), path.data(), nullptr);
    return size && size < path.size() ? fs::path(path.data()) : fs::path();
}
std::wstring quote(const std::wstring &text) {
    std::wstring result = L"\"";
    size_t slashes = 0;
    for (wchar_t character : text) {
        if (character == L'\\') { ++slashes; continue; }
        result.append(slashes * (character == L'"' ? 2 : 1), L'\\');
        if (character == L'"') result += L'\\';
        result += character;
        slashes = 0;
    }
    result.append(slashes * 2, L'\\');
    return result + L'"';
}
struct Process { bool ok = false; std::string output; };
Process run(const std::vector<std::wstring> &arguments, const fs::path &cwd, const Log &log,
        DWORD timeout = INFINITE, const std::map<std::wstring, std::wstring> &overrides = {}) {
    Process result;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    HANDLE reader = nullptr, writer = nullptr;
    if (!CreatePipe(&reader, &writer, &security, 0)) return result;
    SetHandleInformation(reader, HANDLE_FLAG_INHERIT, 0);
    HANDLE input = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, &security, OPEN_EXISTING, 0, nullptr);
    std::wstring command;
    for (const auto &argument : arguments) { if (!command.empty()) command += L' '; command += quote(argument); }
    std::map<std::wstring, std::wstring> variables;
    LPWCH original = GetEnvironmentStringsW();
    if (original) {
        for (auto *entry = original; *entry; entry += wcslen(entry) + 1) {
            std::wstring value(entry);
            auto separator = value.find(L'=', 1);
            if (separator != std::wstring::npos) variables[value.substr(0, separator)] = value.substr(separator + 1);
        }
        FreeEnvironmentStringsW(original);
    }
    variables[L"GIT_TERMINAL_PROMPT"] = L"0";
    for (const auto &[key, value] : overrides) {
        for (auto iterator = variables.begin(); iterator != variables.end();) {
            if (_wcsicmp(iterator->first.c_str(), key.c_str()) == 0) iterator = variables.erase(iterator);
            else ++iterator;
        }
        variables[key] = value;
    }
    std::wstring block;
    for (const auto &[key, value] : variables) { block += key + L"=" + value; block += L'\0'; }
    block += L'\0';
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdOutput = startup.hStdError = writer;
    startup.hStdInput = input;
    PROCESS_INFORMATION process{};
    BOOL started = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT, block.data(), cwd.empty() ? nullptr : cwd.c_str(), &startup, &process);
    CloseHandle(writer);
    if (input != INVALID_HANDLE_VALUE) CloseHandle(input);
    if (!started) { CloseHandle(reader); return result; }
    ULONGLONG deadline = timeout == INFINITE ? 0 : GetTickCount64() + timeout;
    std::array<char, 8192> buffer{};
    bool timed_out = false;
    for (;;) {
        if (deadline && GetTickCount64() >= deadline) { timed_out = true; TerminateProcess(process.hProcess, 1); break; }
        DWORD available = 0, count = 0;
        if (!PeekNamedPipe(reader, nullptr, 0, nullptr, &available, nullptr)) break;
        if (available) {
            if (!ReadFile(reader, buffer.data(), DWORD(std::min<size_t>(buffer.size(), available)), &count, nullptr)) break;
            std::string chunk(buffer.data(), count);
            result.output += chunk;
            if (log) log(chunk);
        } else {
            if (WaitForSingleObject(process.hProcess, 20) == WAIT_OBJECT_0) {
                DWORD remaining = 0;
                if (PeekNamedPipe(reader, nullptr, 0, nullptr, &remaining, nullptr) && remaining) continue;
                break;
            }
            if (deadline && GetTickCount64() >= deadline) { timed_out = true; TerminateProcess(process.hProcess, 1); break; }
        }
    }
    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(process.hProcess, &code);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    CloseHandle(reader);
    result.ok = !timed_out && code == 0;
    return result;
}
struct InternetCloser { void operator()(void *handle) const { if (handle) WinHttpCloseHandle(handle); } };
using Internet = std::unique_ptr<void, InternetCloser>;
bool http(const std::wstring &url, std::string &body, std::string &error, const fs::path &file = {}) {
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) { error = "Invalid HTTPS address."; return false; }
    Internet session(WinHttpOpen(L"MeowodyProjectCreator", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0));
    if (!session) { error = "Cannot initialize HTTPS."; return false; }
    WinHttpSetTimeouts(session.get(), 20000, 20000, 20000, 30000);
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength), resource(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength) resource.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    Internet connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    Internet request(connection ? WinHttpOpenRequest(connection.get(), L"GET", resource.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr);
    if (!request || !WinHttpSendRequest(request.get(), L"Accept: application/vnd.github+json\r\n", DWORD(-1), nullptr, 0, 0, 0) || !WinHttpReceiveResponse(request.get(), nullptr)) {
        error = "HTTPS request failed (Windows error " + std::to_string(GetLastError()) + "). Check the network and proxy."; return false;
    }
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr, &status, &size, nullptr);
    if (status != 200) { error = "Official download server returned HTTP " + std::to_string(status) + "."; return false; }
    std::ofstream output;
    if (!file.empty()) { output.open(file, std::ios::binary | std::ios::trunc); if (!output) { error = "Cannot write download."; return false; } }
    std::array<char, 65536> bytes{};
    for (;;) {
        DWORD count = 0;
        if (!WinHttpReadData(request.get(), bytes.data(), DWORD(bytes.size()), &count)) { error = "Download interrupted."; return false; }
        if (!count) break;
        if (file.empty()) body.append(bytes.data(), count);
        else { output.write(bytes.data(), count); if (!output) { error = "Cannot write download."; return false; } }
    }
    return true;
}
bool inspect(const fs::path &path, Engine &engine) {
    std::ifstream file(path, std::ios::binary);
    std::array<unsigned char, 64> header{};
    if (!file.read(reinterpret_cast<char *>(header.data()), header.size()) || header[0] != 'M' || header[1] != 'Z') return false;
    uint32_t offset = uint32_t(header[60]) | uint32_t(header[61]) << 8 | uint32_t(header[62]) << 16 | uint32_t(header[63]) << 24;
    file.seekg(offset);
    std::array<char, 6> pe{};
    if (!file.read(pe.data(), pe.size()) || std::string(pe.data(), pe.size()) != std::string("PE\0\0\x64\x86", 6)) return false;
    auto result = run({path.wstring(), L"--version"}, path.parent_path(), {}, 5000);
    if (!result.ok || !version(trim(result.output), engine)) return false;
    auto help = run({path.wstring(), L"--help"}, path.parent_path(), {}, 5000);
    if (!help.ok || help.output.find("--editor") == std::string::npos || help.output.find("--dump-extension-api") == std::string::npos) return false;
    engine.path = utf8(path);
    return true;
}
std::string sha512(const fs::path &path) {
    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA512_ALGORITHM, nullptr, 0) < 0) return {};
    DWORD object_size = 0, count = 0;
    BCryptGetProperty(algorithm, BCRYPT_OBJECT_LENGTH, reinterpret_cast<PUCHAR>(&object_size), sizeof(object_size), &count, 0);
    std::vector<unsigned char> object(object_size);
    std::array<unsigned char, 64> digest{};
    bool ok = BCryptCreateHash(algorithm, &hash, object.data(), object_size, nullptr, 0, 0) >= 0;
    std::ifstream input(path, std::ios::binary);
    std::array<char, 65536> bytes{};
    if (!input) ok = false;
    while (ok && input) {
        input.read(bytes.data(), bytes.size());
        auto amount = input.gcount();
        if (amount && BCryptHashData(hash, reinterpret_cast<PUCHAR>(bytes.data()), ULONG(amount), 0) < 0) ok = false;
    }
    if (input.bad()) ok = false;
    if (ok) ok = BCryptFinishHash(hash, digest.data(), ULONG(digest.size()), 0) >= 0;
    if (hash) BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(algorithm, 0);
    if (!ok) return {};
    std::string result;
    for (auto byte : digest) { result += "0123456789abcdef"[byte >> 4]; result += "0123456789abcdef"[byte & 15]; }
    return result;
}
bool download(const Job &job, Engine &engine, std::string &error, const Log &log) {
    if (!std::regex_match(job.version, std::regex(R"(4\.\d+(\.\d+)?-stable)"))) { error = "Choose an official stable version."; return false; }
    fs::path directory = fs::u8path(job.cache) / "engines" / fs::u8path(job.version);
    std::string filename = "Godot_v" + job.version + "_win64.exe.zip";
    auto executable = directory / fs::u8path(filename.substr(0, filename.size() - 4));
    if (fs::exists(directory / "verified.sha512") && inspect(executable, engine)) return true;
    if (!mkdir(directory, error)) return false;
    auto archive = directory / "download.zip";
    std::wstring url = L"https://github.com/godotengine/godot-builds/releases/download/" + wide(job.version) + L"/";
    std::string ignored, checksums;
    log("Downloading official Godot " + job.version + "...\n");
    if (!http(url + wide(filename), ignored, error, archive) || !http(url + L"SHA512-SUMS.txt", checksums, error)) return false;
    std::istringstream lines(checksums);
    std::string expected, line;
    while (std::getline(lines, line)) {
        std::istringstream fields(line);
        std::string checksum, name;
        fields >> checksum >> name;
        if (!name.empty() && name.front() == '*') name.erase(0, 1);
        if (name == filename) expected = lower(checksum);
    }
    std::string actual = sha512(archive);
    if (expected.size() != 128 || actual != expected) { error = "Official SHA512 checksum validation failed."; return false; }
    auto listing = run({L"tar.exe", L"-tf", archive.wstring()}, directory, {}, 30000);
    if (!listing.ok) { error = "Cannot inspect the engine ZIP. Windows tar.exe is required."; return false; }
    std::istringstream entries(listing.output);
    while (std::getline(entries, line)) {
        auto member = fs::u8path(trim(line));
        if (member.has_root_path() || utf8(member).find(':') != std::string::npos) { error = "Unsafe engine archive path."; return false; }
        for (const auto &part : member) if (part == "..") { error = "Unsafe engine archive path."; return false; }
    }
    if (!run({L"tar.exe", L"-xf", archive.wstring(), L"-C", directory.wstring()}, directory, log, 120000).ok || !inspect(executable, engine)) {
        error = "Cannot extract a compatible Godot editor."; return false;
    }
    if (!write(directory / "verified.sha512", expected)) { error = "Cannot write engine cache marker."; return false; }
    std::error_code ec;
    fs::remove(archive, ec);
    return true;
}
Reply discover(const Job &job, const Log &log) {
    std::vector<fs::path> candidates, roots;
    for (const auto &seed : {job.engine, job.current_engine}) if (!seed.empty()) candidates.push_back(fs::u8path(seed));
    std::wstring path = wide(environment(L"PATH"));
    std::wistringstream paths(path);
    std::wstring directory;
    while (std::getline(paths, directory, L';')) {
        std::error_code ec;
        for (fs::directory_iterator iterator(directory, ec), end; !ec && iterator != end; iterator.increment(ec)) {
            auto name = lower(utf8(iterator->path().filename()));
            if (name.find("godot") != std::string::npos && iterator->path().extension() == L".exe") candidates.push_back(iterator->path());
        }
    }
    for (const wchar_t *key : {L"ProgramFiles", L"ProgramFiles(x86)"}) { auto value = environment(key); if (!value.empty()) roots.push_back(fs::u8path(value)); }
    auto profile = fs::u8path(environment(L"USERPROFILE"));
    for (const auto *name : {"Downloads", "Desktop", "scoop/apps/godot"}) roots.push_back(profile / name);
    roots.push_back(fs::u8path(environment(L"LOCALAPPDATA")) / "Programs");
    roots.push_back(fs::u8path(job.cache) / "engines");
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) if (drives & (1u << i)) {
        std::wstring drive = std::wstring(1, wchar_t(L'A' + i)) + L":\\";
        if (GetDriveTypeW(drive.c_str()) != DRIVE_FIXED) continue;
        std::error_code ec;
        for (fs::directory_iterator iterator(drive, ec), end; !ec && iterator != end; iterator.increment(ec)) {
            auto name = lower(utf8(iterator->path().filename()));
            for (const auto *word : {"godot", "tool", "develop", "engine", "program", "software", "app", "工具", "软件", "开发"})
                if (name.find(word) != std::string::npos) { roots.push_back(iterator->path()); break; }
        }
    }
    std::vector<std::pair<fs::path, int>> pending;
    for (const auto &root : roots) pending.emplace_back(root, 0);
    std::set<std::wstring> visited;
    while (!pending.empty()) {
        auto [folder, depth] = pending.back(); pending.pop_back();
        auto key = folder.lexically_normal().wstring();
        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
        DWORD attributes = GetFileAttributesW(folder.c_str());
        if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & FILE_ATTRIBUTE_REPARSE_POINT) || !visited.insert(key).second) continue;
        std::error_code ec;
        for (fs::directory_iterator iterator(folder, ec), end; !ec && iterator != end; iterator.increment(ec)) {
            auto name = lower(utf8(iterator->path().filename()));
            if (iterator->is_regular_file(ec) && name.find("godot") != std::string::npos && iterator->path().extension() == L".exe") candidates.push_back(iterator->path());
            else if (depth < 3 && iterator->is_directory(ec) && name != ".git" && name != ".godot" && name != "node_modules" && name != "cache" && name != "temp" && name != "windows") pending.emplace_back(iterator->path(), depth + 1);
        }
    }
    Reply reply; reply.ok = true;
    visited.clear();
    for (const auto &candidate : candidates) {
        auto key = candidate.lexically_normal().wstring();
        std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return wchar_t(towlower(c)); });
        if (!visited.insert(key).second || lower(utf8(candidate.stem())).ends_with("_console")) continue;
        Engine engine;
        if (inspect(candidate, engine)) { log("Found Godot " + engine.version + " — " + engine.path + "\n"); reply.engines.push_back(std::move(engine)); }
    }
    std::sort(reply.engines.begin(), reply.engines.end(), [](const Engine &a, const Engine &b) {
        auto numbers = [](const std::string &value) { std::istringstream input(value); int major = 0, minor = 0, patch = 0; char dot; input >> major >> dot >> minor >> dot >> patch; return std::array<int, 3>{major, minor, patch}; };
        return numbers(a.version) == numbers(b.version) ? a.path < b.path : numbers(a.version) > numbers(b.version);
    });
    return reply;
}
bool preflight(std::string &error, const Log &log) {
    for (const auto *name : {L"git", L"python", L"scons"}) if (find_program(name).empty()) {
        error = "Install Git, Python and SCons for framework compilation and add them to PATH."; return false;
    }
    auto vswhere = fs::u8path(environment(L"ProgramFiles(x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe";
    if (find_program(L"cl").empty()) {
        auto found = run({vswhere.wstring(), L"-latest", L"-products", L"*", L"-requires", L"Microsoft.VisualStudio.Component.VC.Tools.x86.x64", L"-property", L"installationPath"}, {}, {}, 10000);
        if (!found.ok || trim(found.output).empty()) { error = "Install Visual Studio C++ build tools and a Windows SDK."; return false; }
    }
    log("Git / SCons / Visual Studio toolchain found. SCons will validate the reflection compiler dependencies.\n");
    return true;
}
bool checkout(const std::wstring &url, const std::string &ref, const fs::path &destination, const Log &log, std::string &commit) {
    if (ref.empty() || ref.front() == '-') return false;
    for (auto arguments : {std::vector<std::wstring>{L"git", L"init", destination.wstring()},
            {L"git", L"-C", destination.wstring(), L"remote", L"add", L"origin", url},
            {L"git", L"-C", destination.wstring(), L"fetch", L"--depth", L"1", L"origin", wide(ref)},
            {L"git", L"-C", destination.wstring(), L"checkout", L"--detach", L"FETCH_HEAD"}})
        if (!run(arguments, destination.parent_path(), log).ok) return false;
    auto result = run({L"git", L"rev-parse", L"HEAD"}, destination, {});
    commit = trim(result.output);
    return result.ok;
}
Reply build(const Job &job, const Log &log) {
    fs::path destination = fs::u8path(job.destination), engine = fs::u8path(job.engine);
    std::string error;
    if (!preflight(error, log)) return fail(error);
    Engine actual;
    if (!inspect(engine, actual) || actual.version != job.expected_version) return fail("Use the official Godot version recorded when the project was created.");
    if (job.operation == "build") {
        auto gitlink = run({L"git", L"ls-files", L"--stage", L"godot-cpp"}, fs::u8path(job.source), {});
        std::string framework_pin = job.framework_pin.empty() ? job.pin : job.framework_pin;
        if (!gitlink.ok || (gitlink.output.starts_with("160000 ") && !gitlink.output.starts_with("160000 " + framework_pin + " "))) return fail("Framework submodule commit differs from its dependency record.");
        if (!run({L"git", L"init"}, destination, log).ok) return fail("Cannot initialize the project Git repository.");
        std::string ignored;
        if (!checkout(L"https://github.com/godotengine/godot-cpp.git", job.pin, destination / "godot-cpp", log, ignored)) return fail("Dependency checkout failed; output and logs were retained.");
        for (auto arguments : {std::vector<std::wstring>{L"git", L"submodule", L"add", L"--force", L"https://github.com/godotengine/godot-cpp.git", L"godot-cpp"},
                {L"git", L"submodule", L"absorbgitdirs", L"godot-cpp"}, {L"git", L"submodule", L"update", L"--init", L"--recursive"}})
            if (!run(arguments, destination, log).ok) return fail("Cannot register the pinned godot-cpp submodule.");
    }
    auto gitlink = run({L"git", L"ls-files", L"--stage", L"godot-cpp"}, destination, {});
    if (!gitlink.ok || !gitlink.output.starts_with("160000 " + job.pin + " ") || !fs::is_regular_file(destination / ".gitmodules") || !fs::is_regular_file(destination / "godot-cpp/SConstruct")) return fail("The pinned godot-cpp submodule is incomplete. Restore it before rebuilding.");
    std::map<std::wstring, std::wstring> env;
    for (const auto *folder : {"profile", "temp"}) {
        auto path = destination / "build/creator" / folder;
        if (!mkdir(path, error) || !write(path / ".gdignore", "")) return fail("Cannot isolate the Godot profile.");
        if (std::string(folder) == "profile") env[L"APPDATA"] = env[L"LOCALAPPDATA"] = path.wstring();
        else env[L"TEMP"] = env[L"TMP"] = path.wstring();
    }
    auto api = destination / "godot-api";
    if (!mkdir(api, error)) return fail(error);
    if (!run({engine.wstring(), L"--headless", L"--dump-extension-api"}, api, log, INFINITE, env).ok) return fail("Godot API export failed.");
    if (!std::regex_search(read(api / "extension_api.json"), std::regex(R"("precision"\s*:\s*"single")"))) return fail("Only single-precision Godot editors are supported.");
    log("Building Debug extension; this can take several minutes.\n");
    if (!run({L"scons", L"platform=windows", L"arch=x86_64", L"target=template_debug"}, destination, log).ok) return fail("Framework compilation failed; see the build log.");
    auto project = destination / "project";
    auto imported = run({engine.wstring(), L"--headless", L"--editor", L"--path", project.wstring(), L"--import"}, destination, log, INFINITE, env);
    if (!imported.ok || imported.output.find("ERROR:") != std::string::npos) return fail("Godot import reported errors.");
    auto probe = project / "creator_verify.gd";
    if (!write(probe, "extends SceneTree\nfunc _initialize():\n    for native_class in [\"SpriteAtlas\", \"MultiPatchRect\", \"VirtualList\", \"ArcProgress\"]:\n        if not ClassDB.class_exists(native_class):\n            push_error(\"Missing framework class: \" + native_class)\n            quit(1)\n            return\n    print(\"[Creator] framework loaded\")\n    quit(0)\n")) return fail("Cannot write runtime validation script.");
    auto verified = run({engine.wstring(), L"--headless", L"--path", project.wstring(), L"--script", L"res://creator_verify.gd"}, destination, log, INFINITE, env);
    std::error_code ec;
    fs::remove(probe, ec);
    fs::remove(project / "creator_verify.gd.uid", ec);
    if (!verified.ok || verified.output.find("[Creator] framework loaded") == std::string::npos || verified.output.find("ERROR:") != std::string::npos) return fail("Framework runtime validation failed.");
    auto scene = run({engine.wstring(), L"--headless", L"--path", project.wstring(), L"--quit-after", L"3"}, destination, log, INFINITE, env);
    if (!scene.ok || scene.output.find("ERROR:") != std::string::npos) return fail("Empty scene validation failed.");
    log("Project creation and validation completed.\n");
    Reply reply; reply.ok = true; reply.engine = actual; return reply;
}
#endif
}

Reply execute(const Job &job, const std::string &log_path) {
#ifndef _WIN32
    return fail("Project creation currently supports Windows x64.");
#else
    std::ofstream output(fs::u8path(log_path), std::ios::binary | std::ios::app);
    std::ofstream project_log;
    if (job.operation == "build" || job.operation == "rebuild") {
        std::string error;
        auto folder = fs::u8path(job.destination) / "build/creator";
        if (!mkdir(folder, error)) return fail(error);
        project_log.open(folder / (job.operation == "build" ? "create.log" : "rebuild.log"), std::ios::binary | std::ios::app);
    }
    Log log = [&](const std::string &text) { output << text; output.flush(); if (project_log) { project_log << text; project_log.flush(); } };
    if (!output) return fail("Cannot open the task log.");
    if (job.operation == "discover") return discover(job, log);
    if (job.operation == "inspect") {
        Reply reply;
        if (!inspect(fs::u8path(job.engine), reply.engine)) return fail("Choose an official stable Windows x64 Godot 4 editor.");
        reply.ok = true; reply.engines.push_back(reply.engine); return reply;
    }
    if (job.operation == "versions") {
        Reply reply;
        std::string error;
        if (!http(L"https://api.github.com/repos/godotengine/godot-builds/releases/latest", reply.text, error) ||
                !http(L"https://api.github.com/repos/godotengine/godot-builds/releases?per_page=100", reply.extra, error)) return fail(error);
        reply.ok = true; return reply;
    }
    if (job.operation == "refs") {
        auto result = run({L"git", L"ls-remote", L"--heads", L"--tags", L"--refs", L"https://github.com/baizeyv/meowody-godot.git"}, {}, {}, 30000);
        if (!result.ok) return fail("Cannot query framework branches and tags. Check Git, network and repository access.");
        Reply reply; reply.refs = framework_refs(result.output);
        if (reply.refs.empty()) return fail("The framework repository has no available branches or tags.");
        reply.ok = true; return reply;
    }
    if (job.operation == "preflight") {
        Reply reply; reply.ok = preflight(reply.message, log); return reply;
    }
    if (job.operation == "resolve") {
        Reply reply;
        log("Resolving godot-cpp for " + job.expected_version + "; framework pin " + job.framework_pin +
                (job.legacy_dependency_record ? " (legacy dependency record)\n" : "\n"));
        auto refs = run({L"git", L"ls-remote", L"--tags", L"https://github.com/godotengine/godot-cpp.git"}, {}, {}, 30000);
        Engine engine;
        if (!refs.ok) return fail("Cannot query official godot-cpp releases. Check Git and network access, then retry.");
        if (!version(job.expected_version, engine)) return fail("Cannot recognize the selected official Godot version: " + job.expected_version);
        if (!choose_dependency(refs.output, engine.version, engine.api, job.cpp_tag, job.framework_pin, reply, job.legacy_dependency_record))
            return fail("No verified godot-cpp mapping for Godot " + engine.version + " (API " + engine.api + "). The framework dependency record needs a supported tag/API mapping.");
        log("Matched Godot " + engine.version + " -> godot-cpp " + reply.cpp_tag + " @ " + reply.cpp_commit + "\n");
        reply.ok = true; return reply;
    }
    if (job.operation == "prepare") {
        if (!empty_destination(job.destination)) return fail("Destination must be a new or empty directory; nothing will be overwritten.");
        if (job.ref.empty() || job.ref.front() == '-') return fail("Enter a framework branch, tag or commit.");
        Reply reply;
        if (!preflight(reply.message, log)) return reply;
        if (job.local) { if (!inspect(fs::u8path(job.engine), reply.engine)) return fail("Choose an official stable Windows x64 Godot 4 editor."); }
        else if (!download(job, reply.engine, reply.message, log)) return reply;
        auto source = fs::u8path(job.cache) / ("framework-" + std::to_string(GetCurrentProcessId()) + "-" + std::to_string(GetTickCount64()));
        if (!checkout(L"https://github.com/baizeyv/meowody-godot.git", job.ref, source, log, reply.commit)) return fail("Framework checkout failed. See the task log.");
        reply.source = utf8(source); reply.ok = true; return reply;
    }
    if (job.operation == "build" || job.operation == "rebuild") return build(job, log);
    return fail("Unknown creator operation.");
#endif
}
}
