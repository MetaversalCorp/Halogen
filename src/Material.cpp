// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Material.h"

#include "MathConversions.h"

#include <filament/Material.h>
#include <filament/MaterialInstance.h>

#include <Corrade/Containers/StringStl.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Material *);

namespace AnariFilament {

Material::Material(DeviceState *s, const char *subtype)
    : Object(ANARI_MATERIAL, s)
    , mSubtype(subtype ? subtype : "matte") {}

Material::~Material()
{
    filament::Engine *engine = deviceState()->engine;
    if (mMaterialInstance)
        engine->destroy(mMaterialInstance);
}

void Material::commitParameters()
{
    DeviceState *state = deviceState();

    if (mMaterialInstance) {
        state->engine->destroy(mMaterialInstance);
        mMaterialInstance = nullptr;
    }

    filament::Material *baseMaterial = nullptr;
    if (mSubtype == "physicallyBased") {
        baseMaterial = state->physicallyBasedMaterial.get();
    } else {
        baseMaterial = state->matteMaterial.get();
    }

    if (!baseMaterial) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "%s material not compiled", mSubtype.data());
        return;
    }

    mMaterialInstance = baseMaterial->createInstance();

    Corrade::Containers::String colorStr = getParamString("color", "");
    if (colorStr == "color") {
        mUsesVertexColors = true;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
    } else {
        mUsesVertexColors = false;
        using float3 = anari::math::float3;
        using float4 = anari::math::float4;

        float4 c4 = getParam<float4>("color", float4(1.0f, 1.0f, 1.0f, 1.0f));
        float3 c3 = getParam<float3>("color", float3(c4[0], c4[1], c4[2]));
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{c3[0], c3[1], c3[2], c4[3]});
    }

    if (mSubtype == "physicallyBased") {
        float metallic = getParam<float>("metallic", 1.0f);
        float roughness = getParam<float>("roughness", 1.0f);
        mMaterialInstance->setParameter("metallic", metallic);
        mMaterialInstance->setParameter("roughness", roughness);
    }

    markCommitted();
}

}
