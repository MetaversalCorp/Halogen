// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Surface.h"

#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <utils/EntityManager.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Surface *);

namespace AnariFilament {

Surface::Surface(DeviceState *s)
    : Object(ANARI_SURFACE, s)
{
    mEntity = utils::EntityManager::get().create();
}

Surface::~Surface()
{
    auto *engine = deviceState()->engine;
    engine->destroy(mEntity);
    utils::EntityManager::get().destroy(mEntity);
}

void Surface::commitParameters()
{
    mGeometry = getParamObject<Geometry>("geometry");
    mMaterial = getParamObject<Material>("material");

    if (!mGeometry || !mMaterial)
        return;

    if (!mGeometry->vertexBuffer() || !mGeometry->indexBuffer()
        || !mMaterial->materialInstance())
        return;

    auto *engine = deviceState()->engine;

    if (mBuilt)
        engine->getRenderableManager().destroy(mEntity);

    auto aabbMin = mGeometry->aabbMin();
    auto aabbMax = mGeometry->aabbMax();
    filament::Box aabb = {
        (aabbMin + aabbMax) * 0.5f,
        (aabbMax - aabbMin) * 0.5f};

    filament::RenderableManager::Builder(1)
        .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
            mGeometry->vertexBuffer(), mGeometry->indexBuffer(),
            0, mGeometry->indexCount())
        .material(0, mMaterial->materialInstance())
        .boundingBox(aabb)
        .receiveShadows(false)
        .castShadows(false)
        .build(*engine, mEntity);

    mBuilt = true;
    markCommitted();
}

bool Surface::isValid() const
{
    return mGeometry && mMaterial && mGeometry->vertexBuffer()
        && mMaterial->materialInstance();
}

}
