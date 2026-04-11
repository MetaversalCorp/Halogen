// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

#include <utils/Entity.h>

namespace AnariFilament {

struct Light : public Object
{
    Light(DeviceState *s);
    ~Light() override;

    void commitParameters() override;

    utils::Entity entity() const { return mEntity; }

private:
    utils::Entity mEntity;
    bool mBuilt = false;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Light *, ANARI_LIGHT);
