// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Device.h"

#include <anari/backend/LibraryImpl.h>

#include "anari_library_halogen_export.h"

namespace Halogen {

struct Library : public anari::LibraryImpl {
    Library(void *lib, ANARIStatusCallback defaultStatusCB,
        const void *statusCBPtr);

    ANARIDevice newDevice(const char *subtype) override;
    const char **getDeviceExtensions(const char *deviceType) override;
};

Library::Library(void *lib, ANARIStatusCallback defaultStatusCB,
    const void *statusCBPtr)
    : anari::LibraryImpl(lib, defaultStatusCB, statusCBPtr) {}

ANARIDevice Library::newDevice(const char *) {
    auto *d = new Device(this_library());
    return d->this_device();
}

const char **Library::getDeviceExtensions(const char *) {
    static const char *extensions[] = {
        "HALOGEN_NATIVE_SURFACE",
        "HALOGEN_MATERIAL_UNLIT",
        // Halogen implements the compressedImage2D sampler subtype. Which
        // block formats the *actual* GPU accepts is a per-device question and
        // is reported by the "halogen.textureFormats" device property (the
        // Filament engine, needed to probe support, does not exist yet here).
        "EXT_SAMPLER_COMPRESSED_IMAGE2D",
        nullptr
    };
    return extensions;
}

}

extern "C" HALOGEN_EXPORT ANARI_DEFINE_LIBRARY_ENTRYPOINT(
    halogen, handle, scb, scbPtr) {
    return reinterpret_cast<ANARILibrary>(new Halogen::Library(handle, scb, scbPtr));
}
