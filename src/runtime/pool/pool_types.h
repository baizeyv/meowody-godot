#pragma once
#include <cstddef>
namespace meowody {
struct PoolOptions {
    size_t max_idle = 128;
    size_t max_total = 0; // Zero permits demand-driven growth without a total limit.
    size_t prewarm = 0;
};
struct PoolStats {
    bool exists = false;
    size_t idle = 0;
    size_t active = 0;
    size_t max_idle = 0;
    size_t max_total = 0;
    size_t total() const { return idle + active; }
};
}
