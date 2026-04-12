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
    void quadGeometry();
    void coneGeometry();
    void cylinderWithCaps();
    void coneWithCaps();
    void triangleWithUV();
    void trianglePrimitiveColorWithNormalsAndUV();
    void quadPrimitiveColor();
    void triangleVertexColorConversion();
    void spherePerSphereRadius();
};

GeometryTest::GeometryTest()
{
    addTests({&GeometryTest::triangleGeometry,
        &GeometryTest::sphereGeometry,
        &GeometryTest::cylinderGeometry,
        &GeometryTest::primitiveColor,
        &GeometryTest::quadGeometry,
        &GeometryTest::coneGeometry,
        &GeometryTest::cylinderWithCaps,
        &GeometryTest::coneWithCaps,
        &GeometryTest::triangleWithUV,
        &GeometryTest::trianglePrimitiveColorWithNormalsAndUV,
        &GeometryTest::quadPrimitiveColor,
        &GeometryTest::triangleVertexColorConversion,
        &GeometryTest::spherePerSphereRadius});
}

void GeometryTest::triangleGeometry()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    const float vertices[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
    const uint32_t indices[] = {0, 1, 2};

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

    const float positions[] = {0.0f, 0.0f, -3.0f, 1.0f, 0.0f, -3.0f};
    const float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f};
    const float radii[] = {0.3f, 0.5f};

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
    const float aspect = 1.0f;
    anariSetParameter(f.device, camera, "aspect", ANARI_FLOAT32, &aspect);
    const float pos[] = {0.5f, 0.0f, 0.0f};
    const float dir[] = {0.0f, 0.0f, -1.0f};
    const float up[] = {0.0f, 1.0f, 0.0f};
    anariSetParameter(f.device, camera, "position", ANARI_FLOAT32_VEC3, pos);
    anariSetParameter(f.device, camera, "direction", ANARI_FLOAT32_VEC3, dir);
    anariSetParameter(f.device, camera, "up", ANARI_FLOAT32_VEC3, up);
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

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    const float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
    };
    const float perCylRadii[] = {0.1f, 0.2f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    anariSetParameter(f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);

    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    anariSetParameter(f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);

    const float radius = 0.05f;
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

    const float positions[] = {
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

    const uint8_t primColors[] = {
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

void GeometryTest::quadGeometry()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "quad");
    CORRADE_VERIFY(geom);

    // One quad: 4 vertices
    const float vertices[] = {
        -1.0f, -1.0f, -3.0f,
         1.0f, -1.0f, -3.0f,
         1.0f,  1.0f, -3.0f,
        -1.0f,  1.0f, -3.0f};
    const float normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertices, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D norArr = anariNewArray1D(
        f.device, normals, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.normal", ANARI_ARRAY1D, &norArr);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariCommitParameters(f.device, mat);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surf, nullptr, nullptr, ANARI_SURFACE, 1);
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
    anariCommitParameters(f.device, camera);

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
    anariRelease(f.device, camera);
    anariRelease(f.device, lightArr);
    anariRelease(f.device, light);
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surf);
    anariRelease(f.device, mat);
    anariRelease(f.device, norArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::coneGeometry()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "cone");
    CORRADE_VERIFY(geom);

    // Two endpoints forming one cone segment
    const float positions[] = {
        0.0f, 0.0f, -3.0f,
        0.0f, 1.0f, -3.0f};
    const float radii[] = {0.3f, 0.05f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);
    ANARIArray1D radArr = anariNewArray1D(
        f.device, radii, nullptr, nullptr, ANARI_FLOAT32, 2);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariCommitParameters(f.device, mat);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    anariRelease(f.device, surf);
    anariRelease(f.device, mat);
    anariRelease(f.device, radArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::cylinderWithCaps()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "cylinder");
    CORRADE_VERIFY(geom);

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
    };
    const float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
    };

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);

    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 2);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);

    const float radius = 0.2f;
    anariSetParameter(f.device, geom, "radius", ANARI_FLOAT32, &radius);

    anariSetParameter(
        f.device, geom, "caps", ANARI_STRING, "both");
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, colArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::coneWithCaps()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "cone");
    CORRADE_VERIFY(geom);

    // Two cones: 4 endpoints
    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
    };
    const float radii[] = {0.3f, 0.1f, 0.2f, 0.05f};
    const float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
        1.0f, 1.0f, 0.0f, 1.0f,
    };
    // Per-endpoint caps: cap first pair, only cap B of second pair
    const uint8_t capFlags[] = {1, 1, 0, 1};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D radArr = anariNewArray1D(
        f.device, radii, nullptr, nullptr, ANARI_FLOAT32, 4);
    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    ANARIArray1D capArr = anariNewArray1D(
        f.device, capFlags, nullptr, nullptr, ANARI_UINT8, 4);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(
        f.device, geom, "vertex.cap", ANARI_ARRAY1D, &capArr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, capArr);
    anariRelease(f.device, colArr);
    anariRelease(f.device, radArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::triangleWithUV()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    CORRADE_VERIFY(geom);

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 0.0f,
    };
    // UV0 as FLOAT32_VEC2
    const float uv0[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    // UV1 as FLOAT32 (scalar expansion to float2)
    const float uv1[] = {0.0f, 0.5f, 1.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D uv0Arr = anariNewArray1D(
        f.device, uv0, nullptr, nullptr, ANARI_FLOAT32_VEC2, 3);
    ANARIArray1D uv1Arr = anariNewArray1D(
        f.device, uv1, nullptr, nullptr, ANARI_FLOAT32, 3);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &uv0Arr);
    anariSetParameter(
        f.device, geom, "vertex.attribute1", ANARI_ARRAY1D, &uv1Arr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, uv1Arr);
    anariRelease(f.device, uv0Arr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::trianglePrimitiveColorWithNormalsAndUV()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    CORRADE_VERIFY(geom);

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 0.0f,
        -1.0f, 0.0f, 0.0f,
        -0.5f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.5f,
    };
    const float normals[] = {
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f,
    };
    // UV as scalar FLOAT32 (tests expansion within primitive.color path)
    const float uv0[] = {0.0f, 0.2f, 0.4f, 0.6f, 0.8f, 1.0f};
    const float uv1[] = {1.0f, 0.8f, 0.6f, 0.4f, 0.2f, 0.0f};
    // Per-primitive color as FLOAT32_VEC3 (tests convertColors in expansion)
    const float primColors[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 6);
    ANARIArray1D norArr = anariNewArray1D(
        f.device, normals, nullptr, nullptr, ANARI_FLOAT32_VEC3, 6);
    ANARIArray1D uv0Arr = anariNewArray1D(
        f.device, uv0, nullptr, nullptr, ANARI_FLOAT32, 6);
    ANARIArray1D uv1Arr = anariNewArray1D(
        f.device, uv1, nullptr, nullptr, ANARI_FLOAT32, 6);
    ANARIArray1D primColArr = anariNewArray1D(
        f.device, primColors, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.normal", ANARI_ARRAY1D, &norArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &uv0Arr);
    anariSetParameter(
        f.device, geom, "vertex.attribute1", ANARI_ARRAY1D, &uv1Arr);
    anariSetParameter(
        f.device, geom, "primitive.color", ANARI_ARRAY1D, &primColArr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, primColArr);
    anariRelease(f.device, uv1Arr);
    anariRelease(f.device, uv0Arr);
    anariRelease(f.device, norArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::quadPrimitiveColor()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "quad");
    CORRADE_VERIFY(geom);

    // Two quads: 8 vertices
    const float vertices[] = {
        -1.0f, -1.0f, 0.0f,
         0.0f, -1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,
         0.0f, -1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,
         0.0f,  1.0f, 0.0f,
    };
    const float normals[] = {
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f,
    };
    // Per-quad color (2 quads)
    const float primColors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
    };

    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertices, nullptr, nullptr, ANARI_FLOAT32_VEC3, 8);
    ANARIArray1D norArr = anariNewArray1D(
        f.device, normals, nullptr, nullptr, ANARI_FLOAT32_VEC3, 8);
    ANARIArray1D primColArr = anariNewArray1D(
        f.device, primColors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 2);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.normal", ANARI_ARRAY1D, &norArr);
    anariSetParameter(
        f.device, geom, "primitive.color", ANARI_ARRAY1D, &primColArr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, primColArr);
    anariRelease(f.device, norArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::triangleVertexColorConversion()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    CORRADE_VERIFY(geom);

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        0.5f, 1.0f, 0.0f,
    };
    // vertex.color as FLOAT32_VEC3 (tests convertColors branch)
    const float colors[] = {1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, colArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

void GeometryTest::spherePerSphereRadius()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARIGeometry geom = anariNewGeometry(f.device, "sphere");
    CORRADE_VERIFY(geom);

    const float positions[] = {
        0.0f, 0.0f, 0.0f,
        1.0f, 0.0f, 0.0f,
        2.0f, 0.0f, 0.0f,
    };
    const float radii[] = {0.1f, 0.3f, 0.5f};
    // Sphere vertex.color (tests per-sphere color expansion)
    const float colors[] = {
        1.0f, 0.0f, 0.0f, 1.0f,
        0.0f, 1.0f, 0.0f, 1.0f,
        0.0f, 0.0f, 1.0f, 1.0f,
    };
    // Sphere UV0 as FLOAT32 scalar (tests UV scalar expansion)
    const float uv0[] = {0.0f, 0.5f, 1.0f};

    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D radArr = anariNewArray1D(
        f.device, radii, nullptr, nullptr, ANARI_FLOAT32, 3);
    ANARIArray1D colArr = anariNewArray1D(
        f.device, colors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 3);
    ANARIArray1D uv0Arr = anariNewArray1D(
        f.device, uv0, nullptr, nullptr, ANARI_FLOAT32, 3);

    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.radius", ANARI_ARRAY1D, &radArr);
    anariSetParameter(
        f.device, geom, "vertex.color", ANARI_ARRAY1D, &colArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &uv0Arr);
    anariCommitParameters(f.device, geom);

    anariRelease(f.device, uv0Arr);
    anariRelease(f.device, colArr);
    anariRelease(f.device, radArr);
    anariRelease(f.device, posArr);
    anariRelease(f.device, geom);
}

}

CORRADE_TEST_MAIN(GeometryTest)
