// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Light.h"

#include "Constants.h"
#include "MathConversions.h"

#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

using namespace Corrade::Containers::Literals;

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Light *);

namespace AnariFilament {

Light::Light(DeviceState *s, const char *subtype)
    : Object(ANARI_LIGHT, s)
    , mSubtype(subtype ? subtype : "directional")
{
    mEntity = utils::EntityManager::get().create();
}

Light::~Light()
{
    filament::Engine * const engine = deviceState()->engine;
    engine->destroy(mEntity);
    utils::EntityManager::get().destroy(mEntity);
}

void Light::commitParameters()
{
    filament::Engine * const engine = deviceState()->engine;

    using float3 = anari::math::float3;
    const float3 color = getParam<float3>("color", float3(1.0f, 1.0f, 1.0f));

    if (mBuilt)
        engine->getLightManager().destroy(mEntity);

    if (mSubtype == "point"_s) {
        const float3 position = getParam<float3>(
            "position", float3(0.0f, 0.0f, 0.0f));
        const float intensity = getParam<float>("intensity", 1.0f);
        const float power = getParam<float>("power", 0.0f);

        // ANARI spec: if power is set, use it; otherwise use intensity.
        // Filament point light intensity is in candela (lm/sr).
        const float finalIntensity = power > 0.0f
            ? power / (4.0f * Pi)
            : intensity;

        filament::LightManager::Builder(filament::LightManager::Type::POINT)
            .position(toFilament(position))
            .color(toFilament(color))
            .intensity(finalIntensity)
            .falloff(100.0f)
            .castShadows(false)
            .build(*engine, mEntity);
    } else if (mSubtype == "spot"_s) {
        const float3 position = getParam<float3>(
            "position", float3(0.0f, 0.0f, 0.0f));
        const float3 direction = getParam<float3>(
            "direction", float3(0.0f, 0.0f, -1.0f));
        const float intensity = getParam<float>("intensity", 1.0f);
        const float power = getParam<float>("power", 0.0f);
        const float openingAngle = getParam<float>(
            "openingAngle", Pi);
        const float falloffAngle = getParam<float>("falloffAngle", 0.1f);

        const float finalIntensity = power > 0.0f
            ? power / (4.0f * Pi)
            : intensity;

        // Filament expects half-angle in degrees for the outer cone
        const float outerDeg = openingAngle * 180.0f / Pi;
        float innerDeg = (openingAngle - falloffAngle)
            * 180.0f / Pi;
        if (innerDeg < 0.0f)
            innerDeg = 0.0f;

        filament::LightManager::Builder(
                filament::LightManager::Type::FOCUSED_SPOT)
            .position(toFilament(position))
            .direction(toFilament(direction))
            .color(toFilament(color))
            .intensity(finalIntensity)
            .falloff(100.0f)
            .spotLightCone(innerDeg, outerDeg)
            .castShadows(false)
            .build(*engine, mEntity);
    } else {
        // Directional light (default)
        const float3 direction = getParam<float3>(
            "direction", float3(0.0f, 0.0f, -1.0f));
        const float irradiance = getParam<float>("irradiance", 1.0f);

        // Multiply irradiance by pi to cancel Filament's 1/pi BRDF
        // normalization, so a Lambertian surface with albedo A and
        // irradiance E gives output ~ A*E.
        const float filamentIntensity = irradiance * Pi;

        filament::LightManager::Builder(
                filament::LightManager::Type::DIRECTIONAL)
            .direction(toFilament(direction))
            .color(toFilament(color))
            .intensity(filamentIntensity)
            .castShadows(false)
            .build(*engine, mEntity);
    }

    mBuilt = true;
    markCommitted();
}

}
