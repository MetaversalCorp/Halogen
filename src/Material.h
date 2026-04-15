// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"
#include "Sampler.h"

#include <Corrade/Containers/String.h>
#include <helium/utility/IntrusivePtr.h>

namespace filament {
class MaterialInstance;
}

namespace Halogen {

struct Material : public Object
{
    Material(DeviceState *s, const char *subtype);
    ~Material() override;

    void commitParameters() override;

    filament::MaterialInstance *materialInstance() const
    {
        return mMaterialInstance;
    }

    bool usesVertexColors() const { return mUsesVertexColors; }
    bool usesTexture() const { return static_cast<bool>(mColorSampler); }
    bool usesPrimitiveSampler() const { return mUsesPrimitiveSampler; }

    Sampler *colorSampler() const { return mColorSampler.ptr; }
    Sampler *normalSampler() const { return mNormalSampler.ptr; }

private:
    Corrade::Containers::String mSubtype;
    filament::MaterialInstance *mMaterialInstance = nullptr;
    helium::IntrusivePtr<Sampler> mColorSampler;
    helium::IntrusivePtr<Sampler> mNormalSampler;
    bool mUsesVertexColors = false;
    bool mUsesPrimitiveSampler = false;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Material *, ANARI_MATERIAL);
