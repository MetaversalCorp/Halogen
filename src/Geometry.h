// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Aabb.h"
#include "Object.h"

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

    const Aabb &aabb() const { return mAabb; }

private:
    filament::VertexBuffer *mVertexBuffer = nullptr;
    filament::IndexBuffer *mIndexBuffer = nullptr;
    uint32_t mIndexCount = 0;
    bool mHasColors = false;
    Aabb mAabb;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Geometry *, ANARI_GEOMETRY);
