// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Device.h"

#include <anari/backend/LibraryImpl.h>

#include "anari_library_filament_export.h"

namespace AnariFilament {

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
    return (ANARIDevice) new Device(this_library());
}

const char **Library::getDeviceExtensions(const char *) {
    return nullptr;
}

}

extern "C" FILAMENT_ANARI_EXPORT ANARI_DEFINE_LIBRARY_ENTRYPOINT(
    filament, handle, scb, scbPtr) {
    return (ANARILibrary) new AnariFilament::Library(handle, scb, scbPtr);
}
