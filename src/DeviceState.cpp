// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "DeviceState.h"

#include <filament/Engine.h>
#include <filament/Texture.h>

namespace Halogen {

DeviceState::DeviceState(ANARIDevice d)
    : helium::BaseGlobalDeviceState(d) {}

DeviceState::~DeviceState()
{
    if (engine) {
        // Ensure all pending GPU operations (readPixels, buffer uploads, etc.)
        // complete before destroying any Filament resources.
        engine->flushAndWait();

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
