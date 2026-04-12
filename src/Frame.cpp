// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Frame.h"

#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/RenderTarget.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/SwapChain.h>
#include <filament/Texture.h>
#include <filament/View.h>
#include <filament/Viewport.h>

#include <backend/PixelBufferDescriptor.h>

#include <math/vec3.h>

#include <anari/frontend/type_utility.h>

#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Frame *);

namespace AnariFilament {

Frame::Frame(DeviceState *s)
    : helium::BaseFrame(s)
    , mView(s->engine, s->engine->createView())
    , mSwapChain(s->engine, nullptr)
    , mColorTexture(s->engine, nullptr)
    , mDepthTexture(s->engine, nullptr)
    , mRenderTarget(s->engine, nullptr)
    , mIndirectLight(s->engine, nullptr)
{
}

Frame::~Frame() = default;

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

    anari::math::uint2 imgSize = getParam<anari::math::uint2>(
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
    DeviceState *state = deviceState();
    filament::Engine *engine = state->engine;
    filament::Renderer *renderer = state->renderer.get();

    state->commitBuffer.flush();

    if (!isValid())
        return;

    // Ensure all pending GPU uploads are complete before rendering
    engine->flushAndWait();

    // Enable high-quality rendering defaults:
    // post-processing (tone mapping, HDR), FXAA, temporal dithering, bloom
    mView->setPostProcessingEnabled(true);
    mView->setAntiAliasing(filament::View::AntiAliasing::FXAA);
    mView->setDithering(filament::View::Dithering::TEMPORAL);

    filament::View::BloomOptions bloomOptions;
    bloomOptions.enabled = true;
    bloomOptions.strength = 0.10f;
    mView->setBloomOptions(bloomOptions);

    // Set up ambient (indirect) lighting from renderer parameters
    mIndirectLight.reset();
    if (mRenderer && mRenderer->ambientRadiance() > 0.0f) {
        anari::math::float3 ac = mRenderer->ambientColor();
        float ar = mRenderer->ambientRadiance();

        // Single-band spherical harmonics for uniform ambient irradiance.
        // The SH DC coefficient for a constant irradiance E is:
        //   L00 = E / pi  (since irradiance = pi * L00 for SH)
        // Filament's IndirectLight.Builder().irradiance(bands, sh) expects
        // the SH coefficient directly; the intensity() multiplier scales it.
        filament::math::float3 sh[1] = {
            {ac[0], ac[1], ac[2]}};
        mIndirectLight.reset(
            filament::IndirectLight::Builder()
                .irradiance(1, sh)
                .intensity(ar)
                .build(*engine));
        mWorld->filamentScene()->setIndirectLight(mIndirectLight.get());
    } else {
        mWorld->filamentScene()->setIndirectLight(nullptr);
    }

    // Recreate render target if size changed
    mRenderTarget.reset();
    mColorTexture.reset();
    mDepthTexture.reset();

    mColorTexture.reset(filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .usage(filament::Texture::Usage::COLOR_ATTACHMENT
            | filament::Texture::Usage::SAMPLEABLE
            | filament::Texture::Usage::BLIT_SRC)
        .build(*engine));

    mDepthTexture.reset(filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(filament::Texture::InternalFormat::DEPTH24)
        .usage(filament::Texture::Usage::DEPTH_ATTACHMENT)
        .build(*engine));

    mRenderTarget.reset(filament::RenderTarget::Builder()
        .texture(filament::RenderTarget::AttachmentPoint::COLOR0,
            mColorTexture.get())
        .texture(filament::RenderTarget::AttachmentPoint::DEPTH,
            mDepthTexture.get())
        .build(*engine));

    // Set up the view
    mView->setScene(mWorld->filamentScene());
    mView->setCamera(mCamera->filamentCamera());
    mView->setViewport({0, 0, mWidth, mHeight});
    mView->setRenderTarget(mRenderTarget.get());

    // Dummy headless swap chain for beginFrame/endFrame
    if (!mSwapChain)
        mSwapChain.reset(engine->createSwapChain(1, 1, 0));

    // Allocate pixel buffer (RGBA8)
    mPixelBuffer = Corrade::Containers::Array<char>{
        Corrade::ValueInit, mWidth * mHeight * 4};
    mFrameReady = false;

    if (renderer->beginFrame(mSwapChain.get())) {
        // Set the background/clear color from the renderer parameters
        if (mRenderer) {
            anari::math::float4 bg = mRenderer->backgroundColor();
            filament::Renderer::ClearOptions clearOpts;
            clearOpts.clearColor = {bg[0], bg[1], bg[2], bg[3]};
            clearOpts.clear = true;
            clearOpts.discard = true;
            renderer->setClearOptions(clearOpts);
        }

        renderer->render(mView.get());

        using namespace filament::backend;

        uint8_t *bufferData =
            reinterpret_cast<uint8_t *>(mPixelBuffer.data());
        size_t bufferSize = mPixelBuffer.size();

        renderer->readPixels(mRenderTarget.get(), 0, 0, mWidth, mHeight,
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
