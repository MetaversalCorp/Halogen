// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <Corrade/Containers/Function.h>
#include <Corrade/Utility/Move.h>

namespace filament {
class Engine;
}

namespace Halogen {

template <typename T>
class FilamentResource {
public:
    using Deleter = Corrade::Containers::Function<void()>;

    FilamentResource() = default;
    FilamentResource(filament::Engine *engine, T *ptr)
        : mEngine(engine), mPtr(ptr) {}
    FilamentResource(filament::Engine *engine, T *ptr, Deleter &&deleter)
        : mEngine(engine), mPtr(ptr), mDeleter(Corrade::Utility::move(deleter)) {}

    ~FilamentResource() { destroy(); }

    FilamentResource(const FilamentResource &) = delete;
    FilamentResource &operator=(const FilamentResource &) = delete;

    FilamentResource(FilamentResource &&o) noexcept
        : mEngine(o.mEngine)
        , mPtr(Corrade::Utility::move(o.mPtr))
        , mDeleter(Corrade::Utility::move(o.mDeleter))
    {
        o.mPtr = nullptr;
    }

    FilamentResource &operator=(FilamentResource &&o) noexcept {
        destroy();
        mEngine = o.mEngine;
        mPtr = Corrade::Utility::move(o.mPtr);
        o.mPtr = nullptr;
        mDeleter = Corrade::Utility::move(o.mDeleter);
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
