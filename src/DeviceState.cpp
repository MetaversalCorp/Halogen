// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "DeviceState.h"

#include <filament/Engine.h>
#include <filament/Texture.h>

namespace AnariFilament {

DeviceState::DeviceState(ANARIDevice d)
    : helium::BaseGlobalDeviceState(d) {}

DeviceState::~DeviceState()
{
    if (engine) {
        physicallyBasedMaskedMaterial.reset();
        physicallyBasedBlendMaterial.reset();
        physicallyBasedMaterial.reset();
        matteMaskedMaterial.reset();
        matteBlendMaterial.reset();
        matteMaterial.reset();
        if (dummyTexture) {
            engine->destroy(dummyTexture);
            dummyTexture = nullptr;
        }
        renderer.reset();
        filament::Engine::destroy(&engine);
    }
}

}
