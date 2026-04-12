// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Material.h"

#include "MathConversions.h"
#include "Sampler.h"

#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>

#include <math/mat4.h>

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
        Corrade::Containers::String alphaMode =
            getParamString("alphaMode", "opaque");
        if (alphaMode == "blend"_s)
            baseMaterial = state->physicallyBasedBlendMaterial.get();
        else if (alphaMode == "mask"_s)
            baseMaterial = state->physicallyBasedMaskedMaterial.get();
        else
            baseMaterial = state->physicallyBasedMaterial.get();
    } else {
        Corrade::Containers::String alphaMode =
            getParamString("alphaMode", "opaque");
        if (alphaMode == "blend"_s)
            baseMaterial = state->matteBlendMaterial.get();
        else if (alphaMode == "mask"_s)
            baseMaterial = state->matteMaskedMaterial.get();
        else
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
    mUsesPrimitiveSampler = false;

    bool isMatte = (mSubtype != "physicallyBased"_s);
    Corrade::Containers::String colorStr = getParamString("color", "");
    if (isMatte && mColorSampler && mColorSampler->isTransform()) {
        // Transform sampler: apply 4x4 matrix to UV in the shader
        mUsesVertexColors = false;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        mMaterialInstance->setParameter("hasBaseColorMap", false);
        mMaterialInstance->setParameter("hasColorTransform", true);
        const anari::math::mat4 &t = mColorSampler->colorTransform();
        filament::math::mat4f mat(
            filament::math::float4{t[0][0], t[0][1], t[0][2], t[0][3]},
            filament::math::float4{t[1][0], t[1][1], t[1][2], t[1][3]},
            filament::math::float4{t[2][0], t[2][1], t[2][2], t[2][3]},
            filament::math::float4{t[3][0], t[3][1], t[3][2], t[3][3]});
        mMaterialInstance->setParameter("colorTransform", mat);
    } else if (isMatte && mColorSampler && mColorSampler->isPrimitive()) {
        // Primitive sampler: expand per-primitive colors to per-vertex
        mUsesVertexColors = true;
        mUsesPrimitiveSampler = true;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        mMaterialInstance->setParameter("hasBaseColorMap", false);
        mMaterialInstance->setParameter("hasColorTransform", false);
    } else if (mColorSampler && mColorSampler->texture()) {
        mUsesVertexColors = false;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        mMaterialInstance->setParameter("hasBaseColorMap", true);
        if (isMatte)
            mMaterialInstance->setParameter("hasColorTransform", false);

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
        if (isMatte)
            mMaterialInstance->setParameter("hasColorTransform", false);
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
        if (isMatte)
            mMaterialInstance->setParameter("hasColorTransform", false);
    }

    // Opacity
    float opacity = getParam<float>("opacity", 1.0f);
    mMaterialInstance->setParameter("opacity", opacity);

    // Bind dummy texture to unused sampler parameters (required by Metal)
    filament::TextureSampler dummySampler;
    filament::Texture *dummy = state->dummyTexture;
    if (!mColorSampler || !mColorSampler->texture())
        mMaterialInstance->setParameter("baseColorMap", dummy, dummySampler);

    // Alpha cutoff for masked mode
    Corrade::Containers::String alphaMode =
        getParamString("alphaMode", "opaque");
    if (alphaMode == "mask"_s) {
        float alphaCutoff = getParam<float>("alphaCutoff", 0.5f);
        mMaterialInstance->setMaskThreshold(alphaCutoff);
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

        // Emissive color
        using float3 = anari::math::float3;
        float3 emissive = getParam<float3>(
            "emissive", float3(0.0f, 0.0f, 0.0f));
        mMaterialInstance->setParameter("emissive",
            filament::math::float3{emissive[0], emissive[1], emissive[2]});

        // Normal map
        mNormalSampler = getParamObject<Sampler>("normal");
        if (mNormalSampler && mNormalSampler->texture()) {
            mMaterialInstance->setParameter("hasNormalMap", true);
            filament::TextureSampler normalSampler(
                filament::TextureSampler::MagFilter::LINEAR);
            mMaterialInstance->setParameter("normalMap",
                mNormalSampler->texture(), normalSampler);
        } else {
            mNormalSampler = nullptr;
            mMaterialInstance->setParameter("hasNormalMap", false);
            mMaterialInstance->setParameter("normalMap", dummy, dummySampler);
        }
    }

    markCommitted();
}

}
