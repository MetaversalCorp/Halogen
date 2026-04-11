// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Camera.h"

#include "MathConversions.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <utils/EntityManager.h>

#include <cmath>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Camera *);

namespace AnariFilament {

Camera::Camera(DeviceState *s)
    : Object(ANARI_CAMERA, s)
{
    filament::Engine *engine = s->engine;
    mEntity = utils::EntityManager::get().create();
    filament::Camera *cam = engine->createCamera(mEntity);
    mCamera = {engine, cam, [engine, e = mEntity]() {
        engine->destroyCameraComponent(e);
        utils::EntityManager::get().destroy(e);
    }};
}

Camera::~Camera() = default;

void Camera::commitParameters()
{
    float aspect = getParam<float>("aspect", 1.0f);
    float fovy = getParam<float>("fovy", static_cast<float>(M_PI / 3.0));

    float fovDegrees = fovy * 180.0f / static_cast<float>(M_PI);
    mCamera->setProjection(fovDegrees, aspect, 0.1, 1000.0);

    using float3 = anari::math::float3;
    float3 position = getParam<float3>("position", float3(0.0f, 0.0f, 0.0f));
    float3 direction = getParam<float3>("direction", float3(0.0f, 0.0f, -1.0f));
    float3 up = getParam<float3>("up", float3(0.0f, 1.0f, 0.0f));

    filament::math::double3 eye = toDouble3(position);
    filament::math::double3 center = {
        position[0] + direction[0],
        position[1] + direction[1],
        position[2] + direction[2]};
    filament::math::double3 upVec = toDouble3(up);

    mCamera->lookAt(eye, center, upVec);

    // Neutral exposure by default so Filament's rendered luminance maps
    // directly to pixel values.  The "exposure" parameter is a Filament
    // extension (not part of standard ANARI).
    float exposure = getParam<float>("exposure", 1.0f);
    mCamera->setExposure(exposure);

    markCommitted();
}

}
