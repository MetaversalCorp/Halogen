// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

#include <anari/anari_cpp/ext/linalg.h>

namespace AnariFilament {

struct Renderer : public Object
{
    Renderer(DeviceState *s)
        : Object(ANARI_RENDERER, s) {}

    void commitParameters() override
    {
        using float4 = anari::math::float4;
        mBackgroundColor = getParam<float4>(
            "background", float4(0.0f, 0.0f, 0.0f, 1.0f));

        using float3 = anari::math::float3;
        mAmbientColor = getParam<float3>(
            "ambientColor", float3(1.0f, 1.0f, 1.0f));
        mAmbientRadiance = getParam<float>("ambientRadiance", 0.0f);

        markCommitted();
    }

    anari::math::float4 backgroundColor() const { return mBackgroundColor; }
    anari::math::float3 ambientColor() const { return mAmbientColor; }
    float ambientRadiance() const { return mAmbientRadiance; }

private:
    anari::math::float4 mBackgroundColor{0.0f, 0.0f, 0.0f, 1.0f};
    anari::math::float3 mAmbientColor{1.0f, 1.0f, 1.0f};
    float mAmbientRadiance = 0.0f;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Renderer *, ANARI_RENDERER);
