// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

// Visual verification: renders a scene with multiple geometry types, lights,
// and materials, then writes the result to render_output.png.

#include <cstdint>
#include <cstdio>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <anari/anari.h>

static void statusFunc(const void *, ANARIDevice, ANARIObject,
    ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
    const char *message)
{
    if (severity <= ANARI_SEVERITY_WARNING)
        fprintf(stderr, "[ANARI] %s\n", message);
}

int main()
{
    ANARILibrary lib = anariLoadLibrary("filament", statusFunc);
    if (!lib) {
        fprintf(stderr, "Failed to load filament library\n");
        return 1;
    }

    ANARIDevice dev = anariNewDevice(lib, "default");
    if (!dev) {
        fprintf(stderr, "Failed to create device\n");
        anariUnloadLibrary(lib);
        return 1;
    }
    anariCommitParameters(dev, dev);

    const uint32_t W = 800, H = 600;

    // --- Triangle geometry (colored quad as 2 triangles) ---
    float triVerts[] = {
        -2.0f, -1.0f, -5.0f,
        -0.5f, -1.0f, -5.0f,
        -0.5f,  0.5f, -5.0f,
        -2.0f,  0.5f, -5.0f};
    float triColors[] = {
        0.9f, 0.2f, 0.2f, 1.0f,
        0.2f, 0.9f, 0.2f, 1.0f,
        0.2f, 0.2f, 0.9f, 1.0f,
        0.9f, 0.9f, 0.2f, 1.0f};
    uint32_t triIdx[] = {0, 1, 2, 0, 2, 3};

    ANARIArray1D triPosArr = anariNewArray1D(
        dev, triVerts, nullptr, nullptr, ANARI_FLOAT32_VEC3, 4);
    ANARIArray1D triColArr = anariNewArray1D(
        dev, triColors, nullptr, nullptr, ANARI_FLOAT32_VEC4, 4);
    ANARIArray1D triIdxArr = anariNewArray1D(
        dev, triIdx, nullptr, nullptr, ANARI_UINT32_VEC3, 2);

    ANARIGeometry triGeom = anariNewGeometry(dev, "triangle");
    anariSetParameter(dev, triGeom, "vertex.position", ANARI_ARRAY1D, &triPosArr);
    anariSetParameter(dev, triGeom, "vertex.color", ANARI_ARRAY1D, &triColArr);
    anariSetParameter(dev, triGeom, "primitive.index", ANARI_ARRAY1D, &triIdxArr);
    anariCommitParameters(dev, triGeom);

    ANARIMaterial matteMat = anariNewMaterial(dev, "matte");
    anariSetParameter(dev, matteMat, "color", ANARI_STRING, "color");
    anariCommitParameters(dev, matteMat);

    ANARISurface triSurf = anariNewSurface(dev);
    anariSetParameter(dev, triSurf, "geometry", ANARI_GEOMETRY, &triGeom);
    anariSetParameter(dev, triSurf, "material", ANARI_MATERIAL, &matteMat);
    anariCommitParameters(dev, triSurf);

    // --- Sphere geometry ---
    float spherePos[] = {0.5f, 0.0f, -4.0f, 1.5f, -0.5f, -3.5f};
    float sphereRad[] = {0.4f, 0.25f};
    float sphereCol[] = {
        0.8f, 0.3f, 0.8f, 1.0f,
        0.3f, 0.8f, 0.8f, 1.0f};

    ANARIArray1D sphPosArr = anariNewArray1D(
        dev, spherePos, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);
    ANARIArray1D sphRadArr = anariNewArray1D(
        dev, sphereRad, nullptr, nullptr, ANARI_FLOAT32, 2);
    ANARIArray1D sphColArr = anariNewArray1D(
        dev, sphereCol, nullptr, nullptr, ANARI_FLOAT32_VEC4, 2);

    ANARIGeometry sphGeom = anariNewGeometry(dev, "sphere");
    anariSetParameter(dev, sphGeom, "vertex.position", ANARI_ARRAY1D, &sphPosArr);
    anariSetParameter(dev, sphGeom, "vertex.radius", ANARI_ARRAY1D, &sphRadArr);
    anariSetParameter(dev, sphGeom, "vertex.color", ANARI_ARRAY1D, &sphColArr);
    anariCommitParameters(dev, sphGeom);

    ANARIMaterial pbrMat = anariNewMaterial(dev, "physicallyBased");
    float baseColor[] = {1.0f, 1.0f, 1.0f};
    float metallic = 0.8f;
    float roughness = 0.2f;
    anariSetParameter(dev, pbrMat, "baseColor", ANARI_STRING, "color");
    anariSetParameter(dev, pbrMat, "metallic", ANARI_FLOAT32, &metallic);
    anariSetParameter(dev, pbrMat, "roughness", ANARI_FLOAT32, &roughness);
    anariCommitParameters(dev, pbrMat);

    ANARISurface sphSurf = anariNewSurface(dev);
    anariSetParameter(dev, sphSurf, "geometry", ANARI_GEOMETRY, &sphGeom);
    anariSetParameter(dev, sphSurf, "material", ANARI_MATERIAL, &pbrMat);
    anariCommitParameters(dev, sphSurf);

    // --- Cylinder geometry ---
    float cylPos[] = {
        2.0f, -1.0f, -5.0f,
        2.0f,  0.5f, -5.0f};
    ANARIArray1D cylPosArr = anariNewArray1D(
        dev, cylPos, nullptr, nullptr, ANARI_FLOAT32_VEC3, 2);

    ANARIGeometry cylGeom = anariNewGeometry(dev, "cylinder");
    anariSetParameter(dev, cylGeom, "vertex.position", ANARI_ARRAY1D, &cylPosArr);
    float cylRad = 0.2f;
    anariSetParameter(dev, cylGeom, "radius", ANARI_FLOAT32, &cylRad);
    anariCommitParameters(dev, cylGeom);

    ANARIMaterial cylMat = anariNewMaterial(dev, "matte");
    float cylColor[] = {0.2f, 0.6f, 0.9f};
    anariSetParameter(dev, cylMat, "color", ANARI_FLOAT32_VEC3, cylColor);
    anariCommitParameters(dev, cylMat);

    ANARISurface cylSurf = anariNewSurface(dev);
    anariSetParameter(dev, cylSurf, "geometry", ANARI_GEOMETRY, &cylGeom);
    anariSetParameter(dev, cylSurf, "material", ANARI_MATERIAL, &cylMat);
    anariCommitParameters(dev, cylSurf);

    // --- World ---
    ANARISurface surfaces[] = {triSurf, sphSurf, cylSurf};
    ANARIArray1D surfArr = anariNewArray1D(
        dev, surfaces, nullptr, nullptr, ANARI_SURFACE, 3);

    ANARIWorld world = anariNewWorld(dev);
    anariSetParameter(dev, world, "surface", ANARI_ARRAY1D, &surfArr);

    // Directional light
    ANARILight dirLight = anariNewLight(dev, "directional");
    float lightDir[] = {-1.0f, -1.0f, -1.0f};
    float lightColor[] = {1.0f, 0.95f, 0.9f};
    float irradiance = 3.0f;
    anariSetParameter(dev, dirLight, "direction", ANARI_FLOAT32_VEC3, lightDir);
    anariSetParameter(dev, dirLight, "color", ANARI_FLOAT32_VEC3, lightColor);
    anariSetParameter(dev, dirLight, "irradiance", ANARI_FLOAT32, &irradiance);
    anariCommitParameters(dev, dirLight);

    // Point light
    ANARILight pointLight = anariNewLight(dev, "point");
    float ptPos[] = {0.0f, 2.0f, -2.0f};
    float ptColor[] = {1.0f, 0.8f, 0.5f};
    float ptIntensity = 30.0f;
    anariSetParameter(dev, pointLight, "position", ANARI_FLOAT32_VEC3, ptPos);
    anariSetParameter(dev, pointLight, "color", ANARI_FLOAT32_VEC3, ptColor);
    anariSetParameter(dev, pointLight, "intensity", ANARI_FLOAT32, &ptIntensity);
    anariCommitParameters(dev, pointLight);

    ANARILight lights[] = {dirLight, pointLight};
    ANARIArray1D lightArr = anariNewArray1D(
        dev, lights, nullptr, nullptr, ANARI_LIGHT, 2);
    anariSetParameter(dev, world, "light", ANARI_ARRAY1D, &lightArr);
    anariCommitParameters(dev, world);

    // --- Camera ---
    ANARICamera camera = anariNewCamera(dev, "perspective");
    float camPos[] = {0.0f, 0.5f, 1.0f};
    float camDir[] = {0.0f, -0.1f, -1.0f};
    float camUp[] = {0.0f, 1.0f, 0.0f};
    float aspect = float(W) / float(H);
    anariSetParameter(dev, camera, "position", ANARI_FLOAT32_VEC3, camPos);
    anariSetParameter(dev, camera, "direction", ANARI_FLOAT32_VEC3, camDir);
    anariSetParameter(dev, camera, "up", ANARI_FLOAT32_VEC3, camUp);
    anariSetParameter(dev, camera, "aspect", ANARI_FLOAT32, &aspect);
    anariCommitParameters(dev, camera);

    // --- Renderer ---
    ANARIRenderer renderer = anariNewRenderer(dev, "default");
    float bgColor[] = {0.1f, 0.1f, 0.15f, 1.0f};
    float ambientColor[] = {0.4f, 0.45f, 0.5f};
    float ambientRadiance = 0.5f;
    anariSetParameter(dev, renderer, "background", ANARI_FLOAT32_VEC4, bgColor);
    anariSetParameter(
        dev, renderer, "ambientColor", ANARI_FLOAT32_VEC3, ambientColor);
    anariSetParameter(
        dev, renderer, "ambientRadiance", ANARI_FLOAT32, &ambientRadiance);
    anariCommitParameters(dev, renderer);

    // --- Frame ---
    ANARIFrame frame = anariNewFrame(dev);
    uint32_t imgSize[] = {W, H};
    ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
    anariSetParameter(dev, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(dev, frame, "channel.color", ANARI_DATA_TYPE, &colorType);
    anariSetParameter(dev, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(dev, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(dev, frame, "world", ANARI_WORLD, &world);
    anariCommitParameters(dev, frame);

    // --- Render ---
    printf("Rendering %ux%u scene...\n", W, H);
    anariRenderFrame(dev, frame);
    anariFrameReady(dev, frame, ANARI_WAIT);

    uint32_t fbW = 0, fbH = 0;
    ANARIDataType fbType = ANARI_UNKNOWN;
    auto *pixels = static_cast<const uint8_t *>(
        anariMapFrame(dev, frame, "channel.color", &fbW, &fbH, &fbType));

    if (pixels && fbW > 0 && fbH > 0) {
        stbi_flip_vertically_on_write(1);
        stbi_write_png(
            "render_output.png", int(fbW), int(fbH), 4, pixels, 4 * int(fbW));
        printf("Saved render_output.png (%ux%u)\n", fbW, fbH);
    } else {
        fprintf(stderr, "Failed to map frame buffer\n");
    }

    anariUnmapFrame(dev, frame, "channel.color");

    // --- Cleanup ---
    anariRelease(dev, frame);
    anariRelease(dev, renderer);
    anariRelease(dev, camera);
    anariRelease(dev, lightArr);
    anariRelease(dev, pointLight);
    anariRelease(dev, dirLight);
    anariRelease(dev, surfArr);
    anariRelease(dev, world);
    anariRelease(dev, cylSurf);
    anariRelease(dev, cylMat);
    anariRelease(dev, cylGeom);
    anariRelease(dev, cylPosArr);
    anariRelease(dev, sphSurf);
    anariRelease(dev, pbrMat);
    anariRelease(dev, sphGeom);
    anariRelease(dev, sphColArr);
    anariRelease(dev, sphRadArr);
    anariRelease(dev, sphPosArr);
    anariRelease(dev, triSurf);
    anariRelease(dev, matteMat);
    anariRelease(dev, triGeom);
    anariRelease(dev, triIdxArr);
    anariRelease(dev, triColArr);
    anariRelease(dev, triPosArr);
    anariRelease(dev, dev);
    anariUnloadLibrary(lib);

    return 0;
}
