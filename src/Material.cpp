// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Material.h"

#include "MathConversions.h"
#include "Sampler.h"


#include <filament/Material.h>
#include <filament/MaterialInstance.h>
#include <filament/Texture.h>
#include <filament/TextureSampler.h>

#include <math/mat3.h>
#include <math/mat4.h>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

using namespace Corrade::Containers::Literals;

namespace {

filament::TextureSampler filamentSamplerFrom(const Halogen::Sampler &sampler)
{
    using Min = filament::TextureSampler::MinFilter;
    using Mag = filament::TextureSampler::MagFilter;
    filament::TextureSampler out(
        sampler.isNearest()
            ? Min::NEAREST
            : Min::LINEAR_MIPMAP_LINEAR,
        sampler.isNearest()
            ? Mag::NEAREST
            : Mag::LINEAR);
    out.setWrapModeS(sampler.wrapS());
    out.setWrapModeT(sampler.wrapT());
    return out;
}

int uvIndexOf(const Halogen::Sampler *sampler)
{
    int n = 0;
    if (sampler)
        n = sampler->uvIndex();
    return n;
}

filament::math::mat3f uvMatrixOf(const Halogen::Sampler *sampler)
{
    filament::math::mat3f m;
    if (sampler)
        m = sampler->uvMatrix();
    return m;
}

void bindTextureMap(filament::MaterialInstance *mi,
    const char *mapName,
    const char *hasName,
    const char *uvName,
    const char *matName,
    Halogen::Sampler *sampler,
    filament::Texture *dummy,
    const filament::TextureSampler &dummySampler)
{
    if (sampler && sampler->texture()) {
        mi->setParameter(hasName, true);
        mi->setParameter(mapName, sampler->texture(),
            filamentSamplerFrom(*sampler));
        mi->setParameter(uvName, uvIndexOf(sampler));
        mi->setParameter(matName, uvMatrixOf(sampler));
    } else {
        mi->setParameter(hasName, false);
        mi->setParameter(mapName, dummy, dummySampler);
        mi->setParameter(uvName, 0);
        mi->setParameter(matName, filament::math::mat3f());
    }
}

}

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Material *);

namespace Halogen {

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
    DeviceState * const state = deviceState();

    if (mMaterialInstance) {
        state->engine->destroy(mMaterialInstance);
        mMaterialInstance = nullptr;
    }

    filament::Material *baseMaterial = nullptr;
    const Corrade::Containers::String alphaMode =
        getParamString("alphaMode", "opaque");
    // glTF BLEND is unpremultiplied. Filament "transparent" expects
    // premultiplied RGB and does not fade specular, so white RGB with
    // alpha 0 adds full lighting (a white wash). Filament gltfio maps
    // glTF BLEND to FADE; the *Blend.mat files use blending : fade.
    if (mSubtype == "physicallyBased"_s) {
        if (alphaMode == "blend"_s)
            baseMaterial = state->physicallyBasedBlendMaterial.get();
        else if (alphaMode == "mask"_s)
            baseMaterial = state->physicallyBasedMaskedMaterial.get();
        else
            baseMaterial = state->physicallyBasedMaterial.get();
    } else if (mSubtype == "unlit"_s) {
        if (alphaMode == "blend"_s)
            baseMaterial = state->unlitBlendMaterial.get();
        else if (alphaMode == "mask"_s)
            baseMaterial = state->unlitMaskedMaterial.get();
        else
            baseMaterial = state->unlitMaterial.get();
    } else {
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

    // The ANARI spec uses "color" for matte materials and "baseColor" for
    // physicallyBased materials.  The HALOGEN_MATERIAL_UNLIT extension follows
    // matte and reads "color".  isMatte strictly gates the matte-only
    // colorTransform / primitive-sampler paths, whose parameters the
    // lightweight unlit shader does not declare.
    const bool isPbr = (mSubtype == "physicallyBased"_s);
    const bool isUnlit = (mSubtype == "unlit"_s);
    const bool isMatte = (!isPbr && !isUnlit);
    const char *const colorKey = isPbr ? "baseColor" : "color";

    // Check for sampler-based color (texture)
    mColorSampler = getParamObject<Sampler>(colorKey);
    mUsesPrimitiveSampler = false;

    const Corrade::Containers::String colorStr = getParamString(colorKey, "");
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
        // glTF multiplies baseColorFactor * texture. Keep the factor when a
        // map is bound (physicallyBased only -- unlit/matte have no factor
        // slot). Default white matches callers that still bake the factor.
        if (isPbr) {
            using float4 = anari::math::float4;
            const float4 factor = getParam<float4>(
                "baseColorFactor", float4(1.0f, 1.0f, 1.0f, 1.0f));
            mMaterialInstance->setParameter("baseColor",
                filament::math::float4{factor[0], factor[1], factor[2], factor[3]});
        } else {
            mMaterialInstance->setParameter("baseColor",
                filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
        }
        mMaterialInstance->setParameter("hasBaseColorMap", true);
        if (isMatte)
            mMaterialInstance->setParameter("hasColorTransform", false);

        filament::TextureSampler sampler = filamentSamplerFrom(*mColorSampler);
        mMaterialInstance->setParameter("baseColorMap",
            mColorSampler->texture(), sampler);
        if (isPbr) {
            mMaterialInstance->setParameter("baseColorUV",
                uvIndexOf(mColorSampler.ptr));
            mMaterialInstance->setParameter("baseColorUvMatrix",
                uvMatrixOf(mColorSampler.ptr));
        }
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

        const float4 c4 = getParam<float4>(colorKey, float4(1.0f, 1.0f, 1.0f, 1.0f));
        const float3 c3 = getParam<float3>(colorKey, float3(c4[0], c4[1], c4[2]));
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{c3[0], c3[1], c3[2], c4[3]});
        mMaterialInstance->setParameter("hasBaseColorMap", false);
        if (isMatte)
            mMaterialInstance->setParameter("hasColorTransform", false);
    }

    // Opacity
    const float opacity = getParam<float>("opacity", 1.0f);
    mMaterialInstance->setParameter("opacity", opacity);

    // Bind dummy texture to unused sampler parameters (required by Metal)
    filament::TextureSampler dummySampler;
    filament::Texture *dummy = state->dummyTexture;
    if (!mColorSampler || !mColorSampler->texture())
        mMaterialInstance->setParameter("baseColorMap", dummy, dummySampler);

    // Alpha cutoff for masked mode
    if (alphaMode == "mask"_s) {
        const float alphaCutoff = getParam<float>("alphaCutoff", 0.5f);
        mMaterialInstance->setMaskThreshold(alphaCutoff);
    }

    // glTF / ANARI doubleSided. The .mat files are compiled with
    // doubleSided capability so two-sided lighting is in the shader;
    // culling is chosen per instance. Default false matches ANARI/glTF.
    const bool doubleSided = getParam<bool>("doubleSided", false);
    mMaterialInstance->setDoubleSided(doubleSided);
    mMaterialInstance->setCullingMode(doubleSided
            ? filament::MaterialInstance::CullingMode::NONE
            : filament::MaterialInstance::CullingMode::BACK);

    if (mSubtype == "physicallyBased"_s) {
        // Metallic: can be float or "attribute0"
        const Corrade::Containers::String metallicStr =
            getParamString("metallic", "");
        if (metallicStr == "attribute0"_s) {
            mMaterialInstance->setParameter("useAttribute0ForMetallic", true);
            mMaterialInstance->setParameter("metallic", 0.0f);
        } else {
            mMaterialInstance->setParameter("useAttribute0ForMetallic", false);
            const float metallic = getParam<float>("metallic", 1.0f);
            mMaterialInstance->setParameter("metallic", metallic);
        }

        // Roughness: can be float or "attribute1"
        const Corrade::Containers::String roughnessStr =
            getParamString("roughness", "");
        if (roughnessStr == "attribute1"_s) {
            mMaterialInstance->setParameter("useAttribute1ForRoughness", true);
            mMaterialInstance->setParameter("roughness", 0.0f);
        } else {
            mMaterialInstance->setParameter("useAttribute1ForRoughness", false);
            const float roughness = getParam<float>("roughness", 1.0f);
            mMaterialInstance->setParameter("roughness", roughness);
        }

        // Emissive: vec3 factor always, optional image2D sampler. glTF's
        // emissiveFactor * texture is done in the shader. Callers that still
        // bake the factor into the map leave emissiveFactor at white.
        mEmissiveSampler = getParamObject<Sampler>("emissive");
        if (mEmissiveSampler && mEmissiveSampler->texture()) {
            using float3 = anari::math::float3;
            const float3 factor = getParam<float3>(
                "emissiveFactor", float3(1.0f, 1.0f, 1.0f));
            mMaterialInstance->setParameter("emissive",
                filament::math::float3{factor[0], factor[1], factor[2]});
        } else {
            mEmissiveSampler = nullptr;
            using float3 = anari::math::float3;
            const float3 emissive = getParam<float3>(
                "emissive", float3(0.0f, 0.0f, 0.0f));
            mMaterialInstance->setParameter("emissive",
                filament::math::float3{emissive[0], emissive[1], emissive[2]});
        }
        bindTextureMap(mMaterialInstance,
            "emissiveMap", "hasEmissiveMap",
            "emissiveUV", "emissiveUvMatrix",
            mEmissiveSampler.ptr, dummy, dummySampler);

        if (!(mColorSampler && mColorSampler->texture())) {
            mMaterialInstance->setParameter("baseColorUV", 0);
            mMaterialInstance->setParameter("baseColorUvMatrix",
                filament::math::mat3f());
        }

        mMetallicRoughnessSampler =
            getParamObject<Sampler>("metallicRoughness");
        bindTextureMap(mMaterialInstance,
            "metallicRoughnessMap", "hasMetallicRoughnessMap",
            "metallicRoughnessUV", "metallicRoughnessUvMatrix",
            mMetallicRoughnessSampler.ptr, dummy, dummySampler);

        mNormalSampler = getParamObject<Sampler>("normal");
        bindTextureMap(mMaterialInstance,
            "normalMap", "hasNormalMap",
            "normalUV", "normalUvMatrix",
            mNormalSampler.ptr, dummy, dummySampler);
        mMaterialInstance->setParameter("normalScale",
            getParam<float>("normalScale", 1.0f));

        mOcclusionSampler = getParamObject<Sampler>("occlusion");
        bindTextureMap(mMaterialInstance,
            "occlusionMap", "hasOcclusionMap",
            "occlusionUV", "occlusionUvMatrix",
            mOcclusionSampler.ptr, dummy, dummySampler);
        mMaterialInstance->setParameter("occlusionStrength",
            getParam<float>("occlusionStrength", 1.0f));
    }

    markCommitted();
}

}
