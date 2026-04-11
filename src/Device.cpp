// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Device.h"

#include "Camera.h"
#include "Frame.h"
#include "Geometry.h"
#include "Light.h"
#include "Material.h"
#include "Renderer.h"
#include "Surface.h"
#include "World.h"

#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/Renderer.h>

#include <helium/array/Array1D.h>
#include <helium/array/Array2D.h>
#include <helium/array/Array3D.h>
#include <helium/array/ObjectArray.h>

#include "matte_mat.h"

namespace AnariFilament {

// Array management ///////////////////////////////////////////////////////////

void *Device::mapArray(ANARIArray a)
{
    return helium::BaseDevice::mapArray(a);
}

void Device::unmapArray(ANARIArray a)
{
    helium::BaseDevice::unmapArray(a);
}

// API Objects ////////////////////////////////////////////////////////////////

ANARIArray1D Device::newArray1D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems)
{
    initDevice();
    helium::Array1DMemoryDescriptor md;
    md.appMemory = appMemory;
    md.deleter = deleter;
    md.deleterPtr = userData;
    md.elementType = type;
    md.numItems = numItems;
    if (anari::isObject(type))
        return (ANARIArray1D) new helium::ObjectArray(
            deviceState(), md);
    else
        return (ANARIArray1D) new helium::Array1D(
            deviceState(), md);
}

ANARIArray2D Device::newArray2D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems1,
    uint64_t numItems2)
{
    initDevice();
    helium::Array2DMemoryDescriptor md;
    md.appMemory = appMemory;
    md.deleter = deleter;
    md.deleterPtr = userData;
    md.elementType = type;
    md.numItems1 = numItems1;
    md.numItems2 = numItems2;
    return (ANARIArray2D) new helium::Array2D(deviceState(), md);
}

ANARIArray3D Device::newArray3D(const void *appMemory,
    ANARIMemoryDeleter deleter,
    const void *userData,
    ANARIDataType type,
    uint64_t numItems1,
    uint64_t numItems2,
    uint64_t numItems3)
{
    initDevice();
    helium::Array3DMemoryDescriptor md;
    md.appMemory = appMemory;
    md.deleter = deleter;
    md.deleterPtr = userData;
    md.elementType = type;
    md.numItems1 = numItems1;
    md.numItems2 = numItems2;
    md.numItems3 = numItems3;
    return (ANARIArray3D) new helium::Array3D(deviceState(), md);
}

ANARICamera Device::newCamera(const char *)
{
    initDevice();
    return (ANARICamera) new Camera(deviceState());
}

ANARIFrame Device::newFrame()
{
    initDevice();
    return (ANARIFrame) new Frame(deviceState());
}

ANARIGeometry Device::newGeometry(const char *)
{
    initDevice();
    return (ANARIGeometry) new Geometry(deviceState());
}

ANARIGroup Device::newGroup()
{
    initDevice();
    return (ANARIGroup) new UnknownObject(ANARI_GROUP, deviceState());
}

ANARIInstance Device::newInstance(const char *)
{
    initDevice();
    return (ANARIInstance) new UnknownObject(
        ANARI_INSTANCE, deviceState());
}

ANARILight Device::newLight(const char *)
{
    initDevice();
    return (ANARILight) new Light(deviceState());
}

ANARIMaterial Device::newMaterial(const char *)
{
    initDevice();
    return (ANARIMaterial) new Material(deviceState());
}

ANARIRenderer Device::newRenderer(const char *)
{
    initDevice();
    return (ANARIRenderer) new AnariFilament::Renderer(deviceState());
}

ANARISampler Device::newSampler(const char *)
{
    return (ANARISampler) new UnknownObject(
        ANARI_SAMPLER, deviceState());
}

ANARISpatialField Device::newSpatialField(const char *)
{
    return (ANARISpatialField) new UnknownObject(
        ANARI_SPATIAL_FIELD, deviceState());
}

ANARISurface Device::newSurface()
{
    initDevice();
    return (ANARISurface) new Surface(deviceState());
}

ANARIVolume Device::newVolume(const char *)
{
    return (ANARIVolume) new UnknownObject(
        ANARI_VOLUME, deviceState());
}

ANARIWorld Device::newWorld()
{
    initDevice();
    return (ANARIWorld) new World(deviceState());
}

// Query functions ////////////////////////////////////////////////////////////

const char **Device::getObjectSubtypes(ANARIDataType)
{
    return nullptr;
}

const void *Device::getObjectInfo(ANARIDataType, const char *,
    const char *, ANARIDataType)
{
    return nullptr;
}

const void *Device::getParameterInfo(ANARIDataType, const char *,
    const char *, ANARIDataType, const char *, ANARIDataType)
{
    return nullptr;
}

// Lifecycle //////////////////////////////////////////////////////////////////

Device::Device(ANARIStatusCallback cb, const void *userPtr)
    : helium::BaseDevice(cb, userPtr) {}

Device::Device(ANARILibrary l)
    : helium::BaseDevice(l) {}

Device::~Device()
{
    auto *state = deviceState();
    if (state)
        state->commitBuffer.clear();
}

void Device::initDevice()
{
    if (mInitialized)
        return;
    mInitialized = true;

    m_state = std::make_unique<DeviceState>(this_device());
    auto *state = deviceState();

    state->engine = filament::Engine::create(
        filament::Engine::Backend::DEFAULT);
    if (!state->engine) {
        reportMessage(ANARI_SEVERITY_FATAL_ERROR,
            "failed to create Filament engine");
        return;
    }

    state->renderer = state->engine->createRenderer();

    // Compile the matte material from the embedded binary
    state->matteMaterial = filament::Material::Builder()
        .package(MATTE_MAT_DATA, MATTE_MAT_SIZE)
        .build(*state->engine);
}

void Device::deviceCommitParameters()
{
    initDevice();
}

int Device::deviceGetProperty(const char *, ANARIDataType,
    void *, uint64_t, uint32_t)
{
    return 0;
}

DeviceState *Device::deviceState() const
{
    return static_cast<DeviceState *>(m_state.get());
}

}
