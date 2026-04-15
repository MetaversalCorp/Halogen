// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "DeviceState.h"

#include <helium/BaseObject.h>
#include <helium/utility/ChangeObserverPtr.h>

namespace Halogen {

struct Object : public helium::BaseObject
{
    Object(ANARIDataType type, DeviceState *s)
        : helium::BaseObject(type, s)
    {
        markParameterChanged();
    }

    virtual ~Object() = default;

    bool getProperty(const std::string_view &,
        ANARIDataType,
        void *,
        uint64_t,
        uint32_t) override
    {
        return false;
    }

    void commitParameters() override {}
    void finalize() override {}

    bool isValid() const override { return true; }

    DeviceState *deviceState() const
    {
        return asFilamentState(m_state);
    }
};

struct UnknownObject : public Object
{
    UnknownObject(ANARIDataType type, DeviceState *s)
        : Object(type, s) {}

    bool isValid() const override { return false; }
};

}

ANARI_HALOGEN_TYPEFOR_SPECIALIZATION(
    Halogen::Object *, ANARI_OBJECT);
