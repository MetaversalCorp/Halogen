// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Light.h"

#include "Constants.h"
#include "MathConversions.h"

#include <filament/Engine.h>
#include <filament/LightManager.h>
#include <utils/EntityManager.h>

#include <cmath>

#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

using namespace Corrade::Containers::Literals;

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Light *);

namespace Halogen {

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

        // Influence radius. Filament sizes a point light's shadow cubemap far
        // plane to this radius, so it must track where the geometry actually is
        // or the shadow depth range loses all precision and every surface
        // self-shadows to black. The framework supplies "falloff" in metres,
        // sized to the (compressed) scene span, so shadows stay precise at any
        // scale. Absent it, fall back to an intensity-derived radius
        // (r = sqrt(I / E_min)); note this can balloon for a bright source and
        // is only a safety default, not the shadow-friendly path.
        const float kMinIlluminance = 1.0e-3f;
        float falloff = getParam<float>("falloff", 0.0f);
        if (falloff <= 0.0f)
            falloff = std::sqrt(finalIntensity / kMinIlluminance);

        // ANARI intensity for point lights is in W/sr (radiant intensity),
        // which maps to candela (lm/sr). Filament's intensity() for point
        // lights takes lumens, so we use intensityCandela() instead.
        filament::LightManager::Builder(filament::LightManager::Type::POINT)
            .position(toFilament(position))
            .color(toFilament(color))
            .intensityCandela(finalIntensity)
            .falloff(falloff)
            .castShadows(true)
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

        // Filament's spotLightCone() takes half-angles in RADIANS (from the
        // center axis to the cone edge), with outer clamped to (0, pi/2].
        // ANARI's openingAngle is the FULL cone aperture in radians, so the
        // outer half-angle is openingAngle/2. falloffAngle is the penumbra
        // measured inward from the outer edge (0 <= falloffAngle <=
        // openingAngle/2), so the inner full-intensity half-angle is
        // openingAngle/2 - falloffAngle.
        const float outerRad = openingAngle * 0.5f;
        float innerRad = openingAngle * 0.5f - falloffAngle;
        if (innerRad < 0.0f)
            innerRad = 0.0f;

        // Influence radius (see the point-light branch): the framework supplies
        // "falloff" sized to the scene so the shadow-map far plane tracks the
        // geometry; fall back to an intensity-derived radius only if unset.
        const float kMinIlluminance = 1.0e-3f;
        float falloff = getParam<float>("falloff", 0.0f);
        if (falloff <= 0.0f)
            falloff = std::sqrt(finalIntensity / kMinIlluminance);

        // ANARI intensity for spot lights is peak radiant intensity in W/sr
        // (candela), so use intensityCandela() not intensity() (which takes lm).
        filament::LightManager::Builder(
                filament::LightManager::Type::FOCUSED_SPOT)
            .position(toFilament(position))
            .direction(toFilament(direction))
            .color(toFilament(color))
            .intensityCandela(finalIntensity)
            .falloff(falloff)
            .spotLightCone(innerRad, outerRad)
            .castShadows(true)
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
            .castShadows(true)
            .build(*engine, mEntity);
    }

    mBuilt = true;
    markCommitted();
}

}
