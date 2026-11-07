// Standalone native checks; no Godot or Python runtime is loaded by this executable.
#include "../src/project_creator/creator_backend.cpp"
#include <cassert>
#include <iostream>

int wmain(int argc, wchar_t **argv) {
    if (argc == 6 && std::wstring(argv[1]) == L"--build") {
        creator::Job job;
        job.operation = "rebuild"; job.destination = creator::utf8(std::filesystem::absolute(argv[2]));
        job.engine = creator::utf8(std::filesystem::absolute(argv[3]));
        job.expected_version = creator::utf8(std::filesystem::path(argv[4])); job.pin = creator::utf8(std::filesystem::path(argv[5]));
        auto log = std::filesystem::u8path(job.destination) / "build/creator/native-integration.log";
        std::error_code ec; std::filesystem::create_directories(log.parent_path(), ec);
        auto reply = creator::execute(job, creator::utf8(log));
        if (!reply.ok) { std::cerr << reply.message << "\n" << creator::read(log); return 1; }
        std::cout << "[CreatorNative] PASS generated project build, editor import and runtime\n";
        return 0;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--version") {
        auto name = creator::lower(creator::utf8(std::filesystem::path(argv[0]).filename()));
        std::cout << (name.find("dev") != std::string::npos ? "4.8.dev.official.abcdef" : "4.7.2.stable.official.abcdef") << std::endl;
        return 0;
    }
    if (argc > 1 && std::wstring(argv[1]) == L"--help") { std::cout << "--editor --dump-extension-api" << std::endl; return 0; }
    if (argc > 1 && std::wstring(argv[1]) == L"--echo") {
        for (int i = 2; i < argc; ++i) std::cout << creator::utf8(std::filesystem::path(argv[i])) << "\n";
        return 0;
    }
    namespace fs = std::filesystem;
    auto executable = fs::absolute(argv[0]);
    auto root = executable.parent_path() / "native-fixtures";
    std::error_code ec;
    fs::create_directories(root, ec); assert(!ec);
    auto silent = [](const std::string &) {};
    creator::Engine engine;
    assert(creator::version("4.7.2.stable.official.abcdef", engine) && engine.api == "4.7");
    assert(creator::version("4.7.stable.official.abcdef", engine));
    assert(!creator::version("3.6.stable.official.abcdef", engine));
    assert(!creator::version("4.8.dev.official.abcdef", engine));
    assert(!creator::version("4.7.stable.mono.official.abcdef", engine));
    {
        std::string hash(40, 'a');
        auto refs = creator::framework_refs(hash + "\trefs/tags/v1.0\n" + hash + "\trefs/heads/develop\n" + hash + "\trefs/heads/main\n" + hash + "\trefs/tags/v1.0^{}\n" + hash + "\trefs/heads/main\n");
        assert(refs.size() == 3 && refs[0].name == "main" && refs[0].ref == "refs/heads/main" && refs[1].name == "develop" && refs[2].kind == "tag");
    }
    {
        std::string exact(40, 'a'), minor(40, 'b'), independent(40, 'c');
        auto refs = exact + "\trefs/tags/godot-4.4.1-stable\n" + minor + "\trefs/tags/godot-4.4-stable\n" + independent + "\trefs/tags/10.0.0-stable\n";
        creator::Reply reply;
        assert(creator::choose_dependency(refs, "4.4.1.stable.official.abcdef", "4.4", "10.0.0-stable", independent, reply) && reply.cpp_commit == exact);
        assert(creator::choose_dependency(refs, "4.4.2.stable.official.abcdef", "4.4", "10.0.0-stable", independent, reply) && reply.cpp_commit == minor);
        assert(creator::choose_dependency(refs, "4.7.2.stable.official.abcdef", "4.7", "10.0.0-stable", independent, reply) && reply.cpp_commit == independent);
        assert(!creator::choose_dependency(refs, "4.8.stable.official.abcdef", "4.8", "", independent, reply));
        assert(!creator::choose_dependency(refs, "4.7.2.stable.official.abcdef", "4.7", "10.0.0-stable", exact, reply));
        assert(creator::choose_dependency(refs, "4.7.2.stable.official.abcdef", "4.7", "", independent, reply, true) && reply.cpp_tag == "10.0.0-stable" && reply.cpp_commit == independent);
        assert(!creator::choose_dependency(refs, "4.7.2.stable.official.abcdef", "4.7", "", exact, reply, true));
        assert(!creator::choose_dependency(refs, "4.7.2.stable.official.abcdef", "4.7", "", independent, reply));
        assert(!creator::choose_dependency(refs, "4.8.stable.official.abcdef", "4.8", "", independent, reply, true));
        refs += independent + "\trefs/tags/godot-4.5-stable\n" + exact + "\trefs/tags/godot-4.5-stable^{}\n";
        assert(creator::choose_dependency(refs, "4.5.stable.official.abcdef", "4.5", "", "", reply) && reply.cpp_commit == exact);
    }
    auto copied = root / L"Godot_fixture.exe";
    fs::copy_file(executable, copied, fs::copy_options::overwrite_existing, ec); assert(!ec);
    assert(creator::inspect(copied, engine));
    auto dev = root / L"Godot_dev.exe";
    fs::copy_file(executable, dev, fs::copy_options::overwrite_existing, ec); assert(!ec);
    assert(!creator::inspect(dev, engine));
    auto arm = root / L"Godot_arm64.exe";
    fs::copy_file(executable, arm, fs::copy_options::overwrite_existing, ec); assert(!ec);
    {
        std::fstream stream(arm, std::ios::in | std::ios::out | std::ios::binary);
        stream.seekg(60); uint32_t offset; stream.read(reinterpret_cast<char *>(&offset), 4);
        stream.seekp(offset + 4); stream.write("\x64\xaa", 2);
    }
    assert(!creator::inspect(arm, engine));
    auto echo = creator::run({executable.wstring(), L"--echo", L"D:\\中文 project\\", L"quote\"value", L"$(literal)"}, root, {}, 5000);
    assert(echo.ok && echo.output.find("D:/中文 project/") != std::string::npos && echo.output.find("quote\"value") != std::string::npos && echo.output.find("$(literal)") != std::string::npos);
    auto data = root / "hash.txt";
    assert(creator::write(data, "abc"));
    assert(creator::sha512(data) == "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    auto source = root / "source", destination = root / "copy";
    fs::create_directories(source / ".git", ec); assert(!ec);
    assert(creator::write(source / "framework.cpp", "// source"));
    assert(creator::write(source / "old.obj", "artifact"));
    assert(creator::write(source / ".git/config", "private"));
    std::string error;
    assert(creator::copy_source(source, destination, error));
    assert(fs::exists(destination / "framework.cpp") && !fs::exists(destination / "old.obj") && !fs::exists(destination / ".git"));
    assert(!creator::empty_destination(creator::utf8(destination)));
    assert(creator::empty_destination(creator::utf8(root / "new-directory")));
    creator::Job job; job.operation = "prepare"; job.destination = creator::utf8(destination);
    auto protected_result = creator::execute(job, creator::utf8(root / "task.log"));
    assert(!protected_result.ok && protected_result.message.find("empty") != std::string::npos);
    assert(creator::read(destination / "framework.cpp") == "// source");
    job = {}; job.operation = "inspect"; job.engine = creator::utf8(copied);
    auto inspected = creator::execute(job, creator::utf8(root / "task.log"));
    assert(inspected.ok && inspected.engines.size() == 1);
    if (argc > 1 && std::wstring(argv[1]) == L"--online") {
        job = {}; job.operation = "refs";
        auto available = creator::execute(job, creator::utf8(root / "network.log"));
        if (!available.ok) { std::cerr << available.message << "\n"; return 1; }
        assert(!available.refs.empty());
        job = {}; job.operation = "versions";
        auto reply = creator::execute(job, creator::utf8(root / "network.log"));
        if (!reply.ok) { std::cerr << reply.message << "\n"; return 1; }
        assert(reply.text.find("tag_name") != std::string::npos && reply.extra.find("tag_name") != std::string::npos);
        job.operation = "resolve"; job.expected_version = "4.7.2.stable.official.abcdef";
        job.cpp_tag = "10.0.0-stable"; job.framework_pin = "507ed9d840c01a3c5b2a39af8bb4000bfac30bf5";
        reply = creator::execute(job, creator::utf8(root / "network.log"));
        if (!reply.ok) { std::cerr << reply.message << "\n"; return 1; }
        assert(reply.cpp_tag == "10.0.0-stable" && reply.cpp_commit == job.framework_pin);
        job.cpp_tag.clear(); job.legacy_dependency_record = true;
        reply = creator::execute(job, creator::utf8(root / "network.log"));
        if (!reply.ok) { std::cerr << reply.message << "\n"; return 1; }
        assert(reply.cpp_tag == "10.0.0-stable" && reply.cpp_commit == job.framework_pin);
        std::cout << "[CreatorNative] PASS live official release lookup\n";
    }
    std::cout << "[CreatorNative] PASS PE/version filtering, process quoting, SHA512, source filtering and directory protection\n";
    return 0;
}
