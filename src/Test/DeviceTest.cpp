// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

namespace {

struct DeviceTest : Corrade::TestSuite::Tester {
    explicit DeviceTest();

    void createDestroy();
};

DeviceTest::DeviceTest() {
    addTests({&DeviceTest::createDestroy});
}

void DeviceTest::createDestroy() {
    /* Loading and creating a device through the ANARI API should work */
    auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
        ANARIDataType, ANARIStatusSeverity, ANARIStatusCode,
        const char *message) {
        Corrade::Utility::Debug{} << "ANARI:" << message;
    };
    ANARILibrary library = anariLoadLibrary("filament", statusFunc);
    CORRADE_VERIFY(library);

    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);

    anariRelease(device, device);
    anariUnloadLibrary(library);
}

}

CORRADE_TEST_MAIN(DeviceTest)
