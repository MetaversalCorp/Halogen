// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Sampler.h"

#include "ColorConversion.h"
#include "CompressedFormats.h"

#include <filament/Engine.h>
#include <filament/Texture.h>

#include <backend/PixelBufferDescriptor.h>
#include <backend/DriverEnums.h>

#include <helium/array/Array1D.h>
#include <helium/array/Array2D.h>
#include <helium/array/Array3D.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

#include <algorithm>
#include <cstring>

using namespace Corrade::Containers::Literals;

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
    else if (mSubtype == "compressedImage2D"_s)
        commitCompressedImage2D();
    else if (mSubtype == "transform"_s)
        commitTransform();
    else if (mSubtype == "primitive"_s)
        commitPrimitive();
    else
        commitImage2D();
}

void Sampler::commitCompressedImage2D()
{
    filament::Engine * const engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    auto *imageArray = getParamObject<helium::Array1D>("image");
    if (!imageArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "compressedImage2D sampler requires 'image' parameter");
        return;
    }

    const Corrade::Containers::String formatStr = getParamString("format", "");
    const CompressedFormat * const fmt = findCompressedFormat(formatStr);
    if (!fmt) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "compressedImage2D sampler: unsupported 'format'");
        return;
    }
    // Reject formats the GPU cannot accept, so we fail here with a clear
    // message rather than deep inside Filament. This mirrors the set reported
    // by the 'halogen.textureFormats' device property.
    if (!filament::Texture::isTextureFormatSupported(*engine, fmt->internal)) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "compressedImage2D sampler: 'format' not supported by this GPU");
        return;
    }

    // The extension types 'size' as UINT64_VEC2; texture dimensions never
    // exceed 32 bits and ANARI's linalg has no u64vec2, so it crosses the wire
    // as UINT32_VEC2 (matched on the Sneeze producer side).
    const anari::math::uint2 dims =
        getParam<anari::math::uint2>("size", anari::math::uint2(0u, 0u));
    const uint32_t width = dims[0];
    const uint32_t height = dims[1];
    if (width == 0 || height == 0) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "compressedImage2D sampler requires a non-zero 'size'");
        return;
    }

    const Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);

    // 'image' is an ARRAY1D of UINT8/INT8 -- element count equals byte count.
    const uint32_t byteCount = uint32_t(imageArray->totalSize());
    auto *ownedData = new uint8_t[byteCount];
    std::memcpy(ownedData, imageArray->data(), byteCount);

    mTexture = filament::Texture::Builder()
        .width(width)
        .height(height)
        .levels(1)
        .format(fmt->internal)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*engine);

    using namespace filament::backend;
    mTexture->setImage(*engine, 0,
        PixelBufferDescriptor(
            ownedData, byteCount,
            fmt->compressed, byteCount,
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint8_t *>(buf);
            }));

    markCommitted();
}

void Sampler::commitImage2D()
{
    filament::Engine * const engine = deviceState()->engine;

    if (mTexture) {
        engine->destroy(mTexture);
        mTexture = nullptr;
    }

    auto *imageArray = getParamObject<helium::Array2D>("image");
    if (!imageArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "image2D sampler requires 'image' parameter");
        return;
    }

    const Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);

    const anari::math::uint2 dims = imageArray->size();
    const uint32_t width = dims[0];
    const uint32_t height = dims[1];
    const ANARIDataType type = imageArray->elementType();
    const size_t numPixels = size_t(width) * height;

    auto *ownedData = new uint8_t[numPixels * 4];
    convertToRGBA8(ownedData, imageArray->data(), type, numPixels);

    mTexture = filament::Texture::Builder()
        .width(width)
        .height(height)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
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
