// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "FilamentResource.h"
#include "Object.h"

#include <utils/Entity.h>

namespace filament {
class Camera;
}

namespace AnariFilament {

struct Camera : public Object
{
    Camera(DeviceState *s);
    ~Camera() override;

    void commitParameters() override;

    filament::Camera *filamentCamera() const { return mCamera.get(); }
    utils::Entity entity() const { return mEntity; }

private:
    utils::Entity mEntity;
    FilamentResource<filament::Camera> mCamera;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Camera *, ANARI_CAMERA);
