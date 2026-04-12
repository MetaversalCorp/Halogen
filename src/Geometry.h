// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Aabb.h"
#include "Object.h"

#include <Corrade/Containers/String.h>

namespace filament {
class VertexBuffer;
class IndexBuffer;
}

namespace AnariFilament {

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

private:
    void commitTriangle();
    void commitSphere();
    void commitCylinder();

    Corrade::Containers::String mSubtype;
    filament::VertexBuffer *mVertexBuffer = nullptr;
    filament::IndexBuffer *mIndexBuffer = nullptr;
    uint32_t mIndexCount = 0;
    bool mHasColors = false;
    bool mHasUV0 = false;
    bool mHasUV1 = false;
    Aabb mAabb;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Geometry *, ANARI_GEOMETRY);
