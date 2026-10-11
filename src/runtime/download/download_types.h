#pragma once

#include <cstdint>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace meowody
{
enum class DownloadStatus { Queued, Connecting, Downloading, Retrying, Paused, Succeeded, Failed, Cancelled };
enum class DownloadError { None, InvalidUrl, InvalidOptions, Busy, Network, Timeout, Http, Protocol, FileIO, TooLarge, Checksum, Cancelled, Closed, InvalidHandle };
inline bool is_download_finished(DownloadStatus status)
{
    return status == DownloadStatus::Succeeded || status == DownloadStatus::Failed || status == DownloadStatus::Cancelled;
}

struct DownloadOptions
{
    bool resume = true;
    bool use_cache = true;
    int max_retries = 2;
    int max_redirects = 5;
    double timeout_seconds = 30.0; // Connection/header/body inactivity timeout.
    double retry_delay_seconds = 1.0;
    std::int64_t max_bytes = 512LL * 1024 * 1024;
    godot::String expected_sha256;
    godot::PackedStringArray headers;
};

struct DownloadResult
{
    godot::String path;
    DownloadError error = DownloadError::None;
    godot::Error godot_error = godot::OK;
    int http_status = 0;
    godot::String message;
    bool from_cache = false;
    explicit operator bool() const { return error == DownloadError::None && !path.is_empty(); }
};
}
