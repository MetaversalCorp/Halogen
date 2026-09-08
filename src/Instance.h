// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Group.h"

#include <Corrade/Containers/Array.h>
#include <helium/utility/IntrusivePtr.h>
#include <math/mat4.h>
#include <utils/Entity.h>

namespace Halogen {

struct Instance : public Object
{
    Instance(DeviceState *s);

    void commitParameters() override;

    Group *group() const { return mGroup.ptr; }
    const filament::math::mat4f &transform() const { return mTransform; }
    size_t boneCount() const { return mBones.size(); }
    const filament::math::mat4f *bones() const
    {
        return mBones.isEmpty() ? nullptr : mBones.data();
    }

    // The owning World builds one Filament entity per group surface and hands
    // the (non-owning) handles here so commitParameters() can re-apply this
    // instance's transform to them every frame without a full World rebuild.
    // The World owns the entities' lifetime and clears these on rebuild.
    void setEntities(Corrade::Containers::Array<utils::Entity> aEntity);
    void clearEntities();

private:
    helium::IntrusivePtr<Group> mGroup;
    filament::math::mat4f mTransform;
    Corrade::Containers::Array<utils::Entity> mEntities;
    Corrade::Containers::Array<filament::math::mat4f> mBones;
    Corrade::Containers::Array<filament::math::mat4f> mBonesComm;
    bool mBonesOnGpu = false;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Instance *, ANARI_INSTANCE);
