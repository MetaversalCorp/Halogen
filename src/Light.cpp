// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Light.h"

#include "MathConversions.h"

#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>

#include <cmath>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Light *);

namespace AnariFilament {

Light::Light(DeviceState *s)
    : Object(ANARI_LIGHT, s)
{
    mEntity = utils::EntityManager::get().create();
}

Light::~Light()
{
    filament::Engine *engine = deviceState()->engine;
    engine->destroy(mEntity);
    utils::EntityManager::get().destroy(mEntity);
}

void Light::commitParameters()
{
    filament::Engine *engine = deviceState()->engine;

    using float3 = anari::math::float3;
    float3 direction = getParam<float3>("direction", float3(0.0f, 0.0f, -1.0f));
    float3 color = getParam<float3>("color", float3(1.0f, 1.0f, 1.0f));
    float irradiance = getParam<float>("irradiance", 1.0f);

    // Multiply irradiance by pi to cancel Filament's 1/pi BRDF normalization,
    // so a Lambertian surface with albedo A and irradiance E gives output ≈ A*E.
    float intensity = irradiance * static_cast<float>(M_PI);

    if (mBuilt)
        engine->getLightManager().destroy(mEntity);

    filament::LightManager::Builder(filament::LightManager::Type::DIRECTIONAL)
        .direction(toFilament(direction))
        .color(toFilament(color))
        .intensity(intensity)
        .castShadows(false)
        .build(*engine, mEntity);

    mBuilt = true;
    markCommitted();
}

}
