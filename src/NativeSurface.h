// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "FilamentResource.h"
#include "Object.h"

namespace filament {
class SwapChain;
}

namespace Halogen {

// Extension object that wraps a platform-native window handle (HWND, NSView,
// ANativeWindow*, CAMetalLayer*) and creates a Filament SwapChain from it.
// When attached to a Frame, rendering goes directly to the native surface
// instead of an offscreen render target — eliminating the CPU pixel copy.
//
// Usage (windowed):
//   ANARIObject ns = anariNewObject(device, "nativeSurface", "default");
//   anariSetParameter(device, ns, "nativeWindow", ANARI_VOID_POINTER, &hwnd);
//   anariCommitParameters(device, ns);
//   anariSetParameter(device, frame, "nativeSurface", ANARI_OBJECT, &ns);
//
// Backend selection:
//   The engine chooses its backend automatically (Vulkan, OpenGL, or Metal).
//   Query the active backend after device init:
//     char buf[32];
//     anariGetProperty(device, device, "halogen.backend",
//         ANARI_STRING, buf, sizeof(buf), ANARI_WAIT);
//   Or force a specific backend before first use:
//     anariSetParameter(device, device, "backend", ANARI_STRING, "opengl");
//     anariCommitParameters(device, device);
//
// OpenXR integration (future):
//   For XR compositors that provide per-frame swapchain images, query
//   "halogen.backend" to create a matching XR graphics binding (Vulkan or
//   OpenGL). The XR swapchain images will be imported via
//   Texture::Builder::import() and rendered to a RenderTarget instead of a
//   SwapChain. This path will accept an "externalImage" parameter (intptr_t)
//   alongside "width" and "height" to specify the imported texture.
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
