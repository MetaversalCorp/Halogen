// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "World.h"

#include <filament/Engine.h>
#include <filament/Scene.h>

#include <helium/array/Array1D.h>
#include <helium/array/ObjectArray.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::World *);

namespace AnariFilament {

World::World(DeviceState *s)
    : Object(ANARI_WORLD, s)
{
    mScene = s->engine->createScene();
}

World::~World()
{
    deviceState()->engine->destroy(mScene);
}

void World::commitParameters()
{
    // Remove all existing entities from the scene
    for (const helium::IntrusivePtr<Surface> &surf : mSurfaces) {
        if (surf && surf->isValid())
            mScene->remove(surf->entity());
    }
    for (const helium::IntrusivePtr<Light> &light : mLights)
        mScene->remove(light->entity());

    mSurfaces.clear();
    mLights.clear();

    // Surfaces
    helium::ObjectArray *surfaceArray =
        getParamObject<helium::ObjectArray>("surface");
    if (surfaceArray) {
        helium::BaseObject **surfaces = surfaceArray->handlesBegin();
        size_t count = surfaceArray->totalSize();
        for (size_t i = 0; i < count; ++i) {
            Surface *surface = static_cast<Surface *>(surfaces[i]);
            if (surface && surface->isValid()) {
                mSurfaces.emplace_back(surface);
                mScene->addEntity(surface->entity());
            }
        }
    }

    // Lights
    helium::ObjectArray *lightArray =
        getParamObject<helium::ObjectArray>("light");
    if (lightArray) {
        helium::BaseObject **lights = lightArray->handlesBegin();
        size_t count = lightArray->totalSize();
        for (size_t i = 0; i < count; ++i) {
            Light *light = static_cast<Light *>(lights[i]);
            if (light) {
                mLights.emplace_back(light);
                mScene->addEntity(light->entity());
            }
        }
    }

    markCommitted();
}

}
