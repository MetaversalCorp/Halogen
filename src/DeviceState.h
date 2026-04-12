// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "FilamentResource.h"

#include <helium/BaseGlobalDeviceState.h>

namespace filament {
class Engine;
class Renderer;
class Material;
}

namespace AnariFilament {

struct DeviceState : public helium::BaseGlobalDeviceState
{
    filament::Engine *engine = nullptr;
    FilamentResource<filament::Renderer> renderer;
    FilamentResource<filament::Material> matteMaterial;
    FilamentResource<filament::Material> matteBlendMaterial;
    FilamentResource<filament::Material> matteMaskedMaterial;
    FilamentResource<filament::Material> physicallyBasedMaterial;
    FilamentResource<filament::Material> physicallyBasedBlendMaterial;
    FilamentResource<filament::Material> physicallyBasedMaskedMaterial;

    DeviceState(ANARIDevice d);
    ~DeviceState() override;
};

inline DeviceState *asFilamentState(helium::BaseGlobalDeviceState *s)
{
    return static_cast<DeviceState *>(s);
}

#define ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(type, anari_type) \
    namespace anari {                                           \
    ANARI_TYPEFOR_SPECIALIZATION(type, anari_type);             \
    }

#define ANARI_FILAMENT_TYPEFOR_DEFINITION(type) \
    namespace anari {                           \
    ANARI_TYPEFOR_DEFINITION(type);             \
    }

}
