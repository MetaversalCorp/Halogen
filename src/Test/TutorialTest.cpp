// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#define _CRT_SECURE_NO_WARNINGS

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#include <cstdio>
#include <cstring>

namespace {

struct TutorialTest : Corrade::TestSuite::Tester {
    explicit TutorialTest();

    void renderTriangle();
};

TutorialTest::TutorialTest() {
    addTests({&TutorialTest::renderTriangle});
}

void TutorialTest::renderTriangle() {
    auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
        ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
        const char *message) {
        if (severity <= ANARI_SEVERITY_WARNING)
            Corrade::Utility::Debug{} << "ANARI:" << message;
    };

    ANARILibrary library = anariLoadLibrary("filament", statusFunc);
    CORRADE_VERIFY(library);

    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);

    anariCommitParameters(device, device);

    // Camera
    ANARICamera camera = anariNewCamera(device, "perspective");
    CORRADE_VERIFY(camera);
    float aspect = 1024.0f / 768.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    float cam_pos[] = {0.0f, 0.0f, 0.0f};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    float cam_dir[] = {0.0f, 0.0f, -1.0f};
    anariSetParameter(
        device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(device, camera);

    // Geometry
    float vertex[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
        -1.0f,  1.0f, -3.0f,
         1.0f,  1.0f, -3.0f};
    float color[] = {
        0.9f, 0.5f, 0.5f, 1.0f,
        0.8f, 0.8f, 0.8f, 1.0f,
        0.8f, 0.8f, 0.8f, 1.0f,
        0.5f, 0.9f, 0.5f, 1.0f};
    uint32_t index[] = {0, 1, 2, 1, 3, 2};

    ANARIArray1D posArray = anariNewArray1D(
        device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D colArray = anariNewArray1D(
        device, color, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    ANARIArray1D idxArray = anariNewArray1D(
        device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 2);

    ANARIGeometry mesh = anariNewGeometry(device, "triangle");
    anariSetParameter(
        device, mesh, "vertex.position", ANARI_ARRAY1D, &posArray);
    anariSetParameter(
        device, mesh, "vertex.color", ANARI_ARRAY1D, &colArray);
    anariSetParameter(
        device, mesh, "primitive.index", ANARI_ARRAY1D, &idxArray);
    anariCommitParameters(device, mesh);

    // Material
    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariSetParameter(device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(device, mat);

    // Surface
    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(
        device, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(
        device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    // World
    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfaceArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        device, world, "surface", ANARI_ARRAY1D, &surfaceArr);

    ANARILight light = anariNewLight(device, "directional");
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    // Renderer
    ANARIRenderer renderer = anariNewRenderer(device, "default");
    anariCommitParameters(device, renderer);

    // Frame
    ANARIFrame frame = anariNewFrame(device);
    uint32_t imgSize[] = {1024, 768};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(
        device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(
        device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(
        device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(
        device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    // Render
    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    // Read pixels
    uint32_t width = 0, height = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));

    CORRADE_COMPARE(width, 1024u);
    CORRADE_COMPARE(height, 768u);
    CORRADE_VERIFY(fb != nullptr);

    // Verify the image is not all black
    bool hasNonBlack = false;
    for (uint32_t i = 0; i < width * height * 4; i += 4) {
        if (fb[i] > 0 || fb[i + 1] > 0 || fb[i + 2] > 0) {
            hasNonBlack = true;
            break;
        }
    }
    CORRADE_VERIFY(hasNonBlack);

    // Write PPM for visual verification
    FILE *ppm = fopen("tutorial_output.ppm", "wb");
    if (ppm) {
        fprintf(ppm, "P6\n%u %u\n255\n", width, height);
        for (uint32_t y = 0; y < height; ++y) {
            // Filament readPixels returns bottom-up, flip vertically
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

    anariUnmapFrame(device, frame, "channel.color");

    // Cleanup
    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfaceArr);
    anariRelease(device, world);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, idxArray);
    anariRelease(device, colArray);
    anariRelease(device, posArray);
    anariRelease(device, mesh);
    anariRelease(device, camera);
    anariRelease(device, device);
    anariUnloadLibrary(library);
}

}

CORRADE_TEST_MAIN(TutorialTest)
