// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#include <cstring>

namespace {

struct DeviceFixture {
    ANARILibrary library = nullptr;
    ANARIDevice device = nullptr;

    DeviceFixture() {
        auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
            ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
            const char *message) {
            if (severity <= ANARI_SEVERITY_WARNING)
                Corrade::Utility::Debug{} << "ANARI:" << message;
        };
        library = anariLoadLibrary("filament", statusFunc);
        if (library)
            device = anariNewDevice(library, "default");
        if (device)
            anariCommitParameters(device, device);
    }

    ~DeviceFixture() {
        if (device) anariRelease(device, device);
        if (library) anariUnloadLibrary(library);
    }
};

struct NativeSurfaceTest : Corrade::TestSuite::Tester {
    explicit NativeSurfaceTest();

    void extensionAdvertised();
    void createObject();
    void attachToFrame();
};

NativeSurfaceTest::NativeSurfaceTest() {
    addTests({&NativeSurfaceTest::extensionAdvertised,
        &NativeSurfaceTest::createObject,
        &NativeSurfaceTest::attachToFrame});
}

void NativeSurfaceTest::extensionAdvertised() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    const char *const *extensions =
        anariGetDeviceExtensions(f.library, "default");
    CORRADE_VERIFY(extensions);

    bool found = false;
    for (const char *const *ext = extensions; *ext; ++ext) {
        if (std::strcmp(*ext, "FILAMENT_NATIVE_SURFACE") == 0) {
            found = true;
            break;
        }
    }
    CORRADE_VERIFY(found);
}

void NativeSurfaceTest::createObject() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIObject ns = anariNewObject(f.device, "nativeSurface", "default");
    CORRADE_VERIFY(ns);

    anariCommitParameters(f.device, ns);
    anariRelease(f.device, ns);
}

void NativeSurfaceTest::attachToFrame() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Create a native surface (without a real window handle)
    ANARIObject ns = anariNewObject(f.device, "nativeSurface", "default");
    CORRADE_VERIFY(ns);
    anariCommitParameters(f.device, ns);

    // Attach to a frame — should accept the parameter without crashing
    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIWorld world = anariNewWorld(f.device);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {64, 64};
    anariSetParameter(f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(f.device, frame, "world", ANARI_WORLD, &world);
    anariSetParameter(f.device, frame, "nativeSurface", ANARI_OBJECT, &ns);
    anariCommitParameters(f.device, frame);

    // Render — the native surface has no valid window handle, so the frame
    // should fall back to the offscreen path.
    anariRenderFrame(f.device, frame);
    int ready = anariFrameReady(f.device, frame, ANARI_WAIT);
    CORRADE_VERIFY(ready);

    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, world);
    anariRelease(f.device, camera);
    anariRelease(f.device, ns);
}

}

CORRADE_TEST_MAIN(NativeSurfaceTest)
