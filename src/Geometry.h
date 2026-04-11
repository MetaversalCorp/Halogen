// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

#include <math/vec3.h>

namespace filament {
class VertexBuffer;
class IndexBuffer;
}

namespace AnariFilament {

struct Geometry : public Object
{
    Geometry(DeviceState *s);
    ~Geometry() override;

    void commitParameters() override;

    filament::VertexBuffer *vertexBuffer() const { return mVertexBuffer; }
    filament::IndexBuffer *indexBuffer() const { return mIndexBuffer; }
    uint32_t indexCount() const { return mIndexCount; }
    bool hasVertexColors() const { return mHasColors; }

    filament::math::float3 aabbMin() const { return mAabbMin; }
    filament::math::float3 aabbMax() const { return mAabbMax; }

private:
    filament::VertexBuffer *mVertexBuffer = nullptr;
    filament::IndexBuffer *mIndexBuffer = nullptr;
    uint32_t mIndexCount = 0;
    bool mHasColors = false;
    filament::math::float3 mAabbMin = {0, 0, 0};
    filament::math::float3 mAabbMax = {0, 0, 0};
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Geometry *, ANARI_GEOMETRY);
