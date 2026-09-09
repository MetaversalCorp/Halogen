// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Instance.h"

#include <cstring>

#include <filament/Engine.h>
#include <filament/RenderableManager.h>
#include <filament/TransformManager.h>

#include <helium/array/Array1D.h>

ANARI_HALOGEN_TYPEFOR_DEFINITION(Halogen::Instance *);

namespace Halogen {

Instance::Instance(DeviceState *s)
    : Object(ANARI_INSTANCE, s) {}

void Instance::setEntities(Corrade::Containers::Array<utils::Entity> aEntity)
{
    mEntities = std::move(aEntity);
    // World::finalize already uploaded this palette via Builder.skinning.
    mBonesOnGpu = true;
}

void Instance::appendEntities(Corrade::Containers::Array<utils::Entity> aMore)
{
    if (aMore.isEmpty())
        return;

    Corrade::Containers::Array<utils::Entity> aAll{
        Corrade::NoInit, mEntities.size() + aMore.size()};
    for (size_t i = 0; i < mEntities.size(); ++i)
        new (&aAll[i]) utils::Entity{mEntities[i]};
    for (size_t i = 0; i < aMore.size(); ++i)
        new (&aAll[mEntities.size() + i]) utils::Entity{aMore[i]};
    mEntities = std::move(aAll);
    mBonesOnGpu = true;
}

void Instance::clearEntities()
{
    mEntities = {};
    mBonesOnGpu = false;
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

    mBones = {};
    helium::Array1D *boneArray = getParamObject<helium::Array1D>("bone.matrix");
    if (boneArray && boneArray->elementType() == ANARI_FLOAT32_MAT4
        && boneArray->totalSize() > 0)
    {
        size_t nBone = boneArray->totalSize();
        if (nBone > 255)
            nBone = 255;
        mBones = Corrade::Containers::Array<filament::math::mat4f>{
            Corrade::NoInit, nBone};
        std::memcpy(mBones.data(), boneArray->data(),
            nBone * sizeof(filament::math::mat4f));
    }

    bool bonesChanged = mBones.size() != mBonesComm.size();
    if (!bonesChanged && !mBones.isEmpty())
    {
        bonesChanged = std::memcmp(mBones.data(), mBonesComm.data(),
            mBones.size() * sizeof(filament::math::mat4f)) != 0;
    }
    if (bonesChanged)
    {
        mBonesComm = Corrade::Containers::Array<filament::math::mat4f>{
            Corrade::NoInit, mBones.size()};
        if (!mBones.isEmpty())
        {
            std::memcpy(mBonesComm.data(), mBones.data(),
                mBones.size() * sizeof(filament::math::mat4f));
        }
        mBonesOnGpu = false;
    }

    // Re-apply the (possibly updated) transform to this instance's Filament
    // entities. World::finalize() sets it once at build time and only re-runs
    // on a structural change, so an animated instance would otherwise stay
    // frozen at its build-time transform. This is the per-frame commit helium
    // actually runs for a moving instance. Bone palettes follow the same path
    // via setBones so a pose change does not rebuild vertex buffers. Skip
    // setBones when the palette is unchanged: a transform-only commit on a
    // 137-bone crowd would otherwise copy every surface's palette into the
    // command stream and overflow minCommandBufferSizeMB.
    if (!mEntities.isEmpty())
    {
        auto &tcm = deviceState()->engine->getTransformManager();
        auto &rm = deviceState()->engine->getRenderableManager();
        bool bonesApplied = false;
        size_t nBoneFlush = 0;
        for (utils::Entity e : mEntities)
        {
            auto ti = tcm.getInstance(e);
            if (ti.isValid())
                tcm.setTransform(ti, mTransform);

            if (!mBonesOnGpu && !mBones.isEmpty())
            {
                auto ri = rm.getInstance(e);
                if (ri.isValid())
                {
                    rm.setBones(ri, mBones.data(), mBones.size());
                    bonesApplied = true;
                    if ((++nBoneFlush % 16) == 0)
                        deviceState()->engine->flush();
                }
            }
        }
        if (bonesApplied)
        {
            mBonesOnGpu = true;
            deviceState()->engine->flush();
        }
    }

    markCommitted();
}

}
