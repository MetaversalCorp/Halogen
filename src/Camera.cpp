// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Camera.h"

#include "MathConversions.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <utils/EntityManager.h>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

#include <cmath>

using namespace Corrade::Containers::Literals;

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Camera *);

namespace AnariFilament {

Camera::Camera(DeviceState *s, const char *subtype)
    : Object(ANARI_CAMERA, s)
    , mSubtype(subtype ? subtype : "perspective")
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

    float aspect = getParam<float>("aspect", 1.0f);
    float near = getParam<float>("near", 0.1f);
    float far = getParam<float>("far", 1000.0f);

    if (mSubtype == "orthographic"_s) {
        float height = getParam<float>("height", 1.0f);
        float halfH = height * 0.5f;
        float halfW = halfH * aspect;
        mCamera->setProjection(
            filament::Camera::Projection::ORTHO,
            -halfW, halfW, -halfH, halfH, near, far);
    } else {
        float fovy = getParam<float>("fovy", static_cast<float>(M_PI / 3.0));
        float fovDegrees = fovy * 180.0f / static_cast<float>(M_PI);
        mCamera->setProjection(fovDegrees, aspect, near, far);
    }

    // Neutral exposure by default so Filament's rendered luminance maps
    // directly to pixel values.  The "exposure" parameter is a Filament
    // extension (not part of standard ANARI).
    float exposure = getParam<float>("exposure", 1.0f);
    mCamera->setExposure(exposure);

    markCommitted();
}

}
