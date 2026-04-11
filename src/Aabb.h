// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <math/vec3.h>

#include <algorithm>
#include <limits>

namespace AnariFilament {

struct Aabb {
    filament::math::float3 min = {
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max(),
        std::numeric_limits<float>::max()};
    filament::math::float3 max = {
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest(),
        std::numeric_limits<float>::lowest()};

    void extend(filament::math::float3 p)
    {
        min.x = std::min(min.x, p.x);
        min.y = std::min(min.y, p.y);
        min.z = std::min(min.z, p.z);
        max.x = std::max(max.x, p.x);
        max.y = std::max(max.y, p.y);
        max.z = std::max(max.z, p.z);
    }

    filament::math::float3 center() const
    {
        return (min + max) * 0.5f;
    }

    filament::math::float3 halfExtent() const
    {
        return (max - min) * 0.5f;
    }
};

inline Aabb computeAabb(
    const filament::math::float3 *positions, uint32_t count)
{
    Aabb aabb;
    for (uint32_t i = 0; i < count; ++i)
        aabb.extend(positions[i]);
    return aabb;
}

}
