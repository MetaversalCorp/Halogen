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

namespace AnariFilament {

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

private:
    Corrade::Containers::String mSubtype;
    filament::MaterialInstance *mMaterialInstance = nullptr;
    helium::IntrusivePtr<Sampler> mColorSampler;
    bool mUsesVertexColors = false;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Material *, ANARI_MATERIAL);
