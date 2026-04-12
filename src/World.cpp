// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "World.h"

#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>

#include <helium/array/Array1D.h>
#include <helium/array/ObjectArray.h>

#include <utils/EntityManager.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::World *);

namespace AnariFilament {

World::World(DeviceState *s)
    : Object(ANARI_WORLD, s)
{
    mScene = s->engine->createScene();
}

World::~World()
{
    clearInstanceEntities();
    deviceState()->engine->destroy(mScene);
}

void World::clearInstanceEntities()
{
    filament::Engine *engine = deviceState()->engine;
    for (utils::Entity e : mInstanceEntities) {
        mScene->remove(e);
        engine->getRenderableManager().destroy(e);
        engine->getTransformManager().destroy(e);
        utils::EntityManager::get().destroy(e);
    }
    mInstanceEntities = {};
}

void World::commitParameters()
{
    filament::Engine * const engine = deviceState()->engine;

    // Remove all existing entities from the scene
    for (const helium::IntrusivePtr<Surface> &surf : mSurfaces) {
        if (surf && surf->isValid())
            mScene->remove(surf->entity());
    }
    for (const helium::IntrusivePtr<Light> &light : mLights)
        mScene->remove(light->entity());
    clearInstanceEntities();

    mSurfaces = {};
    mLights = {};
    mInstances = {};

    // Direct surfaces
    helium::ObjectArray *surfaceArray =
        getParamObject<helium::ObjectArray>("surface");
    if (surfaceArray) {
        helium::BaseObject ** const handles = surfaceArray->handlesBegin();
        const size_t total = surfaceArray->totalSize();

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

    // Instances
    helium::ObjectArray *instanceArray =
        getParamObject<helium::ObjectArray>("instance");
    if (instanceArray) {
        helium::BaseObject ** const handles = instanceArray->handlesBegin();
        const size_t total = instanceArray->totalSize();

        // Count total instance entities needed
        size_t totalEntities = 0;
        size_t validInstances = 0;
        for (size_t i = 0; i < total; ++i) {
            Instance *inst = static_cast<Instance *>(handles[i]);
            if (inst && inst->group()) {
                ++validInstances;
                for (const helium::IntrusivePtr<Surface> &surf :
                        inst->group()->surfaces()) {
                    if (surf && surf->isValid())
                        ++totalEntities;
                }
            }
        }

        mInstances = Corrade::Containers::Array<helium::IntrusivePtr<Instance>>{
            Corrade::NoInit, validInstances};
        mInstanceEntities = Corrade::Containers::Array<utils::Entity>{
            Corrade::NoInit, totalEntities};

        size_t instIdx = 0;
        size_t entityIdx = 0;

        auto &tcm = engine->getTransformManager();

        for (size_t i = 0; i < total; ++i) {
            Instance *inst = static_cast<Instance *>(handles[i]);
            if (!inst || !inst->group())
                continue;

            new (&mInstances[instIdx++])
                helium::IntrusivePtr<Instance>{inst};

            const filament::math::mat4f &xform = inst->transform();

            for (const helium::IntrusivePtr<Surface> &surf :
                    inst->group()->surfaces()) {
                if (!surf || !surf->isValid())
                    continue;

                Geometry *geom = surf->geometry();
                Material *mat = surf->material();
                if (!geom || !mat)
                    continue;

                utils::Entity e = utils::EntityManager::get().create();
                new (&mInstanceEntities[entityIdx++]) utils::Entity{e};

                const Aabb &geomAabb = geom->aabb();
                filament::Box box = {
                    geomAabb.center(), geomAabb.halfExtent()};

                filament::RenderableManager::Builder(1)
                    .geometry(0,
                        filament::RenderableManager::PrimitiveType::TRIANGLES,
                        geom->vertexBuffer(), geom->indexBuffer(),
                        0, geom->indexCount())
                    .material(0, mat->materialInstance())
                    .boundingBox(box)
                    .receiveShadows(false)
                    .castShadows(false)
                    .build(*engine, e);

                tcm.create(e);
                auto ti = tcm.getInstance(e);
                tcm.setTransform(ti, xform);

                mScene->addEntity(e);
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
