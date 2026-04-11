// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Frame.h"

#include <filament/Engine.h>
#include <filament/RenderTarget.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <filament/Viewport.h>

#include <backend/PixelBufferDescriptor.h>

#include <anari/frontend/type_utility.h>

#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Frame *);

namespace AnariFilament {

Frame::Frame(DeviceState *s)
    : helium::BaseFrame(s)
{
    mView = s->engine->createView();
}

Frame::~Frame()
{
    auto *engine = deviceState()->engine;
    if (mRenderTarget)
        engine->destroy(mRenderTarget);
    if (mColorTexture)
        engine->destroy(mColorTexture);
    if (mDepthTexture)
        engine->destroy(mDepthTexture);
    if (mSwapChain)
        engine->destroy(mSwapChain);
    engine->destroy(mView);
}

bool Frame::isValid() const
{
    return mCamera && mWorld && mWidth > 0 && mHeight > 0;
}

bool Frame::getProperty(const std::string_view &,
    ANARIDataType,
    void *,
    uint64_t,
    uint32_t)
{
    return false;
}

void Frame::commitParameters()
{
    mRenderer = getParamObject<AnariFilament::Renderer>("renderer");
    mCamera = getParamObject<Camera>("camera");
    mWorld = getParamObject<World>("world");

    auto imgSize = getParam<anari::math::uint2>(
        "size", anari::math::uint2(0u, 0u));
    mWidth = imgSize[0];
    mHeight = imgSize[1];

    mColorType = getParam<anari::DataType>(
        "channel.color", ANARI_UNKNOWN);

    markCommitted();
}

void Frame::finalize() {}

void Frame::renderFrame()
{
    auto *state = deviceState();
    auto *engine = state->engine;
    auto *renderer = state->renderer;

    state->commitBuffer.flush();

    if (!isValid())
        return;

    // Ensure all pending GPU uploads are complete before rendering
    engine->flushAndWait();

    // Disable post-processing for linear output; camera exposure handles
    // the intensity mapping via setExposure(1.0f) (neutral).
    mView->setPostProcessingEnabled(false);
    mView->setAntiAliasing(filament::View::AntiAliasing::NONE);
    mView->setDithering(filament::View::Dithering::NONE);



    // Recreate render target if size changed
    if (mRenderTarget) {
        engine->destroy(mRenderTarget);
        mRenderTarget = nullptr;
    }
    if (mColorTexture) {
        engine->destroy(mColorTexture);
        mColorTexture = nullptr;
    }
    if (mDepthTexture) {
        engine->destroy(mDepthTexture);
        mDepthTexture = nullptr;
    }

    mColorTexture = filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .usage(filament::Texture::Usage::COLOR_ATTACHMENT
            | filament::Texture::Usage::SAMPLEABLE
            | filament::Texture::Usage::BLIT_SRC)
        .build(*engine);

    mDepthTexture = filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(filament::Texture::InternalFormat::DEPTH24)
        .usage(filament::Texture::Usage::DEPTH_ATTACHMENT)
        .build(*engine);

    mRenderTarget = filament::RenderTarget::Builder()
        .texture(filament::RenderTarget::AttachmentPoint::COLOR0,
            mColorTexture)
        .texture(filament::RenderTarget::AttachmentPoint::DEPTH,
            mDepthTexture)
        .build(*engine);

    // Set up the view
    mView->setScene(mWorld->filamentScene());
    mView->setCamera(mCamera->filamentCamera());
    mView->setViewport({0, 0, mWidth, mHeight});
    mView->setRenderTarget(mRenderTarget);

    // Dummy headless swap chain for beginFrame/endFrame
    if (!mSwapChain)
        mSwapChain = engine->createSwapChain(1, 1, 0);

    // Allocate pixel buffer (RGBA8)
    mPixelBuffer.resize(mWidth * mHeight * 4, 0);
    mFrameReady = false;

    if (renderer->beginFrame(mSwapChain)) {
        renderer->render(mView);

        using namespace filament::backend;

        auto *bufferData = mPixelBuffer.data();
        auto bufferSize = mPixelBuffer.size();

        renderer->readPixels(mRenderTarget, 0, 0, mWidth, mHeight,
            PixelBufferDescriptor(
                bufferData, bufferSize,
                PixelDataFormat::RGBA,
                PixelDataType::UBYTE));

        renderer->endFrame();
    }

    engine->flushAndWait();
    mFrameReady = true;
}

void *Frame::map(std::string_view channel,
    uint32_t *width,
    uint32_t *height,
    ANARIDataType *pixelType)
{
    if (channel != "channel.color" && channel != "color")
        return nullptr;

    *width = mWidth;
    *height = mHeight;
    *pixelType = ANARI_UFIXED8_RGBA_SRGB;

    return mPixelBuffer.data();
}

void Frame::unmap(std::string_view) {}

int Frame::frameReady(ANARIWaitMask)
{
    return mFrameReady ? 1 : 0;
}

void Frame::discard()
{
    mFrameReady = false;
}

DeviceState *Frame::deviceState() const
{
    return asFilamentState(m_state);
}

}
