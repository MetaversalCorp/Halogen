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

struct MaterialTest : Corrade::TestSuite::Tester {
    explicit MaterialTest();

    void matteVertexColor();
    void pbrMaterial();
};

MaterialTest::MaterialTest()
{
    addTests({&MaterialTest::matteVertexColor,
        &MaterialTest::pbrMaterial});
}

void MaterialTest::matteVertexColor()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    CORRADE_VERIFY(mat);

    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    float solidColor[] = {1.0f, 0.0f, 0.0f};
    anariSetParameter(
        f.device, mat, "color", ANARI_FLOAT32_VEC3, solidColor);
    anariCommitParameters(f.device, mat);

    anariRelease(f.device, mat);
}

void MaterialTest::pbrMaterial()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

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

}

CORRADE_TEST_MAIN(MaterialTest)
