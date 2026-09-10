// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Aabb.h"
#include "Object.h"

#include <Corrade/Containers/String.h>

#include <math/vec2.h>
#include <math/vec3.h>
#include <math/vec4.h>

namespace filament {
class Engine;
class VertexBuffer;
class IndexBuffer;
}

namespace Halogen {

struct Geometry : public Object
{
    Geometry(DeviceState *s, const char *subtype);
    ~Geometry() override;

    void commitParameters() override;

    filament::VertexBuffer *vertexBuffer() const { return mVertexBuffer; }
    filament::IndexBuffer *indexBuffer() const { return mIndexBuffer; }
    uint32_t indexCount() const { return mIndexCount; }
    bool hasVertexColors() const { return mHasColors; }
    bool hasUV0() const { return mHasUV0; }
    bool hasUV1() const { return mHasUV1; }

    const Aabb &aabb() const { return mAabb; }

    // Builds a VertexBuffer with POSITION alone in buffer 0 and
    // TANGENTS/COLOR/UV0/UV1 interleaved in buffer 1. Keeping POSITION separate
    // lets position-only passes (shadow maps, depth prepass) fetch a tight
    // 12-byte stride instead of pulling the full interleaved vertex. Every
    // input is copied synchronously, so the caller keeps ownership of all
    // pointers and may free them as soon as this returns. Pass nullptr for
    // colors/uv0/uv1 to fill defaults (white / zero). Shared with Surface's
    // primitive-sampler path.
    static filament::VertexBuffer *buildInterleavedVertexBuffer(
        filament::Engine *engine, uint32_t vertexCount,
        const filament::math::float3 *positions,
        const filament::math::short4 *tangents,
        const filament::math::float4 *colors,
        const filament::math::float2 *uv0,
        const filament::math::float2 *uv1);

private:
    void commitTriangle();
    void commitSphere();
    void commitCylinder();
    void commitCurve();
    void commitQuad();
    void commitCone();

    // Defer destruction of the current buffers by one commit generation so a
    // renderable that still references them survives the one-flush lag before
    // its Surface re-finalizes against the rebuilt buffers.
    void retireBuffers();

    Corrade::Containers::String mSubtype;
    filament::VertexBuffer *mVertexBuffer = nullptr;
    filament::IndexBuffer *mIndexBuffer = nullptr;
    filament::VertexBuffer *mPrevVertexBuffer = nullptr;
    filament::IndexBuffer *mPrevIndexBuffer = nullptr;
    uint32_t mIndexCount = 0;
    bool mHasColors = false;
    bool mHasUV0 = false;
    bool mHasUV1 = false;
    Aabb mAabb;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Geometry *, ANARI_GEOMETRY);
