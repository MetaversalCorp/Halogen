// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "FilamentResource.h"
#include "Object.h"

namespace filament {
class SwapChain;
}

namespace AnariFilament {

// Extension object that wraps a platform-native window handle (HWND, NSView,
// ANativeWindow*, CAMetalLayer*) and creates a Filament SwapChain from it.
// When attached to a Frame, rendering goes directly to the native surface
// instead of an offscreen render target — eliminating the CPU pixel copy.
//
// Usage:
//   ANARIObject ns = anariNewObject(device, "nativeSurface", "default");
//   anariSetParameter(device, ns, "nativeWindow", ANARI_VOID_POINTER, &hwnd);
//   anariCommitParameters(device, ns);
//   anariSetParameter(device, frame, "nativeSurface", ANARI_OBJECT, &ns);
struct NativeSurface : public Object
{
    NativeSurface(DeviceState *s);
    ~NativeSurface() override = default;

    void commitParameters() override;

    bool isValid() const override;

    filament::SwapChain *swapChain() const;

private:
    void *mNativeWindow = nullptr;
    uint64_t mFlags = 0;
    FilamentResource<filament::SwapChain> mSwapChain;
};

}
