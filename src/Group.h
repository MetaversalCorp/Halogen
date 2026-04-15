// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Surface.h"

#include <Corrade/Containers/Array.h>
#include <helium/utility/IntrusivePtr.h>

namespace Halogen {

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

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Group *, ANARI_GROUP);
