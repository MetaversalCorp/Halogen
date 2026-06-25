// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Geometry.h"
#include "Material.h"

#include <helium/utility/ChangeObserverPtr.h>
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

    Geometry *geometry() const { return mGeometry.get(); }
    Material *material() const { return mMaterial.get(); }

private:
    void buildPrimitiveSamplerBuffers();

    utils::Entity mEntity;
    helium::ChangeObserverPtr<Geometry> mGeometry;
    helium::ChangeObserverPtr<Material> mMaterial;
    bool mBuilt = false;
    filament::VertexBuffer *mOwnedVB = nullptr;
    filament::IndexBuffer *mOwnedIB = nullptr;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Surface *, ANARI_SURFACE);
