// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Camera.h"

#include "Constants.h"
#include "MathConversions.h"

#include <filament/Camera.h>
#include <filament/Engine.h>
#include <utils/EntityManager.h>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

using namespace Corrade::Containers::Literals;

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Camera *);

namespace Halogen {

Camera::Camera(DeviceState *s, const char *subtype)
    : Object(ANARI_CAMERA, s)
    , mSubtype(subtype ? subtype : "perspective")
{
    filament::Engine * const engine = s->engine;
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
    const float3 position = getParam<float3>(
        "position", float3(0.0f, 0.0f, 0.0f));
    const float3 direction = getParam<float3>(
        "direction", float3(0.0f, 0.0f, -1.0f));
    const float3 up = getParam<float3>("up", float3(0.0f, 1.0f, 0.0f));

    const filament::math::double3 eye = toDouble3(position);
    const filament::math::double3 center = {
        position[0] + direction[0],
        position[1] + direction[1],
        position[2] + direction[2]};
    const filament::math::double3 upVec = toDouble3(up);
    mCamera->lookAt(eye, center, upVec);

    const float aspect = getParam<float>("aspect", 1.0f);
    const float near = getParam<float>("near", 0.1f);
    const float far = getParam<float>("far", 1000.0f);

    if (mSubtype == "orthographic"_s) {
        const float height = getParam<float>("height", 1.0f);
        const float halfH = height * 0.5f;
        const float halfW = halfH * aspect;
        mCamera->setProjection(
            filament::Camera::Projection::ORTHO,
            -halfW, halfW, -halfH, halfH, near, far);
    } else {
        const float fovy = getParam<float>("fovy", Pi / 3.0f);
        const float fovDegrees = fovy * 180.0f / Pi;
        mCamera->setProjection(fovDegrees, aspect, near, far);
    }

    // Neutral exposure by default so Filament's rendered luminance maps
    // directly to pixel values.  The "exposure" parameter is a Filament
    // extension (not part of standard ANARI).
    const float exposure = getParam<float>("exposure", 1.0f);
    mCamera->setExposure(exposure);

    markCommitted();
}

}
