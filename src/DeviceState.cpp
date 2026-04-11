// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "DeviceState.h"

#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/Renderer.h>

namespace AnariFilament {

DeviceState::DeviceState(ANARIDevice d)
    : helium::BaseGlobalDeviceState(d) {}

DeviceState::~DeviceState()
{
    if (engine) {
        if (matteMaterial)
            engine->destroy(matteMaterial);
        if (renderer)
            engine->destroy(renderer);
        filament::Engine::destroy(&engine);
    }
}

}
