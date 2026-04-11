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

struct Milestone2Test : Corrade::TestSuite::Tester {
    explicit Milestone2Test();

    void sphereGeometry();
    void texturedTriangle();
    void pbrMaterial();
    void groupAndInstance();
};

Milestone2Test::Milestone2Test() {
    addTests({&Milestone2Test::sphereGeometry,
        &Milestone2Test::texturedTriangle,
        &Milestone2Test::pbrMaterial,
        &Milestone2Test::groupAndInstance});
}

void Milestone2Test::sphereGeometry() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Create sphere geometry with per-sphere positions, colors, and radii
    float positions[] = {0.0f, 0.0f, -3.0f, 1.0f, 0.0f, -3.0f};
    float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f};
    float radii[] = {0.3f, 0.5f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);
    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 2);
    ANARIArray1D radArr = anariNewArray1D(
        f.device, radii, nullptr, nullptr, ANARI_FLOAT32, 2);

    ANARIGeometry geom = anariNewGeometry(f.device, "sphere");
    CORRADE_VERIFY(geom);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(
        f.device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
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

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, world, "surface", ANARI_ARRAY1D, &surfArr);
    ANARILight light = anariNewLight(f.device, "directional");
    anariCommitParameters(f.device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    float pos[] = {0.5f, 0.0f, 0.0f};
    float dir[] = {0.0f, 0.0f, -1.0f};
    float up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(f.device, camera, "position", ANARI_FLOAT32_VEC3, pos);
    anariSetParameter(f.device, camera, "direction", ANARI_FLOAT32_VEC3, dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, up);
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {64, 64};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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

    // Verify the image is not all black (spheres rendered)
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
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, radArr);
    anariRelease(f.device, colArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void Milestone2Test::texturedTriangle() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Quad with texture coordinates
    float vertex[] = {
        -0.5f,  0.5f, -3.0f,
         0.5f,  0.5f, -3.0f,
        -0.5f, -0.5f, -3.0f,
         0.5f, -0.5f, -3.0f};
    float texcoords[] = {0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f};
    uint32_t index[] = {0, 2, 3, 3, 1, 0};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertex, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D uvArr = anariNewArray1D(
        f.device, texcoords, nullptr, nullptr, ANARI_FLOAT32_VEC2, 4);
    ANARIArray1D idxArr = anariNewArray1D(
        f.device, index, nullptr, nullptr, ANARI_UINT32_VEC3, 2);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &uvArr);
    anariSetParameter(
        f.device, geom, "primitive.index", ANARI_ARRAY1D, &idxArr);
    anariCommitParameters(f.device, geom);

    // Create 4x4 checkerboard texture (float3)
    float texData[4 * 4 * 3];
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
            float v = ((x + y) % 2 == 0) ? 0.8f : 0.2f;
            texData[(y * 4 + x) * 3 + 0] = v;
            texData[(y * 4 + x) * 3 + 1] = v;
            texData[(y * 4 + x) * 3 + 2] = v;
        }
    }

    ANARIArray2D texArray = anariNewArray2D(
        f.device, texData, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4, 4);

    ANARISampler sampler = anariNewSampler(f.device, "image2D");
    CORRADE_VERIFY(sampler);
    anariSetParameter(f.device, sampler, "image", ANARI_ARRAY2D, &texArray);
    anariSetParameter(f.device, sampler, "inAttribute", ANARI_STRING,
        "attribute0");
    anariSetParameter(f.device, sampler, "filter", ANARI_STRING, "nearest");
    anariCommitParameters(f.device, sampler);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_SAMPLER, &sampler);
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
    ANARILight light = anariNewLight(f.device, "directional");
    anariCommitParameters(f.device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    float cam_pos[] = {0.0f, 0.0f, 0.0f};
    float cam_dir[] = {0.0f, 0.0f, -1.0f};
    float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(
        f.device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(
        f.device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {64, 64};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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

    // Verify not all black
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
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, sampler);
    anariRelease(f.device, texArray);
    anariRelease(f.device, idxArr);
    anariRelease(f.device, uvArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void Milestone2Test::pbrMaterial() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Sphere with per-sphere metallic/roughness attributes
    float positions[] = {0.0f, 0.0f, -3.0f, 1.0f, 0.0f, -3.0f};
    float metallic[] = {0.0f, 1.0f};
    float roughness[] = {0.2f, 0.8f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);
    ANARIArray1D attr0Arr = anariNewArray1D(
        f.device, metallic, nullptr, nullptr, ANARI_FLOAT32, 2);
    ANARIArray1D attr1Arr = anariNewArray1D(
        f.device, roughness, nullptr, nullptr, ANARI_FLOAT32, 2);

    ANARIGeometry geom = anariNewGeometry(f.device, "sphere");
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &attr0Arr);
    anariSetParameter(
        f.device, geom, "vertex.attribute1", ANARI_ARRAY1D, &attr1Arr);
    float radius = 0.4f;
    anariSetParameter(f.device, geom, "radius", ANARI_FLOAT32, &radius);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "physicallyBased");
    float baseColor[] = {1.0f, 0.0f, 0.0f};
    anariSetParameter(
        f.device, mat, "baseColor", ANARI_FLOAT32_VEC3, baseColor);
    anariSetParameter(f.device, mat, "metallic", ANARI_STRING, "attribute0");
    anariSetParameter(f.device, mat, "roughness", ANARI_STRING, "attribute1");
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
    ANARILight light = anariNewLight(f.device, "directional");
    anariCommitParameters(f.device, light);
    ANARIArray1D lightArr = anariNewArray1D(
        f.device, &light, nullptr, nullptr, ANARI_LIGHT, 1);
    anariSetParameter(
        f.device, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    float cam_pos[] = {0.5f, 0.0f, 0.0f};
    float cam_dir[] = {0.0f, 0.0f, -1.0f};
    float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(
        f.device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(
        f.device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {64, 64};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, attr1Arr);
    anariRelease(f.device, attr0Arr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void Milestone2Test::groupAndInstance() {
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    // Create a triangle and wrap in group + instance with transform
    float vertex[] = {
        -0.5f, -0.5f, -3.0f,
         0.5f, -0.5f, -3.0f,
         0.0f,  0.5f, -3.0f};
    uint32_t index[] = {0, 1, 2};
    float color[] = {
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

    // Group
    ANARIGroup group = anariNewGroup(f.device);
    CORRADE_VERIFY(group);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, group, "surface", ANARI_ARRAY1D, &surfArr);
    anariCommitParameters(f.device, group);

    // Instance with identity transform
    ANARIInstance inst = anariNewInstance(f.device, "transform");
    CORRADE_VERIFY(inst);
    anariSetParameter(f.device, inst, "group", ANARI_GROUP, &group);
    float transform[] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1};
    anariSetParameter(
        f.device, inst, "transform", ANARI_FLOAT32_MAT4, transform);
    anariCommitParameters(f.device, inst);

    // World with instances
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
    float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    float cam_pos[] = {0.0f, 0.0f, 0.0f};
    float cam_dir[] = {0.0f, 0.0f, -1.0f};
    float cam_up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(
        f.device, camera, "position", ANARI_FLOAT32_VEC3, cam_pos);
    anariSetParameter(
        f.device, camera, "direction", ANARI_FLOAT32_VEC3, cam_dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, cam_up);
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    uint32_t imgSize[] = {64, 64};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
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

}

CORRADE_TEST_MAIN(Milestone2Test)
