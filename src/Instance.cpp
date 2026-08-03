// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Instance.h"

#include <cstring>

#include <filament/Engine.h>
#include <filament/TransformManager.h>

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Instance *);

namespace Halogen {

Instance::Instance(DeviceState *s)
    : Object(ANARI_INSTANCE, s) {}

void Instance::setEntities(Corrade::Containers::Array<utils::Entity> aEntity)
{
    mEntities = std::move(aEntity);
}

void Instance::clearEntities()
{
    mEntities = {};
}

void Instance::commitParameters()
{
    mGroup = getParamObject<Group>("group");

    // ANARI transform is a column-major 4x4 matrix
    using float4 = anari::math::float4;
    using mat4 = anari::math::mat4;

    const mat4 identity = {
        float4{1, 0, 0, 0},
        float4{0, 1, 0, 0},
        float4{0, 0, 1, 0},
        float4{0, 0, 0, 1}
    };
    const mat4 m4 = getParam<mat4>("transform", identity);

    // filament::math::mat4f is also column-major, same layout
    std::memcpy(&mTransform, &m4, sizeof(filament::math::mat4f));

    // Re-apply the (possibly updated) transform to this instance's Filament
    // entities. World::finalize() sets it once at build time and only re-runs
    // on a structural change, so an animated instance would otherwise stay
    // frozen at its build-time transform. This is the per-frame commit helium
    // actually runs for a moving instance.
    if (!mEntities.isEmpty())
    {
        auto &tcm = deviceState()->engine->getTransformManager();
        for (utils::Entity e : mEntities)
        {
            auto ti = tcm.getInstance(e);
            if (ti.isValid())
                tcm.setTransform(ti, mTransform);
        }
    }

    markCommitted();
}

}
