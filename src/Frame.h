// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Camera.h"
#include "FilamentResource.h"
#include "Renderer.h"
#include "World.h"

#include <helium/BaseFrame.h>
#include <helium/utility/IntrusivePtr.h>

#include <Corrade/Containers/Array.h>

namespace filament {
class RenderTarget;
class SwapChain;
class Texture;
class View;
}

namespace AnariFilament {

struct Frame : public helium::BaseFrame
{
    Frame(DeviceState *s);
    ~Frame() override;

    bool isValid() const override;

    bool getProperty(const std::string_view &name,
        ANARIDataType type,
        void *ptr,
        uint64_t size,
        uint32_t flags) override;

    void commitParameters() override;
    void finalize() override;

    void renderFrame() override;

    void *map(std::string_view channel,
        uint32_t *width,
        uint32_t *height,
        ANARIDataType *pixelType) override;
    void unmap(std::string_view channel) override;
    int frameReady(ANARIWaitMask m) override;
    void discard() override;

    DeviceState *deviceState() const;

private:
    uint32_t mWidth = 0;
    uint32_t mHeight = 0;
    ANARIDataType mColorType = ANARI_UNKNOWN;

    helium::IntrusivePtr<AnariFilament::Renderer> mRenderer;
    helium::IntrusivePtr<Camera> mCamera;
    helium::IntrusivePtr<World> mWorld;

    FilamentResource<filament::View> mView;
    FilamentResource<filament::SwapChain> mSwapChain;
    FilamentResource<filament::Texture> mColorTexture;
    FilamentResource<filament::Texture> mDepthTexture;
    FilamentResource<filament::RenderTarget> mRenderTarget;

    Corrade::Containers::Array<char> mPixelBuffer;
    bool mFrameReady = false;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Frame *, ANARI_FRAME);
