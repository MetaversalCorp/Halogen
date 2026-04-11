// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Material.h"

#include <filament/Material.h>
#include <filament/MaterialInstance.h>

#include <anari/anari_cpp/ext/linalg.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Material *);

namespace AnariFilament {

Material::Material(DeviceState *s)
    : Object(ANARI_MATERIAL, s) {}

Material::~Material()
{
    auto *engine = deviceState()->engine;
    if (mMaterialInstance)
        engine->destroy(mMaterialInstance);
}

void Material::commitParameters()
{
    auto *state = deviceState();

    if (mMaterialInstance) {
        state->engine->destroy(mMaterialInstance);
        mMaterialInstance = nullptr;
    }

    if (!state->matteMaterial) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "matte material not compiled");
        return;
    }

    mMaterialInstance = state->matteMaterial->createInstance();

    auto colorStr = getParamString("color", "");
    if (colorStr == "color") {
        mUsesVertexColors = true;
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{1.0f, 1.0f, 1.0f, 1.0f});
    } else {
        mUsesVertexColors = false;
        using float3 = anari::math::float3;
        using float4 = anari::math::float4;

        auto c4 = getParam<float4>("color", float4(1.0f, 1.0f, 1.0f, 1.0f));
        auto c3 = getParam<float3>("color", float3(c4[0], c4[1], c4[2]));
        mMaterialInstance->setParameter("baseColor",
            filament::math::float4{c3[0], c3[1], c3[2], c4[3]});
    }

    markCommitted();
}

}
