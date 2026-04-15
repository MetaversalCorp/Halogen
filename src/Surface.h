// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Geometry.h"
#include "Material.h"

#include <helium/utility/IntrusivePtr.h>
#include <utils/Entity.h>

namespace filament {
class VertexBuffer;
class IndexBuffer;
}

namespace Halogen {

struct Surface : public Object
{
    Surface(DeviceState *s);
    ~Surface() override;

    void commitParameters() override;
    void finalize() override;

    utils::Entity entity() const { return mEntity; }
    bool isValid() const override;

    Geometry *geometry() const { return mGeometry.ptr; }
    Material *material() const { return mMaterial.ptr; }

private:
    void buildPrimitiveSamplerBuffers();

    utils::Entity mEntity;
    helium::IntrusivePtr<Geometry> mGeometry;
    helium::IntrusivePtr<Material> mMaterial;
    bool mBuilt = false;
    filament::VertexBuffer *mOwnedVB = nullptr;
    filament::IndexBuffer *mOwnedIB = nullptr;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Surface *, ANARI_SURFACE);
