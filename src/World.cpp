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

    mSurfaces = {};
    mLights = {};

    // Surfaces
    helium::ObjectArray *surfaceArray =
        getParamObject<helium::ObjectArray>("surface");
    if (surfaceArray) {
        helium::BaseObject **handles = surfaceArray->handlesBegin();
        size_t total = surfaceArray->totalSize();

        // Count valid surfaces, then allocate
        size_t validCount = 0;
        for (size_t i = 0; i < total; ++i) {
            Surface *s = static_cast<Surface *>(handles[i]);
            if (s && s->isValid())
                ++validCount;
        }

        mSurfaces = Corrade::Containers::Array<helium::IntrusivePtr<Surface>>{
            Corrade::NoInit, validCount};
        size_t idx = 0;
        for (size_t i = 0; i < total; ++i) {
            Surface *s = static_cast<Surface *>(handles[i]);
            if (s && s->isValid()) {
                new (&mSurfaces[idx++]) helium::IntrusivePtr<Surface>{s};
                mScene->addEntity(s->entity());
            }
        }
    }

    // Lights
    helium::ObjectArray *lightArray =
        getParamObject<helium::ObjectArray>("light");
    if (lightArray) {
        helium::BaseObject **handles = lightArray->handlesBegin();
        size_t total = lightArray->totalSize();

        size_t validCount = 0;
        for (size_t i = 0; i < total; ++i) {
            if (handles[i])
                ++validCount;
        }

        mLights = Corrade::Containers::Array<helium::IntrusivePtr<Light>>{
            Corrade::NoInit, validCount};
        size_t idx = 0;
        for (size_t i = 0; i < total; ++i) {
            Light *l = static_cast<Light *>(handles[i]);
            if (l) {
                new (&mLights[idx++]) helium::IntrusivePtr<Light>{l};
                mScene->addEntity(l->entity());
            }
        }
    }

    markCommitted();
}

}
