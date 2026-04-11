// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <anari/backend/DeviceImpl.h>

#include "anari_library_filament_export.h"

namespace filament {
class Engine;
class Renderer;
class Scene;
class View;
}

namespace AnariFilament {

class FILAMENT_ANARI_EXPORT Device : public anari::DeviceImpl {
public:
    /** Backend values: 0=DEFAULT, 1=OPENGL, 2=VULKAN, 3=METAL, 5=NOOP */
    Device(ANARILibrary library, int backend = 0);
    ~Device() override;

    ANARIArray1D newArray1D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType type,
        uint64_t numItems1) override;

    ANARIArray2D newArray2D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType type,
        uint64_t numItems1,
        uint64_t numItems2) override;

    ANARIArray3D newArray3D(const void *appMemory,
        ANARIMemoryDeleter deleter,
        const void *userdata,
        ANARIDataType type,
        uint64_t numItems1,
        uint64_t numItems2,
        uint64_t numItems3) override;

    ANARIGeometry newGeometry(const char *type) override;
    ANARIMaterial newMaterial(const char *type) override;
    ANARISampler newSampler(const char *type) override;
    ANARISurface newSurface() override;
    ANARISpatialField newSpatialField(const char *type) override;
    ANARIVolume newVolume(const char *type) override;
    ANARILight newLight(const char *type) override;
    ANARIGroup newGroup() override;
    ANARIInstance newInstance(const char *type) override;
    ANARIWorld newWorld() override;
    ANARICamera newCamera(const char *type) override;
    ANARIRenderer newRenderer(const char *type) override;
    ANARIFrame newFrame() override;

    void setParameter(ANARIObject object,
        const char *name,
        ANARIDataType type,
        const void *mem) override;
    void unsetParameter(ANARIObject object, const char *name) override;
    void unsetAllParameters(ANARIObject object) override;

    void *mapParameterArray1D(ANARIObject object,
        const char *name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t *elementStride) override;
    void *mapParameterArray2D(ANARIObject object,
        const char *name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t numElements2,
        uint64_t *elementStride) override;
    void *mapParameterArray3D(ANARIObject object,
        const char *name,
        ANARIDataType dataType,
        uint64_t numElements1,
        uint64_t numElements2,
        uint64_t numElements3,
        uint64_t *elementStride) override;
    void unmapParameterArray(ANARIObject object, const char *name) override;

    void commitParameters(ANARIObject object) override;
    void release(ANARIObject object) override;
    void retain(ANARIObject object) override;

    void *mapArray(ANARIArray) override;
    void unmapArray(ANARIArray) override;

    int getProperty(ANARIObject object,
        const char *name,
        ANARIDataType type,
        void *mem,
        uint64_t size,
        ANARIWaitMask mask) override;

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

    const void *frameBufferMap(ANARIFrame fb,
        const char *channel,
        uint32_t *width,
        uint32_t *height,
        ANARIDataType *pixelType) override;
    void frameBufferUnmap(ANARIFrame fb, const char *channel) override;

    void renderFrame(ANARIFrame frame) override;
    int frameReady(ANARIFrame frame, ANARIWaitMask mask) override;
    void discardFrame(ANARIFrame frame) override;

    filament::Engine *engine() const { return mEngine; }
    filament::Renderer *renderer() const { return mRenderer; }

private:
    filament::Engine *mEngine = nullptr;
    filament::Renderer *mRenderer = nullptr;
    filament::Scene *mScene = nullptr;
    filament::View *mView = nullptr;
};

}
