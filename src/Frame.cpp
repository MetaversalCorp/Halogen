// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Frame.h"

#include <filament/Engine.h>
#include <filament/IndirectLight.h>
#include <filament/Options.h>
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
    , mBlackCubemap(s->engine, nullptr)
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

    const anari::math::uint2 imgSize = getParam<anari::math::uint2>(
        "size", anari::math::uint2(0u, 0u));
    mWidth = imgSize[0];
    mHeight = imgSize[1];

    mColorType = getParam<anari::DataType>(
        "channel.color", ANARI_UNKNOWN);

    mDepthType = getParam<anari::DataType>(
        "channel.depth", ANARI_UNKNOWN);

    markCommitted();
}

void Frame::finalize() {}

void Frame::renderFrame()
{
    DeviceState * const state = deviceState();
    filament::Engine * const engine = state->engine;
    filament::Renderer * const renderer = state->renderer.get();

    state->commitBuffer.flush();

    if (!isValid())
        return;

    // Ensure all pending GPU uploads and any previous readback are complete
    // before rendering, then clear the scheduled-readback flag.
    engine->flushAndWait();
    mReadbackScheduled = false;

    const bool wantFloat =
        (mColorType == ANARI_FLOAT32_VEC4 || mColorType == ANARI_FLOAT32);

    if (wantFloat) {
        // Float output: disable post-processing to preserve exact linear HDR
        // values in the readback buffer.  MSAA is not needed for pixel-exact
        // float output (Blender composites the raw values).
        mView->setPostProcessingEnabled(false);
    } else {
        // SRGB output: full post-processing with ACES tonemapping, FXAA,
        // 4x MSAA, and subtle bloom for visual quality in screenshots.
        mView->setPostProcessingEnabled(true);
        mView->setColorGrading(nullptr);
        mView->setAntiAliasing(filament::View::AntiAliasing::FXAA);
        mView->setDithering(filament::View::Dithering::TEMPORAL);
        filament::View::MultiSampleAntiAliasingOptions msaa;
        msaa.enabled = true;
        msaa.sampleCount = 4;
        mView->setMultiSampleAntiAliasingOptions(msaa);
        filament::View::BloomOptions bloom;
        bloom.enabled = true;
        bloom.strength = 0.1f;
        mView->setBloomOptions(bloom);
    }

    // Enable soft shadows (PCF)
    mView->setShadowType(filament::View::ShadowType::PCF);

    // Set up indirect (image-based) lighting.
    //
    // When the renderer's ambientRadiance > 0, honour the user-specified
    // uniform ambient.  Otherwise, apply a default 3-band spherical-
    // harmonics environment (from Filament's "lightroom_14b" asset) so
    // that PBR materials look reasonable out of the box.
    mIndirectLight.reset();
    if (mRenderer && mRenderer->ambientRadiance() > 0.0f) {
        const anari::math::float3 ac = mRenderer->ambientColor();
        const float ar = mRenderer->ambientRadiance();

        const filament::math::float3 sh[1] = {
            {ac[0], ac[1], ac[2]}};
        mIndirectLight.reset(
            filament::IndirectLight::Builder()
                .irradiance(1, sh)
                .intensity(ar)
                .build(*engine));
    } else {
        // No explicit ambient: provide a black 1×1 cubemap as the reflection
        // environment so Filament uses it instead of its built-in default
        // specular fallback (which contributes ~1.0 regardless of intensity).
        // Created lazily and reused across frames.
        if (!mBlackCubemap) {
            mBlackCubemap.reset(filament::Texture::Builder()
                .width(1)
                .height(1)
                .levels(1)
                .sampler(filament::Texture::Sampler::SAMPLER_CUBEMAP)
                .format(filament::Texture::InternalFormat::RGBA8)
                .build(*engine));
            for (int face = 0; face < 6; ++face) {
                auto *px = new uint8_t[4]{0, 0, 0, 0};
                mBlackCubemap->setImage(*engine, 0,
                    0, 0, static_cast<uint32_t>(face), 1, 1, 1,
                    filament::backend::PixelBufferDescriptor(
                        px, 4,
                        filament::backend::PixelDataFormat::RGBA,
                        filament::backend::PixelDataType::UBYTE,
                        [](void *buf, size_t, void *) {
                            delete[] static_cast<uint8_t *>(buf);
                        }));
            }
        }
        const filament::math::float3 sh[1] = {{0.0f, 0.0f, 0.0f}};
        mIndirectLight.reset(
            filament::IndirectLight::Builder()
                .reflections(mBlackCubemap.get())
                .irradiance(1, sh)
                .intensity(0.0f)
                .build(*engine));
    }
    mWorld->filamentScene()->setIndirectLight(mIndirectLight.get());

    // Recreate render target if size changed
    mRenderTarget.reset();
    mColorTexture.reset();
    mDepthTexture.reset();

    mColorTexture.reset(filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(wantFloat
            ? filament::Texture::InternalFormat::RGBA32F
            : filament::Texture::InternalFormat::RGBA8)
        .usage(filament::Texture::Usage::COLOR_ATTACHMENT
            | filament::Texture::Usage::SAMPLEABLE
            | filament::Texture::Usage::BLIT_SRC)
        .build(*engine));

    mDepthTexture.reset(filament::Texture::Builder()
        .width(mWidth)
        .height(mHeight)
        .levels(1)
        .format(filament::Texture::InternalFormat::DEPTH32F)
        .usage(filament::Texture::Usage::DEPTH_ATTACHMENT
            | filament::Texture::Usage::BLIT_SRC)
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

    // Allocate pixel buffer
    const size_t bytesPerPixel = wantFloat ? 16u : 4u;
    mPixelBuffer = Corrade::Containers::Array<char>{
        Corrade::ValueInit, mWidth * mHeight * bytesPerPixel};

    mFrameReady = false;

    if (renderer->beginFrame(mSwapChain.get())) {
        // Set the background/clear color from the renderer parameters
        if (mRenderer) {
            const anari::math::float4 bg = mRenderer->backgroundColor();
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
        const size_t bufferSize = mPixelBuffer.size();

        renderer->readPixels(mRenderTarget.get(), 0, 0, mWidth, mHeight,
            PixelBufferDescriptor(
                bufferData, bufferSize,
                PixelDataFormat::RGBA,
                wantFloat ? PixelDataType::FLOAT : PixelDataType::UBYTE));

        renderer->endFrame();
    }

    // readPixels() has been issued; defer flushAndWait() and the vertical
    // flip to map() so callers that never map skip the expensive readback.
    mReadbackScheduled = true;
    mFrameReady = true;
}

void *Frame::map(std::string_view channel,
    uint32_t *width,
    uint32_t *height,
    ANARIDataType *pixelType)
{
    *width = mWidth;
    *height = mHeight;

    if (channel == "channel.depth" || channel == "depth") {
        // Depth readback is not supported by Filament's Renderer::readPixels
        // (it only reads from the COLOR attachment of a RenderTarget).
        (void)mDepthType;
        return nullptr;
    }

    if (channel == "channel.color" || channel == "color") {
        if (mColorType == ANARI_FLOAT32_VEC4
            || mColorType == ANARI_FLOAT32) {
            *pixelType = ANARI_FLOAT32_VEC4;
        } else {
            *pixelType = ANARI_UFIXED8_RGBA_SRGB;
        }
        if (mReadbackScheduled) {
            // Complete the GPU readback that was scheduled in renderFrame().
            // Flip vertically here: Filament row 0 = bottom, ANARI row 0 = top.
            DeviceState *const state = deviceState();
            state->engine->flushAndWait();
            const bool wantFloat =
                (mColorType == ANARI_FLOAT32_VEC4 || mColorType == ANARI_FLOAT32);
            const size_t bytesPerPixel = wantFloat ? 16u : 4u;
            const size_t rowBytes = mWidth * bytesPerPixel;
            Corrade::Containers::Array<char> rowTmp{Corrade::NoInit, rowBytes};
            char *buf = mPixelBuffer.data();
            for (uint32_t top = 0, bot = mHeight - 1; top < bot; ++top, --bot) {
                std::memcpy(rowTmp.data(), buf + top * rowBytes, rowBytes);
                std::memcpy(buf + top * rowBytes, buf + bot * rowBytes, rowBytes);
                std::memcpy(buf + bot * rowBytes, rowTmp.data(), rowBytes);
            }
            mReadbackScheduled = false;
        }
        return mPixelBuffer.data();
    }

    return nullptr;
}

void Frame::unmap(std::string_view) {}

int Frame::frameReady(ANARIWaitMask)
{
    return mFrameReady ? 1 : 0;
}

void Frame::discard()
{
    mFrameReady = false;
    mReadbackScheduled = false;
}

DeviceState *Frame::deviceState() const
{
    return asFilamentState(m_state);
}

}
