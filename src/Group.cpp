// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Group.h"

#include <helium/array/ObjectArray.h>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Group *);

namespace AnariFilament {

Group::Group(DeviceState *s)
    : Object(ANARI_GROUP, s) {}

void Group::commitParameters()
{
    mSurfaces = {};

    helium::ObjectArray *surfaceArray =
        getParamObject<helium::ObjectArray>("surface");
    if (!surfaceArray) {
        markCommitted();
        return;
    }

    helium::BaseObject **handles = surfaceArray->handlesBegin();
    size_t total = surfaceArray->totalSize();

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
        if (s && s->isValid())
            new (&mSurfaces[idx++]) helium::IntrusivePtr<Surface>{s};
    }

    markCommitted();
}

}
