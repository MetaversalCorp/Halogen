// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

namespace filament {
class MaterialInstance;
}

namespace AnariFilament {

struct Material : public Object
{
    Material(DeviceState *s);
    ~Material() override;

    void commitParameters() override;

    filament::MaterialInstance *materialInstance() const
    {
        return mMaterialInstance;
    }

    bool usesVertexColors() const { return mUsesVertexColors; }

private:
    filament::MaterialInstance *mMaterialInstance = nullptr;
    bool mUsesVertexColors = false;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Material *, ANARI_MATERIAL);
