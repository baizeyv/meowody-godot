#pragma once
#include "resource_operation.h"
#include "runtime/download/download_center.h"
#include <godot_cpp/variant/dictionary.hpp>
#include <set>

namespace meowody::detail {
// The catalog is fixed before any resource is loaded. Packs remain mounted for the process lifetime.
class ResourceCatalog {
public:
    explicit ResourceCatalog(DownloadCenter& downloads) : m_downloads(downloads) {}
    ResourceOperation initialize(const ResourceOptions& options = {});
    ResourceOperation download_dependencies(const godot::String& address);
    void update();
    void shutdown();
    bool idle() const;
    bool ready() const { return m_init && m_init->status == LoadStatus::Succeeded; }
    LoadError error() const { return m_init ? m_init->error : LoadError::NotInitialized; }
    godot::String message() const { return m_init ? m_init->message : godot::String("Call initialize before loading resources"); }
    godot::String path(const godot::String& address) const;
    godot::String address_for_path(const godot::String& path) const;
    std::vector<godot::String> select(const godot::String& name, bool label) const;
    std::int64_t download_size(const godot::String& address) const;
    float download_progress(const godot::String& address) const;
    bool cached(const godot::String& address) const;
    bool clear_cached(const godot::String& address);
    LoadError prepare(const godot::String& address, bool start_download);
private:
    struct Pack {
        godot::String file, url, sha256, local;
        std::int64_t size = 0;
        std::vector<godot::String> dependencies;
        DownloadHandle download;
        bool mounted = false;
        mutable godot::String verified_path;
    };
    struct Entry { godot::String path, pack, group; std::vector<godot::String> labels; };
    struct Operation { std::shared_ptr<ResourceOperationState> state; std::vector<godot::String> packs; };
    DownloadCenter& m_downloads;
    ResourceOptions m_options;
    godot::Dictionary m_builtin, m_candidate;
    std::map<godot::String, Pack> m_packs;
    std::map<godot::String, Entry> m_entries;
    std::vector<Operation> m_operations;
    std::shared_ptr<ResourceOperationState> m_init;
    DownloadHandle m_pointer, m_catalog_download;
    godot::String m_remote, m_cached_catalog;
    bool m_source = false, m_selecting = false, m_closed = false;
    int m_fallback_stage = 0;
    godot::String parse(const godot::Dictionary& catalog);
    bool closure(const godot::String& pack, std::vector<godot::String>& output, std::set<godot::String>& visiting) const;
    std::vector<godot::String> dependencies(const godot::String& address) const;
    godot::String pack_path(const Pack& pack) const;
    LoadError advance(Operation& operation, bool start_download);
    bool choose(const godot::Dictionary& catalog);
    void fallback(const godot::String& reason);
    void fail(LoadError error, const godot::String& message);
};
}
