// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Camera.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <utils/EntityManager.h>

#include <anari/anari_cpp/ext/linalg.h>

#include <cmath>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Camera *);

namespace AnariFilament {

Camera::Camera(DeviceState *s)
    : Object(ANARI_CAMERA, s)
{
    auto *engine = s->engine;
    mEntity = utils::EntityManager::get().create();
    mCamera = engine->createCamera(mEntity);
}

Camera::~Camera()
{
    auto *engine = deviceState()->engine;
    engine->destroyCameraComponent(mEntity);
    utils::EntityManager::get().destroy(mEntity);
}

void Camera::commitParameters()
{
    auto aspect = getParam<float>("aspect", 1.0f);
    auto fovy = getParam<float>("fovy", static_cast<float>(M_PI / 3.0));

    float fovDegrees = fovy * 180.0f / static_cast<float>(M_PI);
    mCamera->setProjection(fovDegrees, aspect, 0.1, 1000.0);

    using float3 = anari::math::float3;
    auto position = getParam<float3>("position", float3(0.0f, 0.0f, 0.0f));
    auto direction = getParam<float3>("direction", float3(0.0f, 0.0f, -1.0f));
    auto up = getParam<float3>("up", float3(0.0f, 1.0f, 0.0f));

    filament::math::double3 eye = {position[0], position[1], position[2]};
    filament::math::double3 center = {
        position[0] + direction[0],
        position[1] + direction[1],
        position[2] + direction[2]};
    filament::math::double3 upVec = {up[0], up[1], up[2]};

    mCamera->lookAt(eye, center, upVec);

    // setExposure(1.0f) gives neutral exposure (factor=1.0) so Filament's
    // rendered luminance maps directly to pixel values.
    mCamera->setExposure(1.0f);

    markCommitted();
}

}
