// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

namespace AnariFilament {

struct Renderer : public Object
{
    Renderer(DeviceState *s)
        : Object(ANARI_RENDERER, s) {}

    void commitParameters() override { markCommitted(); }
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Renderer *, ANARI_RENDERER);
