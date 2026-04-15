// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Instance.h"
#include "Light.h"
#include "Surface.h"

#include <Corrade/Containers/Array.h>
#include <helium/utility/IntrusivePtr.h>
#include <utils/Entity.h>

namespace filament {
class Scene;
}

namespace Halogen {

struct World : public Object
{
    World(DeviceState *s);
    ~World() override;

    void commitParameters() override;
    void finalize() override;

    filament::Scene *filamentScene() const { return mScene; }

private:
    void clearInstanceEntities();

    filament::Scene *mScene = nullptr;
    Corrade::Containers::Array<helium::IntrusivePtr<Surface>> mSurfaces;
    Corrade::Containers::Array<helium::IntrusivePtr<Light>> mLights;
    Corrade::Containers::Array<helium::IntrusivePtr<Instance>> mInstances;

    // Entities created for instanced surfaces (owned by this World)
    Corrade::Containers::Array<utils::Entity> mInstanceEntities;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::World *, ANARI_WORLD);
