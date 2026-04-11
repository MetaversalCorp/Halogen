// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <utility>

namespace filament {
class Engine;
}

namespace AnariFilament {

template <typename T>
class FilamentResource {
public:
    using Deleter = std::function<void()>;

    FilamentResource() = default;
    FilamentResource(filament::Engine *engine, T *ptr)
        : mEngine(engine), mPtr(ptr) {}
    FilamentResource(filament::Engine *engine, T *ptr, Deleter deleter)
        : mEngine(engine), mPtr(ptr), mDeleter(std::move(deleter)) {}

    ~FilamentResource() { destroy(); }

    FilamentResource(const FilamentResource &) = delete;
    FilamentResource &operator=(const FilamentResource &) = delete;

    FilamentResource(FilamentResource &&o) noexcept
        : mEngine(o.mEngine)
        , mPtr(std::exchange(o.mPtr, nullptr))
        , mDeleter(std::move(o.mDeleter)) {}

    FilamentResource &operator=(FilamentResource &&o) noexcept {
        destroy();
        mEngine = o.mEngine;
        mPtr = std::exchange(o.mPtr, nullptr);
        mDeleter = std::move(o.mDeleter);
        return *this;
    }

    T *get() const { return mPtr; }
    T *operator->() const { return mPtr; }
    explicit operator bool() const { return mPtr != nullptr; }

    void reset(T *ptr = nullptr);

private:
    void destroy();

    filament::Engine *mEngine = nullptr;
    T *mPtr = nullptr;
    Deleter mDeleter;
};

}
