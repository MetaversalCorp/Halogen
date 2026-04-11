// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <utility>

namespace filament {
class Engine;
}

namespace AnariFilament {

template <typename T>
class FilamentResource {
public:
    FilamentResource() = default;
    FilamentResource(filament::Engine *engine, T *ptr)
        : mEngine(engine), mPtr(ptr) {}

    ~FilamentResource() { destroy(); }

    FilamentResource(const FilamentResource &) = delete;
    FilamentResource &operator=(const FilamentResource &) = delete;

    FilamentResource(FilamentResource &&o) noexcept
        : mEngine(o.mEngine), mPtr(std::exchange(o.mPtr, nullptr)) {}

    FilamentResource &operator=(FilamentResource &&o) noexcept {
        destroy();
        mEngine = o.mEngine;
        mPtr = std::exchange(o.mPtr, nullptr);
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
};

}
