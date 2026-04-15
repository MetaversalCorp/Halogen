// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "FilamentResource.h"
#include "Object.h"

#include <Corrade/Containers/String.h>
#include <utils/Entity.h>

namespace filament {
class Camera;
}

namespace Halogen {

struct Camera : public Object
{
    Camera(DeviceState *s, const char *subtype);
    ~Camera() override;

    void commitParameters() override;

    filament::Camera *filamentCamera() const { return mCamera.get(); }
    utils::Entity entity() const { return mEntity; }

private:
    Corrade::Containers::String mSubtype;
    utils::Entity mEntity;
    FilamentResource<filament::Camera> mCamera;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Camera *, ANARI_CAMERA);
