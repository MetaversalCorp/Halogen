// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Sampler.h"

#include <filament/Engine.h>
#include <filament/Texture.h>

#include <backend/PixelBufferDescriptor.h>

#include <helium/array/Array2D.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStl.h>

#include <algorithm>
#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Sampler *);

namespace AnariFilament {

Sampler::Sampler(DeviceState *s)
    : Object(ANARI_SAMPLER, s) {}

Sampler::~Sampler()
{
    if (mTexture)
        deviceState()->engine->destroy(mTexture);
}

void Sampler::commitParameters()
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
    mNearest = (filterStr == "nearest");

    anari::math::uint2 dims = imageArray->size();
    uint32_t width = dims[0];
    uint32_t height = dims[1];
    ANARIDataType type = imageArray->elementType();

    size_t numPixels = static_cast<size_t>(width) * height;
    auto rgba = Corrade::Containers::Array<uint8_t>{
        Corrade::NoInit, numPixels * 4};

    const void *srcData = imageArray->data();

    if (type == ANARI_FLOAT32_VEC3) {
        const float *src = static_cast<const float *>(srcData);
        for (size_t i = 0; i < numPixels; ++i) {
            rgba[i * 4 + 0] = static_cast<uint8_t>(
                std::clamp(src[i * 3 + 0], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 1] = static_cast<uint8_t>(
                std::clamp(src[i * 3 + 1], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 2] = static_cast<uint8_t>(
                std::clamp(src[i * 3 + 2], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 3] = 255;
        }
    } else if (type == ANARI_FLOAT32_VEC4) {
        const float *src = static_cast<const float *>(srcData);
        for (size_t i = 0; i < numPixels; ++i) {
            rgba[i * 4 + 0] = static_cast<uint8_t>(
                std::clamp(src[i * 4 + 0], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 1] = static_cast<uint8_t>(
                std::clamp(src[i * 4 + 1], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 2] = static_cast<uint8_t>(
                std::clamp(src[i * 4 + 2], 0.0f, 1.0f) * 255.0f);
            rgba[i * 4 + 3] = static_cast<uint8_t>(
                std::clamp(src[i * 4 + 3], 0.0f, 1.0f) * 255.0f);
        }
    } else {
        reportMessage(ANARI_SEVERITY_ERROR,
            "unsupported image data type for sampler");
        return;
    }

    mTexture = filament::Texture::Builder()
        .width(width)
        .height(height)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*engine);

    auto *ownedData = new uint8_t[numPixels * 4];
    std::memcpy(ownedData, rgba.data(), numPixels * 4);

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

}
