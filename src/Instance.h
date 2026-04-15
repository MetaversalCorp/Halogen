// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Group.h"

#include <helium/utility/IntrusivePtr.h>
#include <math/mat4.h>

namespace Halogen {

struct Instance : public Object
{
    Instance(DeviceState *s);

    void commitParameters() override;

    Group *group() const { return mGroup.ptr; }
    const filament::math::mat4f &transform() const { return mTransform; }

private:
    helium::IntrusivePtr<Group> mGroup;
    filament::math::mat4f mTransform;
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Instance *, ANARI_INSTANCE);
