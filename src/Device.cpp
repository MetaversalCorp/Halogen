// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Device.h"

#include <filament/Engine.h>
#include <filament/Renderer.h>
#include <filament/Scene.h>
#include <filament/View.h>

namespace AnariFilament {

Device::Device(ANARILibrary library, int backend)
    : DeviceImpl(library) {
    mEngine = filament::Engine::create(
        static_cast<filament::Engine::Backend>(backend));
    if (mEngine) {
        mRenderer = mEngine->createRenderer();
        mScene = mEngine->createScene();
        mView = mEngine->createView();
        mView->setScene(mScene);
    }
}

Device::~Device() {
    if (mEngine) {
        mEngine->destroy(mView);
        mEngine->destroy(mScene);
        mEngine->destroy(mRenderer);
        filament::Engine::destroy(&mEngine);
    }
}

ANARIArray1D Device::newArray1D(const void *, ANARIMemoryDeleter,
    const void *, ANARIDataType, uint64_t) {
    return nullptr;
}

ANARIArray2D Device::newArray2D(const void *, ANARIMemoryDeleter,
    const void *, ANARIDataType, uint64_t, uint64_t) {
    return nullptr;
}

ANARIArray3D Device::newArray3D(const void *, ANARIMemoryDeleter,
    const void *, ANARIDataType, uint64_t, uint64_t, uint64_t) {
    return nullptr;
}

ANARIGeometry Device::newGeometry(const char *) { return nullptr; }
ANARIMaterial Device::newMaterial(const char *) { return nullptr; }
ANARISampler Device::newSampler(const char *) { return nullptr; }
ANARISurface Device::newSurface() { return nullptr; }
ANARISpatialField Device::newSpatialField(const char *) { return nullptr; }
ANARIVolume Device::newVolume(const char *) { return nullptr; }
ANARILight Device::newLight(const char *) { return nullptr; }
ANARIGroup Device::newGroup() { return nullptr; }
ANARIInstance Device::newInstance(const char *) { return nullptr; }
ANARIWorld Device::newWorld() { return nullptr; }
ANARICamera Device::newCamera(const char *) { return nullptr; }
ANARIRenderer Device::newRenderer(const char *) { return nullptr; }
ANARIFrame Device::newFrame() { return nullptr; }

void Device::setParameter(ANARIObject, const char *, ANARIDataType, const void *) {}
void Device::unsetParameter(ANARIObject, const char *) {}
void Device::unsetAllParameters(ANARIObject) {}

void *Device::mapParameterArray1D(ANARIObject, const char *, ANARIDataType,
    uint64_t, uint64_t *) {
    return nullptr;
}
void *Device::mapParameterArray2D(ANARIObject, const char *, ANARIDataType,
    uint64_t, uint64_t, uint64_t *) {
    return nullptr;
}
void *Device::mapParameterArray3D(ANARIObject, const char *, ANARIDataType,
    uint64_t, uint64_t, uint64_t, uint64_t *) {
    return nullptr;
}
void Device::unmapParameterArray(ANARIObject, const char *) {}

void Device::commitParameters(ANARIObject) {}
void Device::release(ANARIObject) {}
void Device::retain(ANARIObject) {}

void *Device::mapArray(ANARIArray) { return nullptr; }
void Device::unmapArray(ANARIArray) {}

int Device::getProperty(ANARIObject, const char *, ANARIDataType,
    void *, uint64_t, ANARIWaitMask) {
    return 0;
}

const char **Device::getObjectSubtypes(ANARIDataType) {
    return nullptr;
}

const void *Device::getObjectInfo(ANARIDataType, const char *,
    const char *, ANARIDataType) {
    return nullptr;
}

const void *Device::getParameterInfo(ANARIDataType, const char *,
    const char *, ANARIDataType, const char *, ANARIDataType) {
    return nullptr;
}

const void *Device::frameBufferMap(ANARIFrame, const char *,
    uint32_t *, uint32_t *, ANARIDataType *) {
    return nullptr;
}
void Device::frameBufferUnmap(ANARIFrame, const char *) {}
void Device::renderFrame(ANARIFrame) {}
int Device::frameReady(ANARIFrame, ANARIWaitMask) { return 1; }
void Device::discardFrame(ANARIFrame) {}

}
