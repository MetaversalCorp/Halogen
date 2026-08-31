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
    mExternalImage = getParam<uint64_t>("externalImage", 0);
    mExternalWidth = getParam<uint32_t>("width", 0);
    mExternalHeight = getParam<uint32_t>("height", 0);
    mExternalFormat = getParam<uint32_t>("imageFormat", 0);
    rebuildSwapChain();
    markCommitted();
}

void NativeSurface::rebuildSwapChain()
{
    mSwapChain.reset();
    if (mExternalImage)
        return;
    if (mNativeWindow) {
        mSwapChain.reset(
            deviceState()->engine->createSwapChain(mNativeWindow, mFlags));
    }
}

bool NativeSurface::isValid() const
{
    if (mExternalImage && mExternalWidth > 0 && mExternalHeight > 0)
        return true;
    return mSwapChain.get() != nullptr;
}

filament::SwapChain *NativeSurface::swapChain() const
{
    return mSwapChain.get();
}

bool NativeSurface::hasExternalImage() const
{
    return mExternalImage != 0;
}

uint64_t NativeSurface::externalImage() const
{
    return mExternalImage;
}

uint32_t NativeSurface::externalWidth() const
{
    return mExternalWidth;
}

uint32_t NativeSurface::externalHeight() const
{
    return mExternalHeight;
}

uint32_t NativeSurface::externalFormat() const
{
    return mExternalFormat;
}

}
