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
//   // NOTE: ANARI_VOID_POINTER takes the pointer value directly as the 5th
//   // arg — NOT a pointer to it (anari_cpp_impl.hpp:530 dereferences one
//   // extra level for this type). Passing &hwnd stores a dangling address.
//   anariSetParameter(device, ns, "nativeWindow", ANARI_VOID_POINTER, hwnd);
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
// OpenXR integration:
//   After device init, query Vulkan handles as ANARI_UINT64 properties:
//     halogen.vk.instance, halogen.vk.physicalDevice, halogen.vk.device,
//     halogen.vk.queue, halogen.vk.queueFamilyIndex, halogen.vk.queueIndex
//   Per-frame XR swapchain images are imported via Texture::Builder::import()
//   and rendered to a RenderTarget (not an ANativeWindow SwapChain). Set
//   "externalImage" (ANARI_UINT64 VkImage), "width", "height", and optionally
//   "imageFormat" (ANARI_UINT32 VkFormat). Rebind every frame — OpenXR
//   acquires a different image each wait. "waitGpu" (ANARI_UINT32, default 1)
//   flushAndWait after the draw; the stereo caller sets 0 on the first eye
//   and 1 on the last so both eyes share one GPU drain.
struct NativeSurface : public Object
{
    NativeSurface(DeviceState *s);
    ~NativeSurface() override = default;

    void commitParameters() override;
    void rebuildSwapChain();

    bool isValid() const override;

    filament::SwapChain *swapChain() const;

    bool hasExternalImage() const;
    uint64_t externalImage() const;
    uint32_t externalWidth() const;
    uint32_t externalHeight() const;
    uint32_t externalFormat() const;
    bool waitGpu() const;

private:
    void *mNativeWindow = nullptr;
    uint64_t mFlags = 0;
    uint64_t mExternalImage = 0;
    uint32_t mExternalWidth = 0;
    uint32_t mExternalHeight = 0;
    uint32_t mExternalFormat = 0;
    uint32_t mWaitGpu = 1;
    FilamentResource<filament::SwapChain> mSwapChain;
};

}
