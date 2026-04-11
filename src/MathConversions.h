// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <math/vec3.h>
#include <math/vec4.h>

#include <anari/anari_cpp/ext/linalg.h>

namespace AnariFilament {

// Zero-copy conversions between anari::math and filament::math types.
// Both families are standard-layout structs of contiguous floats, so
// reinterpret_cast is safe for references.

inline const filament::math::float3 &toFilament(
    const anari::math::float3 &v)
{
    return reinterpret_cast<const filament::math::float3 &>(v);
}

inline const filament::math::float4 &toFilament(
    const anari::math::float4 &v)
{
    return reinterpret_cast<const filament::math::float4 &>(v);
}

inline filament::math::double3 toDouble3(const anari::math::float3 &v)
{
    return {v[0], v[1], v[2]};
}

}
