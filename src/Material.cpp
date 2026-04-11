// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Material.h"

#include "MathConversions.h"
#include "Sampler.h"

#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

using namespace Corrade::Containers::Literals;

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
    if (mSubtype == "physicallyBased"_s) {
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

    // Check for sampler-based color (texture)
    mColorSampler = getParamObject<Sampler>("color");

    Corrade::Containers::String colorStr = getParamString("color", "");
    if (mColorSampler && mColorSampler->texture()) {
        mUsesVertexColors = false;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        mMaterialInstance->setParameter("hasBaseColorMap", true);

        filament::TextureSampler sampler(
            mColorSampler->isNearest()
                ? filament::TextureSampler::MagFilter::NEAREST
                : filament::TextureSampler::MagFilter::LINEAR);
        mMaterialInstance->setParameter("baseColorMap",
            mColorSampler->texture(), sampler);
    } else if (colorStr == "color"_s) {
        mUsesVertexColors = true;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        mMaterialInstance->setParameter("hasBaseColorMap", false);
    } else {
        mUsesVertexColors = false;
        mColorSampler = nullptr;
        using float3 = anari::math::float3;
        using float4 = anari::math::float4;

        float4 c4 = getParam<float4>("color", float4(1.0f, 1.0f, 1.0f, 1.0f));
        float3 c3 = getParam<float3>("color", float3(c4[0], c4[1], c4[2]));
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{c3[0], c3[1], c3[2], c4[3]});
        mMaterialInstance->setParameter("hasBaseColorMap", false);
    }

    if (mSubtype == "physicallyBased"_s) {
        // Metallic: can be float or "attribute0"
        Corrade::Containers::String metallicStr =
            getParamString("metallic", "");
        if (metallicStr == "attribute0"_s) {
            mMaterialInstance->setParameter("useAttribute0ForMetallic", true);
            mMaterialInstance->setParameter("metallic", 0.0f);
        } else {
            mMaterialInstance->setParameter("useAttribute0ForMetallic", false);
            float metallic = getParam<float>("metallic", 1.0f);
            mMaterialInstance->setParameter("metallic", metallic);
        }

        // Roughness: can be float or "attribute1"
        Corrade::Containers::String roughnessStr =
            getParamString("roughness", "");
        if (roughnessStr == "attribute1"_s) {
            mMaterialInstance->setParameter("useAttribute1ForRoughness", true);
            mMaterialInstance->setParameter("roughness", 0.0f);
        } else {
            mMaterialInstance->setParameter("useAttribute1ForRoughness", false);
            float roughness = getParam<float>("roughness", 1.0f);
            mMaterialInstance->setParameter("roughness", roughness);
        }
    }

    markCommitted();
}

}
