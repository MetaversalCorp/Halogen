// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Surface.h"

#include <Corrade/Containers/Array.h>
#include <helium/utility/IntrusivePtr.h>

namespace AnariFilament {

struct Group : public Object
{
    Group(DeviceState *s);

    void commitParameters() override;
    void finalize() override;

    Corrade::Containers::Array<helium::IntrusivePtr<Surface>> const &
    surfaces() const
    {
        return mSurfaces;
    }

private:
    Corrade::Containers::Array<helium::IntrusivePtr<Surface>> mSurfaces;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Group *, ANARI_GROUP);
