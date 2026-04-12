// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

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

struct LightTest : Corrade::TestSuite::Tester {
    explicit LightTest();

    void directionalLight();
    void pointLight();
    void spotLight();
    void pointLightRender();
};

LightTest::LightTest()
{
    addTests({&LightTest::directionalLight,
        &LightTest::pointLight,
        &LightTest::spotLight,
        &LightTest::pointLightRender});
}

void LightTest::directionalLight()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARILight light = anariNewLight(f.device, "directional");
    CORRADE_VERIFY(light);

    float direction[] = {0.0f, -1.0f, 0.0f};
    float color[] = {1.0f, 0.8f, 0.6f};
    float irradiance = 2.0f;
    anariSetParameter(
        f.device, light, "direction", ANARI_FLOAT32_VEC3, direction);
    anariSetParameter(
        f.device, light, "color", ANARI_FLOAT32_VEC3, color);
    anariSetParameter(
        f.device, light, "irradiance", ANARI_FLOAT32, &irradiance);
    anariCommitParameters(f.device, light);

    anariRelease(f.device, light);
}

void LightTest::pointLight()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARILight light = anariNewLight(f.device, "point");
    CORRADE_VERIFY(light);

    float position[] = {1.0f, 2.0f, 3.0f};
    float color[] = {1.0f, 0.0f, 0.0f};
    float intensity = 10.0f;
    anariSetParameter(
        f.device, light, "position", ANARI_FLOAT32_VEC3, position);
    anariSetParameter(
        f.device, light, "color", ANARI_FLOAT32_VEC3, color);
    anariSetParameter(
        f.device, light, "intensity", ANARI_FLOAT32, &intensity);
    anariCommitParameters(f.device, light);

    anariRelease(f.device, light);
}

void LightTest::spotLight()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARILight light = anariNewLight(f.device, "spot");
    CORRADE_VERIFY(light);

    float position[] = {0.0f, 5.0f, 0.0f};
    float direction[] = {0.0f, -1.0f, 0.0f};
    float color[] = {1.0f, 1.0f, 1.0f};
    float intensity = 50.0f;
    float openingAngle = 0.785f;
    float falloffAngle = 0.1f;
    anariSetParameter(
        f.device, light, "position", ANARI_FLOAT32_VEC3, position);
    anariSetParameter(
        f.device, light, "direction", ANARI_FLOAT32_VEC3, direction);
    anariSetParameter(
        f.device, light, "color", ANARI_FLOAT32_VEC3, color);
    anariSetParameter(
        f.device, light, "intensity", ANARI_FLOAT32, &intensity);
    anariSetParameter(
        f.device, light, "openingAngle", ANARI_FLOAT32, &openingAngle);
    anariSetParameter(
        f.device, light, "falloffAngle", ANARI_FLOAT32, &falloffAngle);
    anariCommitParameters(f.device, light);

    anariRelease(f.device, light);
}

void LightTest::pointLightRender()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Create a simple scene with a point light to verify rendering completes
    float vertices[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         0.0f,  1.0f, -3.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertices, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariCommitParameters(f.device, mat);

    ANARISurface surface = anariNewSurface(f.device);
    anariSetParameter(
        f.device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(
        f.device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surface);

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, world, "surface", ANARI_ARRAY1D, &surfArr);

    ANARILight light = anariNewLight(f.device, "point");
    float lightPos[] = {0.0f, 2.0f, 0.0f};
    float intensity = 50.0f;
    anariSetParameter(
        f.device, light, "position", ANARI_FLOAT32_VEC3, lightPos);
    anariSetParameter(
        f.device, light, "intensity", ANARI_FLOAT32, &intensity);
    anariCommitParameters(f.device, light);

    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {32, 32};
    anariSetParameter(f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(f.device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(f.device, frame);

    anariRenderFrame(f.device, frame);
    int ready = anariFrameReady(f.device, frame, ANARI_WAIT);
    CORRADE_VERIFY(ready);

    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, camera);
    anariRelease(f.device, lightArr);
    anariRelease(f.device, light);
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, geom);
    anariRelease(f.device, posArr);
}

}

CORRADE_TEST_MAIN(LightTest)
