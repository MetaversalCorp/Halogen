// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Sampler.h"

#include "ColorConversion.h"

#include <filament/Engine.h>
#include <filament/Texture.h>

#include <backend/PixelBufferDescriptor.h>

#include <helium/array/Array1D.h>
#include <helium/array/Array2D.h>
#include <helium/array/Array3D.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

#include <algorithm>

using namespace Corrade::Containers::Literals;

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Sampler *);

namespace AnariFilament {

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
    filament::Engine *engine = deviceState()->engine;

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

    Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);

    anari::math::uint2 dims = imageArray->size();
    uint32_t width = dims[0];
    uint32_t height = dims[1];
    ANARIDataType type = imageArray->elementType();
    size_t numPixels = static_cast<size_t>(width) * height;

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
    filament::Engine *engine = deviceState()->engine;

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

    Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);

    uint32_t width = static_cast<uint32_t>(imageArray->totalSize());
    ANARIDataType type = imageArray->elementType();

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
    filament::Engine *engine = deviceState()->engine;

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

    Corrade::Containers::String filterStr = getParamString("filter", "linear");
    mNearest = (filterStr == "nearest"_s);

    anari::math::uint3 dims = imageArray->size();
    uint32_t width = dims[0];
    uint32_t height = dims[1];
    uint32_t depth = dims[2];
    ANARIDataType type = imageArray->elementType();
    size_t numPixels = static_cast<size_t>(width) * height * depth;

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

    uint64_t offset = getParam<uint64_t>("offset", 0);
    ANARIDataType type = colorArray->elementType();
    const void *data = colorArray->data();
    size_t totalElements = colorArray->totalSize();

    // Size of one element in bytes
    size_t elemSize = anari::sizeOf(type);

    // Compute start pointer with byte offset
    const uint8_t *startPtr =
        static_cast<const uint8_t *>(data) + offset;
    size_t availableBytes =
        totalElements * elemSize - static_cast<size_t>(offset);
    size_t count = availableBytes / elemSize;

    mPrimitiveColors = Corrade::Containers::Array<filament::math::float4>{
        Corrade::NoInit, count};
    convertColors(mPrimitiveColors.data(), startPtr, type, count);

    markCommitted();
}

}
