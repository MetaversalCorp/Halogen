// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#define _CRT_SECURE_NO_WARNINGS

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#include <stb_image.h>
#include <stb_image_write.h>

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef SCREENSHOT_GOLDEN_DIR
#define SCREENSHOT_GOLDEN_DIR "."
#endif

namespace {

void writePng(const char *path, const uint8_t *rgba, uint32_t w, uint32_t h)
{
    /* Drop alpha channel (RGBA -> RGB) */
    std::vector<uint8_t> rgb(size_t(w) * h * 3);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t si = (y * w + x) * 4;
            const uint32_t di = (y * w + x) * 3;
            rgb[di + 0] = rgba[si + 0];
            rgb[di + 1] = rgba[si + 1];
            rgb[di + 2] = rgba[si + 2];
        }
    }
    stbi_write_png(path, int(w), int(h), 3, rgb.data(), int(w) * 3);
}

auto makeStatusCallback()
{
    return [](const void *, ANARIDevice, ANARIObject,
        ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
        const char *message) {
        if (severity <= ANARI_SEVERITY_WARNING)
            Corrade::Utility::Debug{} << "ANARI:" << message;
    };
}

constexpr uint32_t IMG_SIZE = 128;
constexpr int MAX_CHANNEL_DIFF = 95;

struct ScreenshotTest : Corrade::TestSuite::Tester {
    explicit ScreenshotTest();

    void coloredQuad();
    void instancedTriangle();
    void sphereScene();
    void curveScene();
    void pbrSpheres();
    void float32Frame();
    void introspection();

private:
    void verifyAgainstGolden(
        const std::vector<uint8_t> &rgba, uint32_t w, uint32_t h,
        const char *name);
};

void ScreenshotTest::verifyAgainstGolden(
    const std::vector<uint8_t> &rgba, uint32_t w, uint32_t h,
    const char *name)
{
    std::string outputPath = std::string(name) + ".png";
    writePng(outputPath.c_str(), rgba.data(), w, h);

    std::string goldenPath =
        std::string(SCREENSHOT_GOLDEN_DIR) + "/" + name + ".png";
    int gw = 0, gh = 0, gn = 0;
    unsigned char *golden = stbi_load(goldenPath.c_str(), &gw, &gh, &gn, 3);
    CORRADE_VERIFY(golden);

    CORRADE_COMPARE(w, uint32_t(gw));
    CORRADE_COMPARE(h, uint32_t(gh));

    uint32_t mismatchCount = 0;
    int maxDiff = 0;
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const uint32_t fbIdx = (y * w + x) * 4;
            const uint32_t goldenIdx = (y * w + x) * 3;
            for (int c = 0; c < 3; ++c) {
                const int diff =
                    std::abs(int(rgba[fbIdx + c])
                        - int(golden[goldenIdx + c]));
                if (diff > maxDiff)
                    maxDiff = diff;
                if (diff > MAX_CHANNEL_DIFF)
                    ++mismatchCount;
            }
        }
    }

    stbi_image_free(golden);

    if (mismatchCount > 0) {
        Corrade::Utility::Debug{} << "Mismatched channels:" << mismatchCount
            << "max diff:" << maxDiff
            << "- see" << outputPath.c_str();
    }
    CORRADE_COMPARE(mismatchCount, 0u);
}

ScreenshotTest::ScreenshotTest()
{
    addTests({&ScreenshotTest::coloredQuad,
        &ScreenshotTest::instancedTriangle,
        &ScreenshotTest::sphereScene,
        &ScreenshotTest::curveScene,
        &ScreenshotTest::pbrSpheres,
        &ScreenshotTest::float32Frame,
        &ScreenshotTest::introspection});
}

void ScreenshotTest::coloredQuad()
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;

    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float pos[] = {0, 0, 0};
    const float dir[] = {0, 0, -1};
    const float up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, up);
    anariCommitParameters(device, camera);

    const float vertex[] = {
        -1, -1, -3,
         1, -1, -3,
        -1,  1, -3,
         1,  1, -3};
    const float color[] = {
        0.8f, 0.1f, 0.1f, 1,
        0.1f, 0.8f, 0.1f, 1,
        0.1f, 0.1f, 0.8f, 1,
        0.8f, 0.8f, 0.1f, 1};
    const uint32_t index[] = {0, 1, 2, 1, 3, 2};

    ANARIArray1D posArr = anariNewArray1D(
        device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D colArr = anariNewArray1D(
        device, color, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    ANARIArray1D idxArr = anariNewArray1D(
        device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 2);

    ANARIGeometry mesh = anariNewGeometry(device, "triangle");
    anariSetParameter(device, mesh, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(device, mesh, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(device, mesh, "primitive.index", ANARI_ARRAY1D, &idxArr);
    anariCommitParameters(device, mesh);

    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariSetParameter(device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(device, mat);

    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(device, "directional");
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {IMG_SIZE, IMG_SIZE};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb);
    pixels.assign(fb, fb + width * height * 4);
    anariUnmapFrame(device, frame, "channel.color");

    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfArr);
    anariRelease(device, world);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, idxArr);
    anariRelease(device, colArr);
    anariRelease(device, posArr);
    anariRelease(device, mesh);
    anariRelease(device, camera);
    anariRelease(device, device);
    anariUnloadLibrary(library);

    verifyAgainstGolden(pixels, width, height, "coloredQuad");
}

void ScreenshotTest::instancedTriangle()
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;

    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    const float vertex[] = {
        -0.5f, -0.5f, -3,
         0.5f, -0.5f, -3,
         0.0f,  0.5f, -3};
    const uint32_t index[] = {0, 1, 2};
    const float color[] = {
        1, 0, 0, 1,
        0, 1, 0, 1,
        0, 0, 1, 1};

    ANARIArray1D posArr = anariNewArray1D(
        device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D colArr = anariNewArray1D(
        device, color, nullptr, nullptr, ANARI_FLOAT32_VEC4, 3);
    ANARIArray1D idxArr = anariNewArray1D(
        device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 1);

    ANARIGeometry geom = anariNewGeometry(device, "triangle");
    anariSetParameter(device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(device, geom, "primitive.index", ANARI_ARRAY1D, &idxArr);
    anariCommitParameters(device, geom);

    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariSetParameter(device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(device, mat);

    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    ANARIGroup group = anariNewGroup(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(device, group, "surface", ANARI_ARRAY1D, &surfArr);
    anariCommitParameters(device, group);

    ANARIInstance inst = anariNewInstance(device, "transform");
    anariSetParameter(device, inst, "group", ANARI_GROUP, &group);
    const float transform[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
    anariSetParameter(
        device, inst, "transform", ANARI_FLOAT32_MAT4, transform);
    anariCommitParameters(device, inst);

    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D instArr = anariNewArray1D(
        device, &inst, nullptr, nullptr, ANARI_INSTANCE, 1);
    anariSetParameter(device, world, "instance", ANARI_ARRAY1D, &instArr);
    ANARILight light = anariNewLight(device, "directional");
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0, 0, 0};
    const float cam_dir[] = {0, 0, -1};
    const float cam_up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(device, camera);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {IMG_SIZE, IMG_SIZE};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb);
    pixels.assign(fb, fb + width * height * 4);
    anariUnmapFrame(device, frame, "channel.color");

    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, camera);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, instArr);
    anariRelease(device, world);
    anariRelease(device, inst);
    anariRelease(device, surfArr);
    anariRelease(device, group);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, idxArr);
    anariRelease(device, colArr);
    anariRelease(device, posArr);
    anariRelease(device, geom);
    anariRelease(device, device);
    anariUnloadLibrary(library);

    verifyAgainstGolden(pixels, width, height, "instancedTriangle");
}

void ScreenshotTest::sphereScene()
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;

    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    const float positions[] = {
        -1.5f, 0, -5,
         0.0f, 0, -5,
         1.5f, 0, -5};
    const float radii[] = {0.5f, 0.7f, 0.4f};
    const float colors[] = {
        0.9f, 0.2f, 0.2f, 1,
        0.2f, 0.9f, 0.2f, 1,
        0.2f, 0.2f, 0.9f, 1};

    ANARIArray1D posArr = anariNewArray1D(
        device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D radArr = anariNewArray1D(
        device, radii, nullptr, nullptr, ANARI_FLOAT32, 3);
    ANARIArray1D colArr = anariNewArray1D(
        device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 3);

    ANARIGeometry geom = anariNewGeometry(device, "sphere");
    anariSetParameter(device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
    anariSetParameter(device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariCommitParameters(device, geom);

    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariSetParameter(device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(device, mat);

    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(device, "directional");
    const float lightDir[] = {-0.5f, -1, -0.5f};
    anariSetParameter(device, light, "direction", ANARI_FLOAT32_VEC3, lightDir);
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0, 0, 0};
    const float cam_dir[] = {0, 0, -1};
    const float cam_up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(device, camera);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    const float sphAmbientColor[] = {1, 1, 1};
    const float sphAmbientRadiance = 0.2f;
    anariSetParameter(device, renderer,
        "ambientColor", ANARI_FLOAT32_VEC3, sphAmbientColor);
    anariSetParameter(device, renderer,
        "ambientRadiance", ANARI_FLOAT32, &sphAmbientRadiance);
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {IMG_SIZE, IMG_SIZE};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb);
    pixels.assign(fb, fb + width * height * 4);
    anariUnmapFrame(device, frame, "channel.color");

    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, camera);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfArr);
    anariRelease(device, world);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, colArr);
    anariRelease(device, radArr);
    anariRelease(device, posArr);
    anariRelease(device, geom);
    anariRelease(device, device);
    anariUnloadLibrary(library);

    verifyAgainstGolden(pixels, width, height, "sphereScene");
}

void ScreenshotTest::curveScene()
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;

    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    const float positions[] = {
        -2.0f, -0.5f, -5,
        -0.5f,  0.5f, -5,
         0.5f, -0.5f, -5,
         2.0f,  0.5f, -5};
    const float radii[] = {0.15f, 0.25f, 0.25f, 0.15f};
    const float colors[] = {
        1, 0.2f, 0.2f, 1,
        1, 1, 0.2f, 1,
        0.2f, 1, 0.2f, 1,
        0.2f, 0.2f, 1, 1};

    ANARIArray1D posArr = anariNewArray1D(
        device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D radArr = anariNewArray1D(
        device, radii, nullptr, nullptr, ANARI_FLOAT32, 4);
    ANARIArray1D colArr = anariNewArray1D(
        device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);

    ANARIGeometry geom = anariNewGeometry(device, "curve");
    anariSetParameter(device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
    anariSetParameter(device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariCommitParameters(device, geom);

    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariSetParameter(device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(device, mat);

    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(device, "directional");
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0, 0, 0};
    const float cam_dir[] = {0, 0, -1};
    const float cam_up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(device, camera);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    const float crvAmbientColor[] = {1, 1, 1};
    const float crvAmbientRadiance = 0.2f;
    anariSetParameter(device, renderer,
        "ambientColor", ANARI_FLOAT32_VEC3, crvAmbientColor);
    anariSetParameter(device, renderer,
        "ambientRadiance", ANARI_FLOAT32, &crvAmbientRadiance);
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {IMG_SIZE, IMG_SIZE};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb);
    pixels.assign(fb, fb + width * height * 4);
    anariUnmapFrame(device, frame, "channel.color");

    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, camera);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfArr);
    anariRelease(device, world);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, colArr);
    anariRelease(device, radArr);
    anariRelease(device, posArr);
    anariRelease(device, geom);
    anariRelease(device, device);
    anariUnloadLibrary(library);

    verifyAgainstGolden(pixels, width, height, "curveScene");
}

void ScreenshotTest::pbrSpheres()
{
    std::vector<uint8_t> pixels;
    uint32_t width = 0, height = 0;

    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    // Three spheres with different PBR properties
    const float positions[] = {
        -1.5f, 0, -4,
         0.0f, 0, -4,
         1.5f, 0, -4};
    const float radii[] = {0.6f, 0.6f, 0.6f};

    ANARIArray1D posArr = anariNewArray1D(
        device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D radArr = anariNewArray1D(
        device, radii, nullptr, nullptr, ANARI_FLOAT32, 3);

    // Rough red sphere
    ANARIGeometry geom0 = anariNewGeometry(device, "sphere");
    {
        const uint32_t idx = 0;
        ANARIArray1D pArr = anariNewArray1D(
            device, &positions[idx * 3], nullptr, nullptr,
            ANARI_FLOAT32_VEC3, 1);
        ANARIArray1D rArr = anariNewArray1D(
            device, &radii[idx], nullptr, nullptr, ANARI_FLOAT32, 1);
        anariSetParameter(device, geom0,
            "vertex.position", ANARI_ARRAY1D, &pArr);
        anariSetParameter(device, geom0,
            "vertex.radius", ANARI_ARRAY1D, &rArr);
        anariCommitParameters(device, geom0);
        anariRelease(device, pArr);
        anariRelease(device, rArr);
    }
    ANARIMaterial mat0 = anariNewMaterial(device, "physicallyBased");
    {
        const float color0[] = {0.8f, 0.1f, 0.1f};
        const float roughness = 0.8f;
        const float metallic = 0.0f;
        anariSetParameter(device, mat0,
            "baseColor", ANARI_FLOAT32_VEC3, color0);
        anariSetParameter(device, mat0,
            "roughness", ANARI_FLOAT32, &roughness);
        anariSetParameter(device, mat0,
            "metallic", ANARI_FLOAT32, &metallic);
        anariCommitParameters(device, mat0);
    }
    ANARISurface surf0 = anariNewSurface(device);
    anariSetParameter(device, surf0, "geometry", ANARI_GEOMETRY, &geom0);
    anariSetParameter(device, surf0, "material", ANARI_MATERIAL, &mat0);
    anariCommitParameters(device, surf0);

    // Smooth metallic gold sphere
    ANARIGeometry geom1 = anariNewGeometry(device, "sphere");
    {
        const uint32_t idx = 1;
        ANARIArray1D pArr = anariNewArray1D(
            device, &positions[idx * 3], nullptr, nullptr,
            ANARI_FLOAT32_VEC3, 1);
        ANARIArray1D rArr = anariNewArray1D(
            device, &radii[idx], nullptr, nullptr, ANARI_FLOAT32, 1);
        anariSetParameter(device, geom1,
            "vertex.position", ANARI_ARRAY1D, &pArr);
        anariSetParameter(device, geom1,
            "vertex.radius", ANARI_ARRAY1D, &rArr);
        anariCommitParameters(device, geom1);
        anariRelease(device, pArr);
        anariRelease(device, rArr);
    }
    ANARIMaterial mat1 = anariNewMaterial(device, "physicallyBased");
    {
        const float color1[] = {0.9f, 0.7f, 0.2f};
        const float roughness = 0.1f;
        const float metallic = 1.0f;
        anariSetParameter(device, mat1,
            "baseColor", ANARI_FLOAT32_VEC3, color1);
        anariSetParameter(device, mat1,
            "roughness", ANARI_FLOAT32, &roughness);
        anariSetParameter(device, mat1,
            "metallic", ANARI_FLOAT32, &metallic);
        anariCommitParameters(device, mat1);
    }
    ANARISurface surf1 = anariNewSurface(device);
    anariSetParameter(device, surf1, "geometry", ANARI_GEOMETRY, &geom1);
    anariSetParameter(device, surf1, "material", ANARI_MATERIAL, &mat1);
    anariCommitParameters(device, surf1);

    // Medium-rough blue sphere
    ANARIGeometry geom2 = anariNewGeometry(device, "sphere");
    {
        const uint32_t idx = 2;
        ANARIArray1D pArr = anariNewArray1D(
            device, &positions[idx * 3], nullptr, nullptr,
            ANARI_FLOAT32_VEC3, 1);
        ANARIArray1D rArr = anariNewArray1D(
            device, &radii[idx], nullptr, nullptr, ANARI_FLOAT32, 1);
        anariSetParameter(device, geom2,
            "vertex.position", ANARI_ARRAY1D, &pArr);
        anariSetParameter(device, geom2,
            "vertex.radius", ANARI_ARRAY1D, &rArr);
        anariCommitParameters(device, geom2);
        anariRelease(device, pArr);
        anariRelease(device, rArr);
    }
    ANARIMaterial mat2 = anariNewMaterial(device, "physicallyBased");
    {
        const float color2[] = {0.1f, 0.2f, 0.8f};
        const float roughness = 0.4f;
        const float metallic = 0.3f;
        anariSetParameter(device, mat2,
            "baseColor", ANARI_FLOAT32_VEC3, color2);
        anariSetParameter(device, mat2,
            "roughness", ANARI_FLOAT32, &roughness);
        anariSetParameter(device, mat2,
            "metallic", ANARI_FLOAT32, &metallic);
        anariCommitParameters(device, mat2);
    }
    ANARISurface surf2 = anariNewSurface(device);
    anariSetParameter(device, surf2, "geometry", ANARI_GEOMETRY, &geom2);
    anariSetParameter(device, surf2, "material", ANARI_MATERIAL, &mat2);
    anariCommitParameters(device, surf2);

    ANARISurface surfaces[] = {surf0, surf1, surf2};
    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, surfaces, nullptr, nullptr, ANARI_SURFACE, 3);
    anariSetParameter(device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(device, "directional");
    const float lightDir[] = {-0.3f, -1, -0.3f};
    anariSetParameter(device, light, "direction", ANARI_FLOAT32_VEC3, lightDir);
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float cam_pos[] = {0, 0, 0};
    const float cam_dir[] = {0, 0, -1};
    const float cam_up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(device, camera);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    const float pbrAmbientColor[] = {1, 1, 1};
    const float pbrAmbientRadiance = 0.3f;
    anariSetParameter(device, renderer,
        "ambientColor", ANARI_FLOAT32_VEC3, pbrAmbientColor);
    anariSetParameter(device, renderer,
        "ambientRadiance", ANARI_FLOAT32, &pbrAmbientRadiance);
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {IMG_SIZE, IMG_SIZE};
    const ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = static_cast<const uint8_t *>(anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType));
    CORRADE_VERIFY(fb);
    pixels.assign(fb, fb + width * height * 4);
    anariUnmapFrame(device, frame, "channel.color");

    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, camera);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfArr);
    anariRelease(device, world);
    anariRelease(device, surf2);
    anariRelease(device, mat2);
    anariRelease(device, geom2);
    anariRelease(device, surf1);
    anariRelease(device, mat1);
    anariRelease(device, geom1);
    anariRelease(device, surf0);
    anariRelease(device, mat0);
    anariRelease(device, geom0);
    anariRelease(device, radArr);
    anariRelease(device, posArr);
    anariRelease(device, device);
    anariUnloadLibrary(library);

    verifyAgainstGolden(pixels, width, height, "pbrSpheres");
}

void ScreenshotTest::float32Frame()
{
    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    ANARICamera camera = anariNewCamera(device, "perspective");
    const float aspect = 1.0f;
    anariSetParameter(device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float pos[] = {0, 0, 0};
    const float dir[] = {0, 0, -1};
    const float up[] = {0, 1, 0};
    anariSetParameter(device, camera, "position", ANARI_FLOAT32_VEC3, pos);
    anariSetParameter(device, camera, "direction", ANARI_FLOAT32_VEC3, dir);
    anariSetParameter(device, camera, "up", ANARI_FLOAT32_VEC3, up);
    anariCommitParameters(device, camera);

    const float vertex[] = {
        -1, -1, -3,
         1, -1, -3,
         0,  1, -3};
    const uint32_t index[] = {0, 1, 2};

    ANARIArray1D posArr = anariNewArray1D(
        device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D idxArr = anariNewArray1D(
        device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 1);

    ANARIGeometry mesh = anariNewGeometry(device, "triangle");
    anariSetParameter(device, mesh, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(device, mesh, "primitive.index", ANARI_ARRAY1D, &idxArr);
    anariCommitParameters(device, mesh);

    ANARIMaterial mat = anariNewMaterial(device, "matte");
    anariCommitParameters(device, mat);

    ANARISurface surface = anariNewSurface(device);
    anariSetParameter(device, surface, "geometry", ANARI_GEOMETRY, &mesh);
    anariSetParameter(device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(device, surface);

    ANARIWorld world = anariNewWorld(device);
    ANARIArray1D surfArr = anariNewArray1D(
        device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(device, "directional");
    anariCommitParameters(device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(device, world);

    ANARIRenderer renderer = anariNewRenderer(device, "default");
    anariCommitParameters(device, renderer);

    ANARIFrame frame = anariNewFrame(device);
    const uint32_t imgSize[] = {64, 64};
    const ANARIDataType colorType = ANARI_FLOAT32_VEC4;
    anariSetParameter(device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(
        device, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(device, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(device, frame);

    anariRenderFrame(device, frame);
    anariFrameReady(device, frame, ANARI_WAIT);

    uint32_t width = 0, height = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *fb = anariMapFrame(
        device, frame, "channel.color", &width, &height, &fbType);
    CORRADE_COMPARE(width, 64u);
    CORRADE_COMPARE(height, 64u);
    CORRADE_COMPARE(fbType, ANARI_FLOAT32_VEC4);
    CORRADE_VERIFY(fb != nullptr);

    const auto *pixels = static_cast<const float *>(fb);
    bool hasNonZero = false;
    for (uint32_t i = 0; i < width * height * 4; ++i) {
        if (pixels[i] != 0.0f) {
            hasNonZero = true;
            break;
        }
    }
    CORRADE_VERIFY(hasNonZero);

    anariUnmapFrame(device, frame, "channel.color");
    anariRelease(device, frame);
    anariRelease(device, renderer);
    anariRelease(device, lightArr);
    anariRelease(device, light);
    anariRelease(device, surfArr);
    anariRelease(device, world);
    anariRelease(device, surface);
    anariRelease(device, mat);
    anariRelease(device, idxArr);
    anariRelease(device, posArr);
    anariRelease(device, mesh);
    anariRelease(device, camera);
    anariRelease(device, device);
    anariUnloadLibrary(library);
}

void ScreenshotTest::introspection()
{
    ANARILibrary library = anariLoadLibrary("halogen", makeStatusCallback());
    CORRADE_VERIFY(library);
    ANARIDevice device = anariNewDevice(library, "default");
    CORRADE_VERIFY(device);
    anariCommitParameters(device, device);

    const char **geomSubs =
        anariGetObjectSubtypes(device, ANARI_GEOMETRY);
    CORRADE_VERIFY(geomSubs);
    CORRADE_VERIFY(geomSubs[0] != nullptr);
    bool foundTriangle = false;
    bool foundCurve = false;
    for (int i = 0; geomSubs[i]; ++i) {
        if (std::strcmp(geomSubs[i], "triangle") == 0)
            foundTriangle = true;
        if (std::strcmp(geomSubs[i], "curve") == 0)
            foundCurve = true;
    }
    CORRADE_VERIFY(foundTriangle);
    CORRADE_VERIFY(foundCurve);

    const char **camSubs =
        anariGetObjectSubtypes(device, ANARI_CAMERA);
    CORRADE_VERIFY(camSubs);
    bool foundPerspective = false;
    for (int i = 0; camSubs[i]; ++i) {
        if (std::strcmp(camSubs[i], "perspective") == 0)
            foundPerspective = true;
    }
    CORRADE_VERIFY(foundPerspective);

    const char **lightSubs =
        anariGetObjectSubtypes(device, ANARI_LIGHT);
    CORRADE_VERIFY(lightSubs);
    bool foundDirectional = false;
    for (int i = 0; lightSubs[i]; ++i) {
        if (std::strcmp(lightSubs[i], "directional") == 0)
            foundDirectional = true;
    }
    CORRADE_VERIFY(foundDirectional);

    const char **matSubs =
        anariGetObjectSubtypes(device, ANARI_MATERIAL);
    CORRADE_VERIFY(matSubs);
    bool foundMatte = false;
    for (int i = 0; matSubs[i]; ++i) {
        if (std::strcmp(matSubs[i], "matte") == 0)
            foundMatte = true;
    }
    CORRADE_VERIFY(foundMatte);

    const char **unknownSubs =
        anariGetObjectSubtypes(device, ANARI_FLOAT32);
    CORRADE_VERIFY(!unknownSubs);

    anariRelease(device, device);
    anariUnloadLibrary(library);
}

}

CORRADE_TEST_MAIN(ScreenshotTest)
