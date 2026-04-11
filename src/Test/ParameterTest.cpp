// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#include <cstring>

namespace {

struct ParameterTest : Corrade::TestSuite::Tester {
    explicit ParameterTest();

    void cameraDefaults();
    void cameraExposure();
    void frameInvalidWithoutRequiredParams();
    void frameValidWithAllParams();
    void geometryBindsBuffers();
    void materialVertexColor();
};

ParameterTest::ParameterTest() {
    addTests({&ParameterTest::cameraDefaults,
        &ParameterTest::cameraExposure,
        &ParameterTest::frameInvalidWithoutRequiredParams,
        &ParameterTest::frameValidWithAllParams,
        &ParameterTest::geometryBindsBuffers,
        &ParameterTest::materialVertexColor});
}

struct DeviceFixture {
    ANARILibrary library = nullptr;
    ANARIDevice device = nullptr;

    DeviceFixture() {
        auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
            ANARIDataType, ANARIStatusSeverity, ANARIStatusCode,
            const char *) {};
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

void ParameterTest::cameraDefaults() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);

    // Default parameters should commit without errors
    anariCommitParameters(f.device, camera);

    anariRelease(f.device, camera);
}

void ParameterTest::cameraExposure() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);

    // Custom exposure value should be accepted
    float exposure = 2.5f;
    anariSetParameter(
        f.device, camera, "exposure", ANARI_FLOAT32, &exposure);
    anariCommitParameters(f.device, camera);

    anariRelease(f.device, camera);
}

void ParameterTest::frameInvalidWithoutRequiredParams() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIFrame frame = anariNewFrame(f.device);
    CORRADE_VERIFY(frame);

    // Frame without size/camera/world should not crash on render
    anariCommitParameters(f.device, frame);
    anariRenderFrame(f.device, frame);

    // frameReady should still return (frame was invalid, render is a no-op)
    int ready = anariFrameReady(f.device, frame, ANARI_WAIT);
    CORRADE_COMPARE(ready, 0);

    anariRelease(f.device, frame);
}

void ParameterTest::frameValidWithAllParams() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIWorld world = anariNewWorld(f.device);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {64, 64};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(
        f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        f.device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(
        f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(
        f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(
        f.device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(f.device, frame);

    anariRenderFrame(f.device, frame);
    anariFrameReady(f.device, frame, ANARI_WAIT);

    uint32_t width = 0, height = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = anariMapFrame(
        f.device, frame, "channel.color", &width, &height, &fbType);
    CORRADE_COMPARE(width, 64u);
    CORRADE_COMPARE(height, 64u);
    CORRADE_VERIFY(fb != nullptr);
    anariUnmapFrame(f.device, frame, "channel.color");

    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, world);
    anariRelease(f.device, camera);
}

void ParameterTest::geometryBindsBuffers() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    float vertices[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    uint32_t indices[] = {0, 1, 2};

    ANARIArray1D posArray = anariNewArray1D(
        f.device, vertices, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D idxArray = anariNewArray1D(
        f.device, indices, nullptr, nullptr, ANARI_UINT32_VEC3, 1);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    CORRADE_VERIFY(geom);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArray);
    anariSetParameter(
        f.device, geom, "primitive.index", ANARI_ARRAY1D, &idxArray);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, geom);
    anariRelease(f.device, idxArray);
    anariRelease(f.device, posArray);
}

void ParameterTest::materialVertexColor() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    CORRADE_VERIFY(mat);

    // Setting color to "color" string requests vertex-color mode
    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    // Setting a solid color value
    float solidColor[] = {1.0f, 0.0f, 0.0f};
    anariSetParameter(
        f.device, mat, "color", ANARI_FLOAT32_VEC3, solidColor);
    anariCommitParameters(f.device, mat);

    anariRelease(f.device, mat);
}

}

CORRADE_TEST_MAIN(ParameterTest)
