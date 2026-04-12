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

struct GeometryTest : Corrade::TestSuite::Tester {
    explicit GeometryTest();

    void triangleGeometry();
    void sphereGeometry();
    void cylinderGeometry();
    void primitiveColor();
};

GeometryTest::GeometryTest()
{
    addTests({&GeometryTest::triangleGeometry,
        &GeometryTest::sphereGeometry,
        &GeometryTest::cylinderGeometry,
        &GeometryTest::primitiveColor});
}

void GeometryTest::triangleGeometry()
{
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

void GeometryTest::sphereGeometry()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

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

void GeometryTest::cylinderGeometry()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "cylinder");
    CORRADE_VERIFY(geom);

    float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
    };
    float perCylRadii[] = {0.1f, 0.2f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    anariSetParameter(f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);

    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    anariSetParameter(f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);

    float radius = 0.05f;
    anariSetParameter(f.device, geom, "radius", ANARI_FLOAT32, &radius);

    ANARIArray1D radiiArr = anariNewArray1D(
        f.device, perCylRadii, nullptr, nullptr, ANARI_FLOAT32, 2);
    anariSetParameter(
        f.device, geom, "primitive.radius", ANARI_ARRAY1D, &radiiArr);

    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    anariRelease(f.device, posArr);
    anariRelease(f.device, colArr);
    anariRelease(f.device, radiiArr);
    anariRelease(f.device, surf);
    anariRelease(f.device, mat);
    anariRelease(f.device, geom);
}

void GeometryTest::primitiveColor()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");

    float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -0.5f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.5f,
    };
    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 6);
    anariSetParameter(f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);

    uint8_t primColors[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
    };
    ANARIArray1D primColArr = anariNewArray1D(
        f.device, primColors, nullptr, nullptr, ANARI_UFIXED8_VEC4, 2);
    anariSetParameter(
        f.device, geom, "primitive.color", ANARI_ARRAY1D, &primColArr);

    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_STRING, "color");
    anariCommitParameters(f.device, mat);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    anariRelease(f.device, posArr);
    anariRelease(f.device, primColArr);
    anariRelease(f.device, surf);
    anariRelease(f.device, mat);
    anariRelease(f.device, geom);
}

}

CORRADE_TEST_MAIN(GeometryTest)
