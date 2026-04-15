// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "DeviceState.h"
#include "Object.h"

#include <helium/BaseDevice.h>

namespace Halogen {

struct Device : public helium::BaseDevice
{
    // -- Data Arrays --

    void *mapArray(ANARIArray) override;
    void unmapArray(ANARIArray) override;

    // -- API Objects --

    ANARIArray1D newArray1D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType,
        uint64_t numItems1) override;
    ANARIArray2D newArray2D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType,
        uint64_t numItems1,
        uint64_t numItems2) override;
    ANARIArray3D newArray3D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType,
        uint64_t numItems1,
        uint64_t numItems2,
        uint64_t numItems3) override;

    ANARICamera newCamera(const char *type) override;
    ANARIFrame newFrame() override;
    ANARIGeometry newGeometry(const char *type) override;
    ANARIGroup newGroup() override;
    ANARIInstance newInstance(const char *type) override;
    ANARILight newLight(const char *type) override;
    ANARIMaterial newMaterial(const char *type) override;
    ANARIRenderer newRenderer(const char *type) override;
    ANARISampler newSampler(const char *type) override;
    ANARISpatialField newSpatialField(const char *type) override;
    ANARISurface newSurface() override;
    ANARIVolume newVolume(const char *type) override;
    ANARIWorld newWorld() override;

    // -- Extension interface --

    ANARIObject newObject(
        const char *objectType, const char *type) override;

    // -- Query functions --

    const char **getObjectSubtypes(ANARIDataType objectType) override;
    const void *getObjectInfo(ANARIDataType objectType,
        const char *objectSubtype,
        const char *infoName,
        ANARIDataType infoType) override;
    const void *getParameterInfo(ANARIDataType objectType,
        const char *objectSubtype,
        const char *parameterName,
        ANARIDataType parameterType,
        const char *infoName,
        ANARIDataType infoType) override;

    // -- Lifecycle --

    Device(ANARIStatusCallback defaultCallback, const void *userPtr);
    Device(ANARILibrary l);
    ~Device() override;

    void initDevice();

    void deviceCommitParameters() override;
    int deviceGetProperty(const char *name,
        ANARIDataType type,
        void *mem,
        uint64_t size,
        uint32_t mask) override;

private:
    DeviceState *deviceState() const;
    bool mInitialized = false;
};

}
