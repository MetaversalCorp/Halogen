// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Light.h"
#include "Surface.h"

#include <helium/utility/IntrusivePtr.h>

namespace filament {
class Scene;
}

namespace AnariFilament {

struct World : public Object
{
    World(DeviceState *s);
    ~World() override;

    void commitParameters() override;

    filament::Scene *filamentScene() const { return mScene; }

private:
    filament::Scene *mScene = nullptr;
    std::vector<helium::IntrusivePtr<Surface>> mSurfaces;
    std::vector<helium::IntrusivePtr<Light>> mLights;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::World *, ANARI_WORLD);
