// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <cstring>
#include <string>

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>
#include <Corrade/Utility/DebugStl.h>

#include <anari/anari.h>

namespace {

struct DeviceTest : Corrade::TestSuite::Tester {
    explicit DeviceTest();

    void createDestroy();
    void compressedFormatIntrospection();
};

DeviceTest::DeviceTest() {
    addTests({&DeviceTest::createDestroy,
        &DeviceTest::compressedFormatIntrospection});
}

namespace {

bool stringListContains(const char **list, const char *needle) {
    for (int i = 0; list && list[i]; ++i)
        if (std::strcmp(list[i], needle) == 0)
            return true;
    return false;
}

}

void DeviceTest::createDestroy() {
    /* Loading and creating a device through the ANARI API should work */
    auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
        ANARIDataType, ANARIStatusSeverity, ANARIStatusCode,
        const char *message) {
        Corrade::Utility::Debug{} << "ANARI:" << message;
    };
    ANARILibrary library = anariLoadLibrary("halogen", statusFunc);
    CORRADE_VERIFY(library);

    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);

    anariRelease(device, device);
    anariUnloadLibrary(library);
}

void DeviceTest::compressedFormatIntrospection() {
    /* Halogen reports compressed-texture support through the custom
       "halogen.textureFormats" device property probed against the live
       Filament backend (commit 804a0a9), NOT through the standard
       getParameterInfo() value list. This exercises the query plumbing at
       the public-API surface; the concrete format set is GPU-dependent, so
       we assert only well-formedness, never specific tokens. */
    auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
        ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
        const char *message) {
        if (severity <= ANARI_SEVERITY_WARNING)
            Corrade::Utility::Debug{} << "ANARI:" << message;
    };
    ANARILibrary library = anariLoadLibrary("halogen", statusFunc);
    CORRADE_VERIFY(library);

    /* Extension + subtype are advertised deterministically, no GPU probe. */
    const char **extensions = anariGetDeviceExtensions(library, "default");
    CORRADE_VERIFY(
        stringListContains(extensions, "EXT_SAMPLER_COMPRESSED_IMAGE2D"));

    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    const char **samplers = anariGetObjectSubtypes(device, ANARI_SAMPLER);
    CORRADE_VERIFY(stringListContains(samplers, "compressedImage2D"));

    /* Ask for the size first, then the string. The backend may legitimately
       report an empty list on headless HW; that is still well-formed (size ==
       1 for the terminator). */
    uint64_t size = 0;
    int gotSize = anariGetProperty(device, device,
        "halogen.textureFormats.size", ANARI_UINT64,
        &size, sizeof(size), ANARI_WAIT);

    std::string propFormats;
    if (gotSize) {
        CORRADE_VERIFY(size >= 1); /* includes the null terminator */
        propFormats.resize(size);
        int gotStr = anariGetProperty(device, device,
            "halogen.textureFormats", ANARI_STRING,
            &propFormats[0], size, ANARI_WAIT);
        CORRADE_VERIFY(gotStr);
        propFormats.resize(std::strlen(propFormats.c_str())); /* drop NULs */
    } else {
        Corrade::Utility::Debug{}
            << "backend did not initialize; skipping format-content checks";
    }

    /* Standard ANARI introspection now returns the GPU-supported 'format'
       subset for compressedImage2D, and it must agree with the comma-separated
       'halogen.textureFormats' property -- both derive from the same probe. */
    const auto *formatValues = static_cast<const char *const *>(
        anariGetParameterInfo(device, ANARI_SAMPLER, "compressedImage2D",
            "format", ANARI_STRING, "value", ANARI_STRING_LIST));

    if (gotSize) {
        CORRADE_VERIFY(formatValues);
        std::string joined;
        for (int i = 0; formatValues[i]; ++i) {
            if (!joined.empty())
                joined += ",";
            joined += formatValues[i];
        }
        CORRADE_COMPARE(joined, propFormats);
    }

    anariRelease(device, device);
    anariUnloadLibrary(library);
}

}

CORRADE_TEST_MAIN(DeviceTest)
