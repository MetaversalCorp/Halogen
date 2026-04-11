// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Instance.h"

#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Instance *);

namespace AnariFilament {

Instance::Instance(DeviceState *s)
    : Object(ANARI_INSTANCE, s) {}

void Instance::commitParameters()
{
    mGroup = getParamObject<Group>("group");

    // ANARI transform is a column-major 4x4 matrix
    using float4 = anari::math::float4;
    using mat4 = anari::math::mat4;

    mat4 identity = {
        float4{1, 0, 0, 0},
        float4{0, 1, 0, 0},
        float4{0, 0, 1, 0},
        float4{0, 0, 0, 1}
    };
    mat4 m4 = getParam<mat4>("transform", identity);

    // filament::math::mat4f is also column-major, same layout
    std::memcpy(&mTransform, &m4, sizeof(filament::math::mat4f));

    markCommitted();
}

}
