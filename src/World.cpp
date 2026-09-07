// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "World.h"

#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/Scene.h>
#include <filament/TransformManager.h>

#include <helium/array/Array1D.h>
#include <helium/array/ObjectArray.h>

#include <Corrade/Containers/GrowableArray.h>

#include <utils/EntityManager.h>

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::World *);

namespace Halogen {

World::World(DeviceState *s)
    : Object(ANARI_WORLD, s)
{
    mScene = s->engine->createScene();
}

World::~World()
{
    clearObservers();
    clearInstanceEntities();
    deviceState()->engine->destroy(mScene);
}

void World::clearObservers()
{
    for (const helium::IntrusivePtr<helium::BaseObject> &o : mObserved) {
        if (o.ptr)
            o.ptr->removeChangeObserver(this);
    }
    mObserved.clear();
}

void World::observe(helium::BaseObject *obj)
{
    if (!obj)
        return;

    for (const helium::IntrusivePtr<helium::BaseObject> &o : mObserved) {
        if (o.ptr == obj)
            return;
    }

    obj->addChangeObserver(this);
    mObserved.push_back(helium::IntrusivePtr<helium::BaseObject>{obj});
}

void World::clearInstanceEntities()
{
    // The entities are about to be destroyed; drop the non-owning copies each
    // instance kept for its per-frame transform refresh so a commit between now
    // and the next finalize() cannot touch a stale handle.
    for (const helium::IntrusivePtr<Instance> &inst : mInstances) {
        if (inst.ptr)
            inst.ptr->clearEntities();
    }

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
    markCommitted();
}

void World::finalize()
{
    filament::Engine * const engine = deviceState()->engine;

    // Re-registered below against the current set of rendered geometries.
    clearObservers();

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

            // Collected so the instance can re-apply its transform to exactly
            // these entities every frame (Instance::commitParameters), which is
            // the only per-frame commit helium runs for an animated instance.
            std::vector<utils::Entity> aEntity_Inst;

            for (const helium::IntrusivePtr<Surface> &surf :
                    inst->group()->surfaces()) {
                if (!surf || !surf->isValid())
                    continue;

                Geometry *geom = surf->geometry();
                Material *mat = surf->material();
                if (!geom || !mat)
                    continue;

                // Observe the geometry/material so a rebuild of their buffers
                // re-queues this World to rebuild the instance renderable,
                // rather than leaving it bound to a destroyed VertexBufferInfo.
                observe(geom);
                observe(mat);

                utils::Entity e = utils::EntityManager::get().create();
                new (&mInstanceEntities[entityIdx++]) utils::Entity{e};
                aEntity_Inst.push_back(e);

                const Aabb &geomAabb = geom->aabb();
                filament::Box box = {
                    geomAabb.center(), geomAabb.halfExtent()};

                filament::RenderableManager::Builder builder(1);
                builder.geometry(0,
                        filament::RenderableManager::PrimitiveType::TRIANGLES,
                        geom->vertexBuffer(), geom->indexBuffer(),
                        0, geom->indexCount())
                    .material(0, mat->materialInstance())
                    .boundingBox(box)
                    .receiveShadows(true)
                    .castShadows(true);
                if (geom->hasSkinning())
                {
                    // Bones stay in model space; the instance transform (Y-up
                    // convert, node TRS, dRenderScale) stays on the entity.
                    // Folding that matrix into the palette and leaving the
                    // entity at identity dropped dRenderScale, so a scale-1
                    // node drew at metre size in a render-unit camera.
                    size_t nBone = inst->boneCount();
                    if (nBone == 0)
                        nBone = 1;
                    if (nBone > 255)
                        nBone = 255;
                    if (inst->bones() && inst->boneCount() > 0)
                        builder.skinning(nBone, inst->bones());
                    else
                        builder.skinning(nBone);
                    // Rest-pose AABB is object-space; a scaled or posed skin
                    // can sit outside it. Keep the draw even if the box misses.
                    builder.culling(false);
                }
                builder.build(*engine, e);

                tcm.create(e);
                auto ti = tcm.getInstance(e);
                tcm.setTransform(ti, xform);

                mScene->addEntity(e);
            }

            Corrade::Containers::Array<utils::Entity> aEntity{
                Corrade::NoInit, aEntity_Inst.size()};
            for (size_t k = 0; k < aEntity_Inst.size(); ++k)
                new (&aEntity[k]) utils::Entity{aEntity_Inst[k]};
            inst->setEntities(std::move(aEntity));
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
}

}
