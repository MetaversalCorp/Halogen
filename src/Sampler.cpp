// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Sampler.h"

#include "ColorConversion.h"

#include <filament/Engine.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>

#include <math/mat3.h>

#include <backend/PixelBufferDescriptor.h>

#include <helium/array/Array1D.h>
#include <helium/array/Array2D.h>
#include <helium/array/Array3D.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

#include <algorithm>

using namespace Corrade::Containers::Literals;

namespace {

// 2x2 box. Live image2D rewrites (the camera panel) call setImage on LOD0
// and then generateMipmaps. On Vulkan that blit can sample the previous
// LOD0, so LOD1 stays stale while LOD0 moves. Uploading LOD1+ from this
// buffer writes the same frame into every level.
void downsampleBox(const uint8_t *src, uint32_t sw, uint32_t sh,
    uint8_t *dst, uint32_t dw, uint32_t dh)
{
    for (uint32_t y = 0; y < dh; ++y) {
        uint32_t const y0 = y * 2;
        uint32_t const y1 = std::min(y0 + 1, sh - 1);
        for (uint32_t x = 0; x < dw; ++x) {
            uint32_t const x0 = x * 2;
            uint32_t const x1 = std::min(x0 + 1, sw - 1);
            uint8_t const *p00 = src + (size_t(y0) * sw + x0) * 4;
            uint8_t const *p10 = src + (size_t(y0) * sw + x1) * 4;
            uint8_t const *p01 = src + (size_t(y1) * sw + x0) * 4;
            uint8_t const *p11 = src + (size_t(y1) * sw + x1) * 4;
            uint8_t *o = dst + (size_t(y) * dw + x) * 4;
            for (int c = 0; c < 4; ++c)
                o[c] = uint8_t((int(p00[c]) + p10[c] + p01[c] + p11[c]) / 4);
        }
    }
}

filament::TextureSampler::WrapMode wrapFromString(
    const Corrade::Containers::String &s)
{
    filament::TextureSampler::WrapMode wrap =
        filament::TextureSampler::WrapMode::CLAMP_TO_EDGE;
    if (s == "repeat"_s)
        wrap = filament::TextureSampler::WrapMode::REPEAT;
    else if (s == "mirrorRepeat"_s)
        wrap = filament::TextureSampler::WrapMode::MIRRORED_REPEAT;
    return wrap;
}

}

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Sampler *);

namespace Halogen {

Sampler::Sampler(DeviceState *s, const char *subtype)
    : Object(ANARI_SAMPLER, s)
    , mSubtype(subtype ? subtype : "image2D") {}

Sampler::~Sampler()
{
    if (mTexture)
        deviceState()->engine->destroy(mTexture);
}

void Sampler::commitParameters()
{
    if (mSubtype == "image1D"_s)
        commitImage1D();
    else if (mSubtype == "image3D"_s)
        commitImage3D();
    else if (mSubtype == "transform"_s)
        commitTransform();
    else if (mSubtype == "primitive"_s)
        commitPrimitive();
    else
        commitImage2D();
}

void Sampler::commitImage2D()
{
    filament::Engine * const engine = deviceState()->engine;

    auto *imageArray = getParamObject<helium::Array2D>("image");
    if (!imageArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "image2D sampler requires 'image' parameter");
        return;
    }

    const Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);
    mWrapS = wrapFromString(getParamString("wrapMode1", "clampToEdge"));
    mWrapT = wrapFromString(getParamString("wrapMode2", "clampToEdge"));
    mInAttribute = getParamString("inAttribute", "attribute0");
    mTransform = getParam<anari::math::mat4>("inTransform",
        anari::math::mat4(anari::math::identity));
    const Corrade::Containers::String colorSpace =
        getParamString("colorSpace", "linear");
    mSrgb = (colorSpace == "sRGB"_s);

    const anari::math::uint2 dims = imageArray->size();
    const uint32_t width = dims[0];
    const uint32_t height = dims[1];
    const ANARIDataType type = imageArray->elementType();
    const size_t numPixels = size_t(width) * height;

    uint8_t levels = 1;
    if (!mNearest) {
        uint32_t m = width > height ? width : height;
        while (m > 1) {
            m >>= 1;
            ++levels;
        }
    }

    auto *ownedData = new uint8_t[numPixels * 4];
    convertToRGBA8(ownedData, imageArray->data(), type, numPixels);

    const filament::Texture::InternalFormat fmt = mSrgb
        ? filament::Texture::InternalFormat::SRGB8_A8
        : filament::Texture::InternalFormat::RGBA8;

    // Keep the Filament Texture when size/format match. destroy()+Builder on a
    // sampler the MaterialInstance still samples aborts Filament. Live video
    // (and any later image2D rewrite) must setImage the existing object.
    const bool reuse = mTexture
        && mTexture->getWidth() == width
        && mTexture->getHeight() == height
        && mTexture->getFormat() == fmt
        && mTexture->getLevels() == levels;

    if (!reuse) {
        if (mTexture) {
            engine->destroy(mTexture);
            mTexture = nullptr;
        }
        auto texUsage = filament::Texture::Usage::DEFAULT;
        if (levels > 1) {
            texUsage = texUsage | filament::Texture::Usage::GEN_MIPMAPPABLE;
        }
        mTexture = filament::Texture::Builder()
            .width(width)
            .height(height)
            .levels(levels)
            .format(fmt)
            .sampler(filament::Texture::Sampler::SAMPLER_2D)
            .usage(texUsage)
            .build(*engine);
    }

    using namespace filament::backend;
    auto upload = [](void *buf, uint32_t w, uint32_t h) {
        return PixelBufferDescriptor(
            buf, size_t(w) * h * 4,
            PixelDataFormat::RGBA,
            PixelDataType::UBYTE,
            [](void *p, size_t, void *) {
                delete[] static_cast<uint8_t *>(p);
            });
    };
    // Build the chain before any setImage. The upload callback can free a
    // level as soon as that call returns.
    uint8_t *levelData[16] = {};
    uint32_t levelW[16] = {};
    uint32_t levelH[16] = {};
    levelData[0] = ownedData;
    levelW[0] = width;
    levelH[0] = height;
    uint8_t chain = 1;
    if (levels > 1 && reuse) {
        for (uint8_t level = 1; level < levels && level < 16; ++level) {
            uint32_t const nw = std::max(levelW[level - 1] >> 1, 1u);
            uint32_t const nh = std::max(levelH[level - 1] >> 1, 1u);
            auto *next = new uint8_t[size_t(nw) * nh * 4];
            downsampleBox(levelData[level - 1], levelW[level - 1], levelH[level - 1], next, nw, nh);
            levelData[level] = next;
            levelW[level] = nw;
            levelH[level] = nh;
            chain = uint8_t(level + 1);
        }
    }
    for (uint8_t level = 0; level < chain; ++level)
        mTexture->setImage(*engine, level, upload(levelData[level], levelW[level], levelH[level]));
    if (levels > 1 && !reuse)
        mTexture->generateMipmaps(*engine);

    markCommitted();
}

int Sampler::uvIndex() const
{
    int n = 0;
    if (mInAttribute == "attribute1"_s)
        n = 1;
    return n;
}

filament::math::mat3f Sampler::uvMatrix() const
{
    // ANARI inTransform is column-vector: uv' = (M * vec4(u, v, 0, 1)).xy.
    // Filament gltfio samples as (vec3(uv, 1) * mat3).xy, with translation in
    // the third component of the first two columns.
    const anari::math::mat4 &t = mTransform;
    return filament::math::mat3f(
        t[0][0], t[0][1], t[3][0],
        t[1][0], t[1][1], t[3][1],
        0.0f, 0.0f, 1.0f);
}

void Sampler::commitImage1D()
{
    filament::Engine * const engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    auto *imageArray = getParamObject<helium::Array1D>("image");
    if (!imageArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "image1D sampler requires 'image' parameter");
        return;
    }

    const Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);
    mWrapS = wrapFromString(getParamString("wrapMode", "clampToEdge"));
    mWrapT = mWrapS;

    const uint32_t width = uint32_t(imageArray->totalSize());
    const ANARIDataType type = imageArray->elementType();

    auto *ownedData = new uint8_t[width * 4];
    convertToRGBA8(ownedData, imageArray->data(), type, width);

    // Create as Nx1 2D texture
    mTexture = filament::Texture::Builder()
        .width(width)
        .height(1)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*engine);

    using namespace filament::backend;
    mTexture->setImage(*engine, 0,
        PixelBufferDescriptor(
            ownedData, width * 4,
            PixelDataFormat::RGBA,
            PixelDataType::UBYTE,
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint8_t *>(buf);
            }));

    markCommitted();
}

void Sampler::commitImage3D()
{
    filament::Engine * const engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    auto *imageArray = getParamObject<helium::Array3D>("image");
    if (!imageArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "image3D sampler requires 'image' parameter");
        return;
    }

    const Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);
    mWrapS = wrapFromString(getParamString("wrapMode1", "clampToEdge"));
    mWrapT = wrapFromString(getParamString("wrapMode2", "clampToEdge"));

    const anari::math::uint3 dims = imageArray->size();
    const uint32_t width = dims[0];
    const uint32_t height = dims[1];
    const uint32_t depth = dims[2];
    const ANARIDataType type = imageArray->elementType();
    const size_t numPixels = size_t(width) * height * depth;

    auto *ownedData = new uint8_t[numPixels * 4];
    convertToRGBA8(ownedData, imageArray->data(), type, numPixels);

    mTexture = filament::Texture::Builder()
        .width(width)
        .height(height)
        .depth(depth)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_3D)
        .build(*engine);

    using namespace filament::backend;
    mTexture->setImage(*engine, 0,
        PixelBufferDescriptor(
            ownedData, numPixels * 4,
            PixelDataFormat::RGBA,
            PixelDataType::UBYTE,
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint8_t *>(buf);
            }));

    markCommitted();
}

void Sampler::commitTransform()
{
    filament::Engine *engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    // Read the 4x4 transform matrix
    mTransform = getParam<anari::math::mat4>("transform",
        anari::math::mat4(anari::math::identity));

    markCommitted();
}

void Sampler::commitPrimitive()
{
    filament::Engine *engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    auto *colorArray = getParamObject<helium::Array1D>("array");
    if (!colorArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "primitive sampler requires 'array' parameter");
        return;
    }

    const uint64_t offset = getParam<uint64_t>("offset", 0);
    const ANARIDataType type = colorArray->elementType();
    const void *data = colorArray->data();
    const size_t totalElements = colorArray->totalSize();

    // Size of one element in bytes
    const size_t elemSize = anari::sizeOf(type);

    // Compute start pointer with byte offset
    const uint8_t *startPtr =
        static_cast<const uint8_t *>(data) + offset;
    const size_t availableBytes =
        totalElements * elemSize - size_t(offset);
    const size_t count = availableBytes / elemSize;

    mPrimitiveColors = Corrade::Containers::Array<filament::math::float4>{
        Corrade::NoInit, count};
    convertColors(mPrimitiveColors.data(), startPtr, type, count);

    markCommitted();
}

}
