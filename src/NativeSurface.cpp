// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "NativeSurface.h"

#include "AndroidSwapchain.h"

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
    rebuildSwapChain();
    markCommitted();
}

void NativeSurface::rebuildSwapChain()
{
    mSwapChain.reset();
    if (mNativeWindow) {
#if defined(__ANDROID__)
        if (mFlags & filament::SwapChain::CONFIG_TRANSPARENT) {
            installTransparentSwapchainHook();
        }
#endif
        mSwapChain.reset(
            deviceState()->engine->createSwapChain(mNativeWindow, mFlags));
    }
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
