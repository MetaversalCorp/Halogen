// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#define _CRT_SECURE_NO_WARNINGS

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#include <cstdio>

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
        library = anariLoadLibrary("halogen", statusFunc);
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

struct RenderTest : Corrade::TestSuite::Tester {
    explicit RenderTest();

    void cameraDefaults();
    void cameraExposure();
    void cameraOrthographic();
    void cameraNearFar();
    void frameInvalidWithoutRequiredParams();
    void frameValidWithAllParams();
    void renderTriangle();
    void groupAndInstance();
    void rendererBackgroundColor();
};

RenderTest::RenderTest()
{
    addTests({&RenderTest::cameraDefaults,
        &RenderTest::cameraExposure,
        &RenderTest::cameraOrthographic,
        &RenderTest::cameraNearFar,
        &RenderTest::frameInvalidWithoutRequiredParams,
        &RenderTest::frameValidWithAllParams,
        &RenderTest::renderTriangle,
        &RenderTest::groupAndInstance,
        &RenderTest::rendererBackgroundColor});
}

void RenderTest::cameraDefaults()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);

    anariCommitParameters(f.device, camera);

    anariRelease(f.device, camera);
}

void RenderTest::cameraExposure()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);

    const float exposure = 2.5f;
    anariSetParameter(
        f.device, camera, "exposure", ANARI_FLOAT32, &exposure);
    anariCommitParameters(f.device, camera);

    anariRelease(f.device, camera);
}

void RenderTest::cameraOrthographic()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "orthographic");
    CORRADE_VERIFY(camera);

    const float aspect = 1.0f;
    const float height = 2.0f;
    const float near = 0.01f;
    const float far = 100.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    anariSetParameter(f.device, camera, "height", ANARI_FLOAT32, &height);
    anariSetParameter(f.device, camera, "near", ANARI_FLOAT32, &near);
    anariSetParameter(f.device, camera, "far", ANARI_FLOAT32, &far);
    anariCommitParameters(f.device, camera);

    // Create a minimal scene to verify orthographic rendering completes
    ANARIWorld world = anariNewWorld(f.device);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {32, 32};
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
    anariRelease(f.device, world);
    anariRelease(f.device, camera);
}

void RenderTest::cameraNearFar()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);

    const float near = 0.5f;
    const float far = 500.0f;
    anariSetParameter(f.device, camera, "near", ANARI_FLOAT32, &near);
    anariSetParameter(f.device, camera, "far", ANARI_FLOAT32, &far);
    anariCommitParameters(f.device, camera);

    anariRelease(f.device, camera);
}

void RenderTest::frameInvalidWithoutRequiredParams()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIFrame frame = anariNewFrame(f.device);
    CORRADE_VERIFY(frame);

    anariCommitParameters(f.device, frame);
    anariRenderFrame(f.device, frame);

    int ready = anariFrameReady(f.device, frame, ANARI_WAIT);
    CORRADE_COMPARE(ready, 0);

    anariRelease(f.device, frame);
}

void RenderTest::frameValidWithAllParams()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIWorld world = anariNewWorld(f.device);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {64, 64};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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

void RenderTest::renderTriangle()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    CORRADE_VERIFY(camera);
    const float aspect = 1024.0f / 768.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0.0f, 0.0f, 0.0f};
    anariSetParameter(f.device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    const float cam_dir[] = {0.0f, 0.0f, -1.0f};
    anariSetParameter(
        f.device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    const float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(f.device, camera);

    const float vertex[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
        -1.0f,  1.0f, -3.0f,
         1.0f,  1.0f, -3.0f};
    const float color[] = {
        0.9f, 0.5f, 0.5f, 1.0f,
        0.8f, 0.8f, 0.8f, 1.0f,
        0.8f, 0.8f, 0.8f, 1.0f,
        0.5f, 0.9f, 0.5f, 1.0f};
    const uint32_t index[] = {0, 1, 2, 1, 3, 2};

    ANARIArray1D posArray = anariNewArray1D(
        f.device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D colArray = anariNewArray1D(
        f.device, color, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    ANARIArray1D idxArray = anariNewArray1D(
        f.device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 2);

    ANARIGeometry mesh = anariNewGeometry(f.device, "triangle");
    anariSetParameter(
        f.device, mesh, "vertex.position", ANARI_ARRAY1D, &posArray);
    anariSetParameter(
        f.device, mesh, "vertex.color", ANARI_ARRAY1D, &colArray);
    anariSetParameter(
        f.device, mesh, "primitive.index", ANARI_ARRAY1D, &idxArray);
    anariCommitParameters(f.device, mesh);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    ANARISurface surface = anariNewSurface(f.device);
    anariSetParameter(
        f.device, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(
        f.device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surface);

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D surfaceArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, world, "surface", ANARI_ARRAY1D, &surfaceArr);

    ANARILight light = anariNewLight(f.device, "directional");
    anariCommitParameters(f.device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {1024, 768};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        f.device, frame, "channel.color", &width, &height, &fbType));

    CORRADE_COMPARE(width, 1024u);
    CORRADE_COMPARE(height, 768u);
    CORRADE_VERIFY(fb != nullptr);

    bool hasNonBlack = false;
    for (uint32_t i = 0; i < width * height * 4; i += 4) {
        if (fb[i] > 0 || fb[i + 1] > 0 || fb[i + 2] > 0) {
            hasNonBlack = true;
            break;
        }
    }
    CORRADE_VERIFY(hasNonBlack);

    FILE *ppm = fopen("tutorial_output.ppm", "wb");
    if (ppm) {
        fprintf(ppm, "P6\n%u %u\n255\n", width, height);
        for (uint32_t y = 0; y < height; ++y) {
            uint32_t srcY = height - 1 - y;
            for (uint32_t x = 0; x < width; ++x) {
                uint32_t idx = (srcY * width + x) * 4;
                fputc(fb[idx + 0], ppm);
                fputc(fb[idx + 1], ppm);
                fputc(fb[idx + 2], ppm);
            }
        }
        fclose(ppm);
    }

    anariUnmapFrame(f.device, frame, "channel.color");

    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, lightArr);
    anariRelease(f.device, light);
    anariRelease(f.device, surfaceArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, idxArray);
    anariRelease(f.device, colArray);
    anariRelease(f.device, posArray);
    anariRelease(f.device, mesh);
    anariRelease(f.device, camera);
}

void RenderTest::groupAndInstance()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    const float vertex[] = {
        -0.5f, -0.5f, -3.0f,
         0.5f, -0.5f, -3.0f,
         0.0f,  0.5f, -3.0f};
    const uint32_t index[] = {0, 1, 2};
    const float color[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D colArr = anariNewArray1D(
        f.device, color, nullptr, nullptr, ANARI_FLOAT32_VEC4, 3);
    ANARIArray1D idxArr = anariNewArray1D(
        f.device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 1);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(
        f.device, geom, "primitive.index", ANARI_ARRAY1D, &idxArr);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    ANARISurface surface = anariNewSurface(f.device);
    anariSetParameter(
        f.device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(
        f.device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surface);

    ANARIGroup group = anariNewGroup(f.device);
    CORRADE_VERIFY(group);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, group, "surface", ANARI_ARRAY1D, &surfArr);
    anariCommitParameters(f.device, group);

    ANARIInstance inst = anariNewInstance(f.device, "transform");
    CORRADE_VERIFY(inst);
    anariSetParameter(f.device, inst, "group", ANARI_GROUP, &group);
    const float transform[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
    anariSetParameter(
        f.device, inst, "transform", ANARI_FLOAT32_MAT4, transform);
    anariCommitParameters(f.device, inst);

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D instArr = anariNewArray1D(
        f.device, &inst, nullptr, nullptr, ANARI_INSTANCE, 1);
    anariSetParameter(
        f.device, world, "instance", ANARI_ARRAY1D, &instArr);
    ANARILight light = anariNewLight(f.device, "directional");
    anariCommitParameters(f.device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0.0f, 0.0f, 0.0f};
    const float cam_dir[] = {0.0f, 0.0f, -1.0f};
    const float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(
        f.device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(
        f.device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {64, 64};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        f.device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(f.device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(f.device, frame);

    anariRenderFrame(f.device, frame);
    anariFrameReady(f.device, frame, ANARI_WAIT);

    uint32_t width = 0, height = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        f.device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_COMPARE(width, 64u);
    CORRADE_COMPARE(height, 64u);
    CORRADE_VERIFY(fb != nullptr);

    bool hasNonBlack = false;
    for (uint32_t i = 0; i < width * height * 4; i += 4) {
        if (fb[i] > 0 || fb[i + 1] > 0 || fb[i + 2] > 0) {
            hasNonBlack = true;
            break;
        }
    }
    CORRADE_VERIFY(hasNonBlack);

    anariUnmapFrame(f.device, frame, "channel.color");
    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, camera);
    anariRelease(f.device, lightArr);
    anariRelease(f.device, light);
    anariRelease(f.device, instArr);
    anariRelease(f.device, world);
    anariRelease(f.device, inst);
    anariRelease(f.device, surfArr);
    anariRelease(f.device, group);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, idxArr);
    anariRelease(f.device, colArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void RenderTest::rendererBackgroundColor()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIWorld world = anariNewWorld(f.device);
    anariCommitParameters(f.device, world);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    const float bg[] = {0.2f, 0.4f, 0.8f, 1.0f};
    anariSetParameter(
        f.device, renderer, "background", ANARI_FLOAT32_VEC4, bg);
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {32, 32};
    anariSetParameter(f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(f.device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(f.device, frame);

    anariRenderFrame(f.device, frame);
    anariFrameReady(f.device, frame, ANARI_WAIT);

    uint32_t width = 0, height = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        f.device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb != nullptr);

    // The background should be visible (non-black) in an empty scene
    bool hasNonBlack = false;
    for (uint32_t i = 0; i < width * height * 4; i += 4) {
        if (fb[i] > 0 || fb[i + 1] > 0 || fb[i + 2] > 0) {
            hasNonBlack = true;
            break;
        }
    }
    CORRADE_VERIFY(hasNonBlack);

    anariUnmapFrame(f.device, frame, "channel.color");
    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, world);
    anariRelease(f.device, camera);
}

}

CORRADE_TEST_MAIN(RenderTest)
