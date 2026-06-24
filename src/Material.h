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

// Extension: HALOGEN_MATERIAL_UNLIT (advertised via getDeviceExtensions).
//
// Adds an "unlit" material subtype alongside the core ANARI "matte" and
// "physicallyBased" subtypes. An unlit surface emits its color directly,
// independent of scene lighting -- the correct model for UI panels, HUD
// chrome, text labels, and vector line-art submitted as scene geometry.
// It is far lighter than coercing a physicallyBased material into faking
// unlit via an emissive map.
//
// Discovery (portable clients):
//   const char **ext = anariGetDeviceExtensions(library, "default");
//   // use "unlit" only when "HALOGEN_MATERIAL_UNLIT" is listed; otherwise
//   // fall back to "matte"/"physicallyBased".
//
// Usage:
//   ANARIMaterial m = anariNewMaterial(device, "unlit");
//   anariSetParameter(device, m, "color", ANARI_SAMPLER, &tex);   // or float4
//   anariSetParameter(device, m, "opacity", ANARI_FLOAT32, &a);
//   anariSetParameter(device, m, "alphaMode", ANARI_STRING, "blend");
//   anariCommitParameters(device, m);
//
// Parameters mirror "matte": "color" (float4 / image2D sampler / "color"
// vertex attribute), "opacity" (float), and "alphaMode" ("opaque" / "blend"
// / "mask"). The matte-only "transform" and "primitive" color samplers are
// not supported by unlit. Final color = color * vertexColor * opacity.
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
