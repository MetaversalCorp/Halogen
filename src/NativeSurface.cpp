// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "NativeSurface.h"

#include <filament/Engine.h>
#include <filament/SwapChain.h>

namespace Halogen {

NativeSurface::NativeSurface(DeviceState *s)
    : Object(ANARI_OBJECT, s)
    , mSwapChain(s->engine, nullptr)
{
}

void NativeSurface::commitParameters()
{
    mNativeWindow = getParam<void *>("nativeWindow", nullptr);
    mFlags = getParam<uint64_t>("flags", 0);

    // Recreate the SwapChain whenever the native window changes.
    mSwapChain.reset();
    if (mNativeWindow) {
        mSwapChain.reset(
            deviceState()->engine->createSwapChain(mNativeWindow, mFlags));
    }

    markCommitted();
}

bool NativeSurface::isValid() const
{
    return mSwapChain.get() != nullptr;
}

filament::SwapChain *NativeSurface::swapChain() const
{
    return mSwapChain.get();
}

}
