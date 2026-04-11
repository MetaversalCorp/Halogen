// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "DeviceState.h"

#include <filament/Engine.h>

namespace AnariFilament {

DeviceState::DeviceState(ANARIDevice d)
    : helium::BaseGlobalDeviceState(d) {}

DeviceState::~DeviceState()
{
    if (engine) {
        matteMaterial.reset();
        renderer.reset();
        filament::Engine::destroy(&engine);
    }
}

}
