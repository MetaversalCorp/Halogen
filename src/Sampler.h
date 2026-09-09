// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

#include <anari/anari_cpp/ext/linalg.h>
#include <filament/TextureSampler.h>
#include <math/vec4.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/String.h>

namespace filament {
class Texture;
}

namespace Halogen {

struct Sampler : public Object
{
    Sampler(DeviceState *s, const char *subtype);
    ~Sampler() override;

    void commitParameters() override;

    filament::Texture *texture() const { return mTexture; }
    bool isNearest() const { return mNearest; }
    filament::TextureSampler::WrapMode wrapS() const { return mWrapS; }
    filament::TextureSampler::WrapMode wrapT() const { return mWrapT; }

    bool isTransform() const
    {
        using namespace Corrade::Containers::Literals;
        return mSubtype == "transform"_s;
    }

    bool isPrimitive() const
    {
        using namespace Corrade::Containers::Literals;
        return mSubtype == "primitive"_s;
    }

    const anari::math::mat4 &colorTransform() const { return mTransform; }

    const Corrade::Containers::Array<filament::math::float4> &primitiveColors() const
    {
        return mPrimitiveColors;
    }

    size_t primitiveColorCount() const
    {
        return mPrimitiveColors.size();
    }

private:
    void commitImage2D();
    void commitImage1D();
    void commitImage3D();
    void commitTransform();
    void commitPrimitive();

    Corrade::Containers::String mSubtype;
    filament::Texture *mTexture = nullptr;
    bool mNearest = false;
    filament::TextureSampler::WrapMode mWrapS =
        filament::TextureSampler::WrapMode::CLAMP_TO_EDGE;
    filament::TextureSampler::WrapMode mWrapT =
        filament::TextureSampler::WrapMode::CLAMP_TO_EDGE;
    anari::math::mat4 mTransform{anari::math::identity};
    Corrade::Containers::Array<filament::math::float4> mPrimitiveColors;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Sampler *, ANARI_SAMPLER);
