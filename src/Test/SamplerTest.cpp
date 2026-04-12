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

struct SamplerTest : Corrade::TestSuite::Tester {
    explicit SamplerTest();

    void image2DSampler();
    void image1DSampler();
    void image3DSampler();
    void transformSampler();
    void primitiveSampler();
};

SamplerTest::SamplerTest()
{
    addTests({&SamplerTest::image2DSampler,
        &SamplerTest::image1DSampler,
        &SamplerTest::image3DSampler,
        &SamplerTest::transformSampler,
        &SamplerTest::primitiveSampler});
}

void SamplerTest::image2DSampler()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

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
    anariSetParameter(
        f.device, sampler, "inAttribute", ANARI_STRING, "attribute0");
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

void SamplerTest::image1DSampler()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    uint8_t texData[16 * 4];
    for (int i = 0; i < 16; ++i) {
        float t = static_cast<float>(i) / 15.0f;
        texData[i * 4 + 0] = static_cast<uint8_t>((1.0f - t) * 255);
        texData[i * 4 + 1] = static_cast<uint8_t>(t * 255);
        texData[i * 4 + 2] = 128;
        texData[i * 4 + 3] = 255;
    }

    ANARIArray1D arr = anariNewArray1D(
        f.device, texData, nullptr, nullptr, ANARI_UFIXED8_VEC4, 16);

    ANARISampler sampler = anariNewSampler(f.device, "image1D");
    anariSetParameter(f.device, sampler, "image", ANARI_ARRAY1D, &arr);
    anariSetParameter(
        f.device, sampler, "inAttribute", ANARI_STRING, "attribute0");
    anariCommitParameters(f.device, sampler);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_SAMPLER, &sampler);
    anariCommitParameters(f.device, mat);

    anariRelease(f.device, arr);
    anariRelease(f.device, mat);
    anariRelease(f.device, sampler);
}

void SamplerTest::image3DSampler()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    constexpr uint32_t N = 4;
    uint8_t texData[N * N * N * 4];
    for (uint32_t z = 0; z < N; ++z)
        for (uint32_t y = 0; y < N; ++y)
            for (uint32_t x = 0; x < N; ++x) {
                size_t i = x + N * y + N * N * z;
                texData[i * 4 + 0] = static_cast<uint8_t>(x * 255 / (N - 1));
                texData[i * 4 + 1] = static_cast<uint8_t>(y * 255 / (N - 1));
                texData[i * 4 + 2] = static_cast<uint8_t>(z * 255 / (N - 1));
                texData[i * 4 + 3] = 255;
            }

    ANARIArray3D arr = anariNewArray3D(
        f.device, texData, nullptr, nullptr, ANARI_UFIXED8_VEC4, N, N, N);

    ANARISampler sampler = anariNewSampler(f.device, "image3D");
    anariSetParameter(f.device, sampler, "image", ANARI_ARRAY3D, &arr);
    anariSetParameter(
        f.device, sampler, "inAttribute", ANARI_STRING, "attribute0");
    anariCommitParameters(f.device, sampler);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_SAMPLER, &sampler);
    anariCommitParameters(f.device, mat);

    anariRelease(f.device, arr);
    anariRelease(f.device, mat);
    anariRelease(f.device, sampler);
}

void SamplerTest::transformSampler()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    ANARISampler sampler = anariNewSampler(f.device, "transform");
    anariSetParameter(
        f.device, sampler, "inAttribute", ANARI_STRING, "attribute0");

    float transform[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f,
    };
    anariSetParameter(
        f.device, sampler, "transform", ANARI_FLOAT32_MAT4, transform);
    anariCommitParameters(f.device, sampler);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_SAMPLER, &sampler);
    anariCommitParameters(f.device, mat);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    float positions[] = {0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f};
    float uvs[] = {0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 1.0f};
    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);
    ANARIArray1D uvArr = anariNewArray1D(
        f.device, uvs, nullptr, nullptr, ANARI_FLOAT32_VEC2, 3);
    anariSetParameter(f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariSetParameter(
        f.device, geom, "vertex.attribute0", ANARI_ARRAY1D, &uvArr);
    anariCommitParameters(f.device, geom);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    anariRelease(f.device, posArr);
    anariRelease(f.device, uvArr);
    anariRelease(f.device, surf);
    anariRelease(f.device, geom);
    anariRelease(f.device, mat);
    anariRelease(f.device, sampler);
}

void SamplerTest::primitiveSampler()
{
    DeviceFixture f;
    CORRADE_VERIFY(f.device);

    uint8_t primColors[] = {
        255, 0, 0, 255,
        0, 255, 0, 255,
        0, 0, 255, 255,
        255, 255, 0, 255,
    };
    ANARIArray1D arr = anariNewArray1D(
        f.device, primColors, nullptr, nullptr, ANARI_UFIXED8_VEC4, 4);

    ANARISampler sampler = anariNewSampler(f.device, "primitive");
    anariSetParameter(f.device, sampler, "array", ANARI_ARRAY1D, &arr);
    uint64_t offset = 0;
    anariSetParameter(f.device, sampler, "offset", ANARI_UINT64, &offset);
    anariCommitParameters(f.device, sampler);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    anariSetParameter(f.device, mat, "color", ANARI_SAMPLER, &sampler);
    anariCommitParameters(f.device, mat);

    float positions[] = {
        0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.0f,
        1.0f, 0.0f, 0.0f, 2.0f, 0.0f, 0.0f, 1.5f, 1.0f, 0.0f,
    };
    ANARIArray1D posArr = anariNewArray1D(
        f.device, positions, nullptr, nullptr, ANARI_FLOAT32_VEC3, 6);
    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    anariSetParameter(f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariCommitParameters(f.device, geom);

    ANARISurface surf = anariNewSurface(f.device);
    anariSetParameter(f.device, surf, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(f.device, surf, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surf);

    anariRelease(f.device, posArr);
    anariRelease(f.device, arr);
    anariRelease(f.device, surf);
    anariRelease(f.device, geom);
    anariRelease(f.device, mat);
    anariRelease(f.device, sampler);
}

}

CORRADE_TEST_MAIN(SamplerTest)
