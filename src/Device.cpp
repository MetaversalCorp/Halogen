// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Device.h"

#include "Camera.h"
#include "Frame.h"
#include "Geometry.h"
#include "NativeSurface.h"
#include "Group.h"
#include "Instance.h"
#include "Light.h"
#include "Material.h"
#include "Renderer.h"
#include "Sampler.h"
#include "Surface.h"
#include "World.h"

#include <filament/Engine.h>
#include <filament/Material.h>
#include <filament/Renderer.h>
#include <filament/Texture.h>
#include <backend/PixelBufferDescriptor.h>

#include <algorithm>
#include <cstring>

#include <helium/BaseObject.h>
#include <helium/array/Array1D.h>
#include <helium/array/Array2D.h>
#include <helium/array/Array3D.h>
#include <helium/array/ObjectArray.h>

#include <cstdlib>
#include <cstring>

#include "matte_mat.h"
#include "matteBlend_mat.h"
#include "matteMasked_mat.h"
#include "physicallyBased_mat.h"
#include "physicallyBasedBlend_mat.h"
#include "physicallyBasedMasked_mat.h"

namespace AnariFilament {

// -- Array management --

void *Device::mapArray(ANARIArray a)
{
    return helium::BaseDevice::mapArray(a);
}

void Device::unmapArray(ANARIArray a)
{
    helium::BaseDevice::unmapArray(a);
}

// -- API Objects --

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
        return reinterpret_cast<ANARIArray1D>(new helium::ObjectArray(
            deviceState(), md));
    else
        return reinterpret_cast<ANARIArray1D>(new helium::Array1D(
            deviceState(), md));
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
    return reinterpret_cast<ANARIArray2D>(new helium::Array2D(deviceState(), md));
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
    return reinterpret_cast<ANARIArray3D>(new helium::Array3D(deviceState(), md));
}

ANARICamera Device::newCamera(const char *subtype)
{
    initDevice();
    return reinterpret_cast<ANARICamera>(new Camera(deviceState(), subtype));
}

ANARIFrame Device::newFrame()
{
    initDevice();
    return reinterpret_cast<ANARIFrame>(new Frame(deviceState()));
}

ANARIGeometry Device::newGeometry(const char *subtype)
{
    initDevice();
    return reinterpret_cast<ANARIGeometry>(new Geometry(deviceState(), subtype));
}

ANARIGroup Device::newGroup()
{
    initDevice();
    return reinterpret_cast<ANARIGroup>(new Group(deviceState()));
}

ANARIInstance Device::newInstance(const char *)
{
    initDevice();
    return reinterpret_cast<ANARIInstance>(new Instance(deviceState()));
}

ANARILight Device::newLight(const char *subtype)
{
    initDevice();
    return reinterpret_cast<ANARILight>(new Light(deviceState(), subtype));
}

ANARIMaterial Device::newMaterial(const char *subtype)
{
    initDevice();
    return reinterpret_cast<ANARIMaterial>(new Material(deviceState(), subtype));
}

ANARIRenderer Device::newRenderer(const char *)
{
    initDevice();
    return reinterpret_cast<ANARIRenderer>(new AnariFilament::Renderer(deviceState()));
}

ANARISampler Device::newSampler(const char *subtype)
{
    initDevice();
    return reinterpret_cast<ANARISampler>(new Sampler(deviceState(), subtype));
}

ANARISpatialField Device::newSpatialField(const char *)
{
    return reinterpret_cast<ANARISpatialField>(new UnknownObject(
        ANARI_SPATIAL_FIELD, deviceState()));
}

ANARISurface Device::newSurface()
{
    initDevice();
    return reinterpret_cast<ANARISurface>(new Surface(deviceState()));
}

ANARIVolume Device::newVolume(const char *)
{
    return reinterpret_cast<ANARIVolume>(new UnknownObject(
        ANARI_VOLUME, deviceState()));
}

ANARIWorld Device::newWorld()
{
    initDevice();
    return reinterpret_cast<ANARIWorld>(new World(deviceState()));
}

// -- Extension objects --

ANARIObject Device::newObject(const char *objectType, const char *type)
{
    if (std::string_view(objectType) == "nativeSurface") {
        initDevice();
        return reinterpret_cast<ANARIObject>(
            new NativeSurface(deviceState()));
    }
    return helium::BaseDevice::newObject(objectType, type);
}

// -- Query functions --

namespace {

const char *geometrySubtypes[] = {
    "triangle", "sphere", "cylinder", "curve", "quad", "cone", nullptr};
const char *cameraSubtypes[] = {
    "perspective", "orthographic", nullptr};
const char *lightSubtypes[] = {
    "directional", "point", "spot", nullptr};
const char *materialSubtypes[] = {
    "matte", "physicallyBased", nullptr};
const char *samplerSubtypes[] = {
    "image1D", "image2D", "image3D", "transform", "primitive", nullptr};
const char *rendererSubtypes[] = {
    "default", nullptr};
const char *volumeSubtypes[] = {nullptr};
const char *spatialFieldSubtypes[] = {nullptr};

}

const char **Device::getObjectSubtypes(ANARIDataType objectType)
{
    switch (objectType) {
    case ANARI_GEOMETRY: return geometrySubtypes;
    case ANARI_CAMERA: return cameraSubtypes;
    case ANARI_LIGHT: return lightSubtypes;
    case ANARI_MATERIAL: return materialSubtypes;
    case ANARI_SAMPLER: return samplerSubtypes;
    case ANARI_RENDERER: return rendererSubtypes;
    case ANARI_VOLUME: return volumeSubtypes;
    case ANARI_SPATIAL_FIELD: return spatialFieldSubtypes;
    default: return nullptr;
    }
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

// -- Lifecycle --

Device::Device(ANARIStatusCallback cb, const void *userPtr)
    : helium::BaseDevice(cb, userPtr) {}

Device::Device(ANARILibrary l)
    : helium::BaseDevice(l) {}

Device::~Device()
{
    DeviceState * const state = deviceState();
    if (state)
        state->commitBuffer.clear();
}

void Device::initDevice()
{
    if (mInitialized)
        return;
    mInitialized = true;

    m_state = std::make_unique<DeviceState>(this_device());
    DeviceState * const state = deviceState();

    auto backend = filament::Engine::Backend::DEFAULT;
    {
        auto backendStr = getParamString("backend", "");
        if (backendStr.empty()) {
#ifdef _MSC_VER
            char *envBackend = nullptr;
            size_t len = 0;
            _dupenv_s(&envBackend, &len, "FILAMENT_BACKEND");
#else
            const char *envBackend = std::getenv("FILAMENT_BACKEND");
#endif
            if (envBackend)
                backendStr = envBackend;
#ifdef _MSC_VER
            free(envBackend);
#endif
        }
        if (backendStr == "opengl")
            backend = filament::Engine::Backend::OPENGL;
        else if (backendStr == "vulkan")
            backend = filament::Engine::Backend::VULKAN;
        else if (backendStr == "metal")
            backend = filament::Engine::Backend::METAL;
        else if (backendStr == "noop")
            backend = filament::Engine::Backend::NOOP;
    }

    state->engine = filament::Engine::create(backend);
    if (!state->engine) {
        reportMessage(ANARI_SEVERITY_FATAL_ERROR,
            "failed to create Filament engine");
        return;
    }

    state->renderer = {state->engine,
        state->engine->createRenderer()};

    state->matteMaterial = {state->engine,
        filament::Material::Builder()
            .package(MATTE_MAT_DATA, MATTE_MAT_SIZE)
            .build(*state->engine)};

    state->matteBlendMaterial = {state->engine,
        filament::Material::Builder()
            .package(MATTEBLEND_MAT_DATA, MATTEBLEND_MAT_SIZE)
            .build(*state->engine)};

    state->matteMaskedMaterial = {state->engine,
        filament::Material::Builder()
            .package(MATTEMASKED_MAT_DATA, MATTEMASKED_MAT_SIZE)
            .build(*state->engine)};

    state->physicallyBasedMaterial = {state->engine,
        filament::Material::Builder()
            .package(PHYSICALLYBASED_MAT_DATA, PHYSICALLYBASED_MAT_SIZE)
            .build(*state->engine)};

    state->physicallyBasedBlendMaterial = {state->engine,
        filament::Material::Builder()
            .package(PHYSICALLYBASEDBLEND_MAT_DATA,
                PHYSICALLYBASEDBLEND_MAT_SIZE)
            .build(*state->engine)};

    state->physicallyBasedMaskedMaterial = {state->engine,
        filament::Material::Builder()
            .package(PHYSICALLYBASEDMASKED_MAT_DATA,
                PHYSICALLYBASEDMASKED_MAT_SIZE)
            .build(*state->engine)};

    // 1x1 white dummy texture for unused sampler parameters (required by Metal)
    state->dummyTexture = filament::Texture::Builder()
        .width(1)
        .height(1)
        .levels(1)
        .format(filament::Texture::InternalFormat::RGBA8)
        .sampler(filament::Texture::Sampler::SAMPLER_2D)
        .build(*state->engine);
    auto *white = new uint8_t[4]{255, 255, 255, 255};
    state->dummyTexture->setImage(*state->engine, 0,
        filament::backend::PixelBufferDescriptor(
            white, 4,
            filament::backend::PixelDataFormat::RGBA,
            filament::backend::PixelDataType::UBYTE,
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint8_t *>(buf);
            }));
}

void Device::deviceCommitParameters()
{
    initDevice();
}

namespace {

const char *backendString(filament::Engine::Backend b)
{
    switch (b) {
    case filament::Engine::Backend::OPENGL: return "opengl";
    case filament::Engine::Backend::VULKAN: return "vulkan";
    case filament::Engine::Backend::METAL: return "metal";
    case filament::Engine::Backend::NOOP: return "noop";
    default: return "unknown";
    }
}

}

int Device::deviceGetProperty(const char *name, ANARIDataType type,
    void *mem, uint64_t size, uint32_t mask)
{
    std::string_view prop(name);

    if (prop == "filament.backend" && type == ANARI_STRING) {
        if (mask & ANARI_WAIT)
            initDevice();
        if (!mInitialized)
            return 0;
        auto str = backendString(deviceState()->engine->getBackend());
        std::memset(mem, 0, size);
        std::memcpy(mem, str,
            std::min(uint64_t(std::strlen(str)), size - 1));
        return 1;
    }

    if (prop == "filament.backend.size" && type == ANARI_UINT64) {
        if (mask & ANARI_WAIT)
            initDevice();
        if (!mInitialized)
            return 0;
        auto str = backendString(deviceState()->engine->getBackend());
        helium::writeToVoidP(mem, uint64_t(std::strlen(str) + 1));
        return 1;
    }

    return 0;
}

DeviceState *Device::deviceState() const
{
    return static_cast<DeviceState *>(m_state.get());
}

}
