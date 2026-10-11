#pragma once
namespace meowody {
// Register the concrete type with create<T>/create_scene<T> to bind these hooks without RTTI.
class IPoolable {
public:
    virtual ~IPoolable() = default;
    virtual void on_pool_acquire() = 0;
    virtual void on_pool_release() = 0;
};
}
