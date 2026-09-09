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

#include <vector>

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

static size_t countRenderableSurfaces(Instance *inst)
{
    size_t n = 0;
    if (!inst || !inst->group())
        return n;
    for (const helium::IntrusivePtr<Surface> &surf : inst->group()->surfaces()) {
        if (surf && surf->isValid() && surf->geometry() && surf->material())
            ++n;
    }
    return n;
}

bool World::instancesArePrefixGrow(const std::vector<Instance *> &aWant) const
{
    if (aWant.size() < mInstances.size())
        return false;
    for (size_t i = 0; i < mInstances.size(); ++i) {
        if (mInstances[i].ptr != aWant[i])
            return false;
        // Group-surface growth (first VRM filling in unique draws) is still
        // a prefix: existing entities stay, new surfaces append. A shrink
        // still forces a full rebuild.
        if (countRenderableSurfaces(aWant[i]) < aWant[i]->entityCount())
            return false;
    }
    return true;
}

void World::appendEntitiesForInstance(Instance *inst,
    std::vector<utils::Entity> &aWorldEntity)
{
    if (!inst || !inst->group())
        return;

    const size_t nSkip = inst->entityCount();
    if (countRenderableSurfaces(inst) <= nSkip)
        return;

    filament::Engine * const engine = deviceState()->engine;
    auto &tcm = engine->getTransformManager();
    const filament::math::mat4f &xform = inst->transform();
    std::vector<utils::Entity> aEntity_New;
    size_t nSeen = 0;

    for (const helium::IntrusivePtr<Surface> &surf :
            inst->group()->surfaces()) {
        if (!surf || !surf->isValid())
            continue;

        Geometry *geom = surf->geometry();
        Material *mat = surf->material();
        if (!geom || !mat)
            continue;

        if (nSeen < nSkip) {
            ++nSeen;
            continue;
        }
        ++nSeen;

        // Observe the geometry/material so a rebuild of their buffers
        // re-queues this World to rebuild the instance renderable,
        // rather than leaving it bound to a destroyed VertexBufferInfo.
        observe(geom);
        observe(mat);

        utils::Entity e = utils::EntityManager::get().create();
        aWorldEntity.push_back(e);
        aEntity_New.push_back(e);

        const Aabb &geomAabb = geom->aabb();
        filament::Box box = {
            geomAabb.center(), geomAabb.halfExtent()};
        // Same threshold as Surface::finalize: PCF + 4x MSAA over a skinned
        // VRM crowd TDRs the GPU, after which beginFrame never returns.
        const uint32_t idxCount = geom->indexCount();
        const bool castShadows = !geom->hasSkinning()
            && idxCount <= 196608u;

        filament::RenderableManager::Builder builder(1);
        builder.geometry(0,
                filament::RenderableManager::PrimitiveType::TRIANGLES,
                geom->vertexBuffer(), geom->indexBuffer(),
                0, geom->indexCount())
            .material(0, mat->materialInstance())
            .boundingBox(box)
            .receiveShadows(true)
            .castShadows(castShadows);
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

        // Each skinned Builder.skinning(N, bones) copies N matrices
        // into Filament's command buffer. Flush often enough that a
        // crowd of high-bone VRMs cannot overflow the arena.
        if ((aEntity_New.size() % 16) == 0)
            engine->flush();
    }

    if (aEntity_New.empty())
        return;

    Corrade::Containers::Array<utils::Entity> aEntity{
        Corrade::NoInit, aEntity_New.size()};
    for (size_t k = 0; k < aEntity_New.size(); ++k)
        new (&aEntity[k]) utils::Entity{aEntity_New[k]};
    inst->appendEntities(std::move(aEntity));
}

void World::createEntitiesForInstance(Instance *inst,
    std::vector<utils::Entity> &aWorldEntity)
{
    appendEntitiesForInstance(inst, aWorldEntity);
}

void World::finalize()
{
    filament::Engine * const engine = deviceState()->engine;

    for (const helium::IntrusivePtr<Surface> &surf : mSurfaces) {
        if (surf && surf->isValid())
            mScene->remove(surf->entity());
    }
    for (const helium::IntrusivePtr<Light> &light : mLights)
        mScene->remove(light->entity());

    mSurfaces = {};
    mLights = {};

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

    // Instances. A Tester06-style crowd rebinds the world instance array
    // once per new copy. Destroying and recreating every Filament entity
    // on that rebind is O(crowd) and is the 1 s hitch after each batch.
    // When the new list is the old list plus copies at the end, only the
    // new instances get entities.
    helium::ObjectArray *instanceArray =
        getParamObject<helium::ObjectArray>("instance");
    std::vector<Instance *> aWant;
    if (instanceArray) {
        helium::BaseObject ** const handles = instanceArray->handlesBegin();
        const size_t total = instanceArray->totalSize();
        aWant.reserve(total);
        for (size_t i = 0; i < total; ++i) {
            Instance *inst = static_cast<Instance *>(handles[i]);
            if (inst && inst->group())
                aWant.push_back(inst);
        }
    }

    const bool bAppend = instancesArePrefixGrow(aWant);
    if (!bAppend) {
        clearObservers();
        clearInstanceEntities();
        engine->flush();
        mInstances = {};
        mInstanceEntities = {};
    }

    std::vector<helium::IntrusivePtr<Instance>> aInst;
    std::vector<utils::Entity> aWorldEntity;
    const size_t nKeep = bAppend ? mInstances.size() : 0;
    bool bCreated = false;
    aInst.reserve(aWant.size());
    aWorldEntity.reserve(mInstanceEntities.size() + 16);
    for (size_t i = 0; i < mInstanceEntities.size(); ++i)
        aWorldEntity.push_back(mInstanceEntities[i]);
    for (size_t i = 0; i < nKeep; ++i) {
        aInst.emplace_back(mInstances[i]);
        const size_t nBefore = aWorldEntity.size();
        appendEntitiesForInstance(aWant[i], aWorldEntity);
        if (aWorldEntity.size() > nBefore)
            bCreated = true;
    }
    for (size_t i = nKeep; i < aWant.size(); ++i) {
        aInst.emplace_back(aWant[i]);
        createEntitiesForInstance(aWant[i], aWorldEntity);
        bCreated = true;
    }

    mInstances = Corrade::Containers::Array<helium::IntrusivePtr<Instance>>{
        Corrade::NoInit, aInst.size()};
    for (size_t i = 0; i < aInst.size(); ++i)
        new (&mInstances[i]) helium::IntrusivePtr<Instance>{std::move(aInst[i])};

    mInstanceEntities = Corrade::Containers::Array<utils::Entity>{
        Corrade::NoInit, aWorldEntity.size()};
    for (size_t i = 0; i < aWorldEntity.size(); ++i)
        new (&mInstanceEntities[i]) utils::Entity{aWorldEntity[i]};

    if (bCreated)
        engine->flush();

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
