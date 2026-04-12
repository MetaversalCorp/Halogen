// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Geometry.h"

#include "Aabb.h"
#include "ColorConversion.h"

#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/VertexBuffer.h>

#include <geometry/SurfaceOrientation.h>

#include <helium/array/Array1D.h>

#include <Corrade/Containers/Array.h>
#include <Corrade/Containers/StringStl.h>
#include <Corrade/Containers/StringView.h>

#include <cmath>
#include <cstring>

using namespace Corrade::Containers::Literals;

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Geometry *);

namespace AnariFilament {



Geometry::Geometry(DeviceState *s, const char *subtype)
    : Object(ANARI_GEOMETRY, s)
    , mSubtype(subtype ? subtype : "triangle") {}

Geometry::~Geometry()
{
    filament::Engine *engine = deviceState()->engine;
    if (mVertexBuffer)
        engine->destroy(mVertexBuffer);
    if (mIndexBuffer)
        engine->destroy(mIndexBuffer);
}

void Geometry::commitParameters()
{
    if (mSubtype == "sphere"_s)
        commitSphere();
    else if (mSubtype == "cylinder"_s)
        commitCylinder();
    else if (mSubtype == "quad"_s)
        commitQuad();
    else if (mSubtype == "cone"_s)
        commitCone();
    else
        commitTriangle();
}

void Geometry::commitTriangle()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *norArray =
        getParamObject<helium::Array1D>("vertex.normal");
    helium::Array1D *idxArray =
        getParamObject<helium::Array1D>("primitive.index");
    helium::Array1D *attr0Array =
        getParamObject<helium::Array1D>("vertex.attribute0");
    helium::Array1D *attr1Array =
        getParamObject<helium::Array1D>("vertex.attribute1");
    helium::Array1D *primColArray =
        getParamObject<helium::Array1D>("primitive.color");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "triangle geometry requires 'vertex.position'");
        return;
    }

    if (mVertexBuffer) {
        engine->destroy(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        engine->destroy(mIndexBuffer);
        mIndexBuffer = nullptr;
    }

    uint32_t numVertices = static_cast<uint32_t>(posArray->totalSize());
    mHasColors = colArray != nullptr || primColArray != nullptr;
    mHasUV0 = attr0Array != nullptr;
    mHasUV1 = attr1Array != nullptr;

    // Generate indices if not provided
    uint32_t numTriangles = 0;
    Corrade::Containers::Array<uint32_t> generatedIndices;
    const uint32_t *indexData = nullptr;

    if (idxArray) {
        numTriangles = static_cast<uint32_t>(idxArray->totalSize());
        indexData = static_cast<const uint32_t *>(idxArray->data());
    } else {
        numTriangles = numVertices / 3;
        generatedIndices = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, numVertices};
        for (uint32_t i = 0; i < numVertices; ++i)
            generatedIndices[i] = i;
        indexData = generatedIndices.data();
    }
    mIndexCount = numTriangles * 3;

    const filament::math::float3 *posData =
        static_cast<const filament::math::float3 *>(posArray->data());

    // Expand geometry for primitive.color — each triangle gets unique vertices
    Corrade::Containers::Array<filament::math::float3> expandedPos;
    Corrade::Containers::Array<filament::math::float3> expandedNor;
    Corrade::Containers::Array<filament::math::float2> expandedUV0;
    Corrade::Containers::Array<filament::math::float2> expandedUV1;
    Corrade::Containers::Array<filament::math::float4> expandedColors;
    Corrade::Containers::Array<uint32_t> expandedIndices;

    if (primColArray) {
        uint32_t expVerts = numTriangles * 3;
        expandedPos = Corrade::Containers::Array<filament::math::float3>{
            Corrade::NoInit, expVerts};
        expandedIndices = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, expVerts};
        expandedColors = Corrade::Containers::Array<filament::math::float4>{
            Corrade::NoInit, expVerts};

        const filament::math::float3 *norData = norArray
            ? static_cast<const filament::math::float3 *>(norArray->data())
            : nullptr;
        if (norData) {
            expandedNor = Corrade::Containers::Array<filament::math::float3>{
                Corrade::NoInit, expVerts};
        }

        if (attr0Array) {
            expandedUV0 = Corrade::Containers::Array<filament::math::float2>{
                Corrade::NoInit, expVerts};
        }
        if (attr1Array) {
            expandedUV1 = Corrade::Containers::Array<filament::math::float2>{
                Corrade::NoInit, expVerts};
        }

        ANARIDataType primColType = primColArray->elementType();
        const void *primColData = primColArray->data();

        Corrade::Containers::Array<filament::math::float4> primColConverted{
            Corrade::NoInit, numTriangles};
        convertColors(primColConverted.data(), primColData, primColType,
            numTriangles);

        for (uint32_t t = 0; t < numTriangles; ++t) {
            uint32_t i0 = indexData[t * 3 + 0];
            uint32_t i1 = indexData[t * 3 + 1];
            uint32_t i2 = indexData[t * 3 + 2];
            uint32_t base = t * 3;

            expandedPos[base + 0] = posData[i0];
            expandedPos[base + 1] = posData[i1];
            expandedPos[base + 2] = posData[i2];

            if (norData) {
                expandedNor[base + 0] = norData[i0];
                expandedNor[base + 1] = norData[i1];
                expandedNor[base + 2] = norData[i2];
            }

            if (attr0Array) {
                ANARIDataType a0Type = attr0Array->elementType();
                const float *uvSrc =
                    static_cast<const float *>(attr0Array->data());
                if (a0Type == ANARI_FLOAT32_VEC2) {
                    expandedUV0[base + 0] = {uvSrc[i0 * 2], uvSrc[i0 * 2 + 1]};
                    expandedUV0[base + 1] = {uvSrc[i1 * 2], uvSrc[i1 * 2 + 1]};
                    expandedUV0[base + 2] = {uvSrc[i2 * 2], uvSrc[i2 * 2 + 1]};
                } else {
                    expandedUV0[base + 0] = {uvSrc[i0], 0.0f};
                    expandedUV0[base + 1] = {uvSrc[i1], 0.0f};
                    expandedUV0[base + 2] = {uvSrc[i2], 0.0f};
                }
            }
            if (attr1Array) {
                ANARIDataType a1Type = attr1Array->elementType();
                const float *uvSrc =
                    static_cast<const float *>(attr1Array->data());
                if (a1Type == ANARI_FLOAT32_VEC2) {
                    expandedUV1[base + 0] = {uvSrc[i0 * 2], uvSrc[i0 * 2 + 1]};
                    expandedUV1[base + 1] = {uvSrc[i1 * 2], uvSrc[i1 * 2 + 1]};
                    expandedUV1[base + 2] = {uvSrc[i2 * 2], uvSrc[i2 * 2 + 1]};
                } else {
                    expandedUV1[base + 0] = {uvSrc[i0], 0.0f};
                    expandedUV1[base + 1] = {uvSrc[i1], 0.0f};
                    expandedUV1[base + 2] = {uvSrc[i2], 0.0f};
                }
            }

            filament::math::float4 primColor = primColConverted[t];
            expandedColors[base + 0] = primColor;
            expandedColors[base + 1] = primColor;
            expandedColors[base + 2] = primColor;

            expandedIndices[base + 0] = base + 0;
            expandedIndices[base + 1] = base + 1;
            expandedIndices[base + 2] = base + 2;
        }

        numVertices = expVerts;
        posData = expandedPos.data();
        indexData = expandedIndices.data();
    }

    const filament::math::uint3 *triData =
        reinterpret_cast<const filament::math::uint3 *>(indexData);

    // Generate tangent frames using SurfaceOrientation
    filament::geometry::SurfaceOrientation::Builder orientBuilder;
    orientBuilder.vertexCount(numVertices)
        .positions(posData)
        .triangleCount(numTriangles)
        .triangles(triData);

    if (norArray) {
        const filament::math::float3 *norData = primColArray
            ? expandedNor.data()
            : static_cast<const filament::math::float3 *>(norArray->data());
        if (norData)
            orientBuilder.normals(norData);
    }

    filament::geometry::SurfaceOrientation *orientation =
        orientBuilder.build();

    Corrade::Containers::Array<filament::math::short4> tangents{
        Corrade::NoInit, numVertices};
    orientation->getQuats(tangents.data(), numVertices);
    delete orientation;

    // Count buffers: POSITION + TANGENTS + optional COLOR + optional UV0 + optional UV1
    uint8_t bufIdx = 0;
    uint8_t posBuffer = bufIdx++;
    uint8_t tangentBuffer = bufIdx++;
    uint8_t colorBuffer = mHasColors ? bufIdx++ : 0;
    uint8_t uv0Buffer = mHasUV0 ? bufIdx++ : 0;
    uint8_t uv1Buffer = mHasUV1 ? bufIdx++ : 0;
    uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(numVertices)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        builder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }

    if (mHasUV0) {
        // Determine attribute0 element type to pick the right format
        ANARIDataType a0Type = attr0Array->elementType();
        if (a0Type == ANARI_FLOAT32_VEC2 || a0Type == ANARI_FLOAT64_VEC2) {
            builder.attribute(filament::VertexAttribute::UV0, uv0Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2);
        } else {
            // Scalar float — pack as float2 (value, 0)
            builder.attribute(filament::VertexAttribute::UV0, uv0Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2);
        }
    }

    if (mHasUV1) {
        builder.attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);
    }

    mVertexBuffer = builder.build(*engine);

    // Upload position data
    size_t posSize = numVertices * sizeof(float) * 3;
    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            posData, posSize));

    // Upload tangent data (transfer ownership via callback)
    auto *tangentOwned = new filament::math::short4[numVertices];
    std::memcpy(tangentOwned, tangents.data(),
        numVertices * sizeof(filament::math::short4));
    mVertexBuffer->setBufferAt(*engine, tangentBuffer,
        filament::VertexBuffer::BufferDescriptor(
            tangentOwned,
            numVertices * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));

    if (mHasColors) {
        if (primColArray) {
            // Colors were pre-computed during expansion
            auto *colorData = new filament::math::float4[numVertices];
            std::memcpy(colorData, expandedColors.data(),
                numVertices * sizeof(filament::math::float4));
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colorData,
                    numVertices * sizeof(filament::math::float4),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float4 *>(buf);
                    }));
        } else if (colArray->elementType() == ANARI_FLOAT32_VEC4) {
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colArray->data(),
                    colArray->totalSize() * sizeof(float) * 4));
        } else {
            // Convert from any supported color type to FLOAT4
            auto *colorData = new filament::math::float4[numVertices];
            convertColors(colorData, colArray->data(),
                colArray->elementType(), numVertices);
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colorData,
                    numVertices * sizeof(filament::math::float4),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float4 *>(buf);
                    }));
        }
    }

    if (mHasUV0) {
        if (primColArray && expandedUV0.data()) {
            mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    expandedUV0.data(),
                    numVertices * sizeof(filament::math::float2)));
        } else {
            ANARIDataType a0Type = attr0Array->elementType();
            if (a0Type == ANARI_FLOAT32_VEC2) {
                mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                    filament::VertexBuffer::BufferDescriptor(
                        attr0Array->data(),
                        attr0Array->totalSize() * sizeof(float) * 2));
            } else if (a0Type == ANARI_FLOAT32) {
                auto *uv0Data = new filament::math::float2[numVertices];
                const float *src =
                    static_cast<const float *>(attr0Array->data());
                for (uint32_t i = 0; i < numVertices; ++i)
                    uv0Data[i] = {src[i], 0.0f};
                mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                    filament::VertexBuffer::BufferDescriptor(
                        uv0Data, numVertices * sizeof(filament::math::float2),
                        [](void *buf, size_t, void *) {
                            delete[] static_cast<filament::math::float2 *>(buf);
                        }));
            }
        }
    }

    if (mHasUV1) {
        if (primColArray && expandedUV1.data()) {
            mVertexBuffer->setBufferAt(*engine, uv1Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    expandedUV1.data(),
                    numVertices * sizeof(filament::math::float2)));
        } else {
            ANARIDataType a1Type = attr1Array->elementType();
            if (a1Type == ANARI_FLOAT32_VEC2) {
                mVertexBuffer->setBufferAt(*engine, uv1Buffer,
                    filament::VertexBuffer::BufferDescriptor(
                        attr1Array->data(),
                        attr1Array->totalSize() * sizeof(float) * 2));
            } else if (a1Type == ANARI_FLOAT32) {
                auto *uv1Data = new filament::math::float2[numVertices];
                const float *src =
                    static_cast<const float *>(attr1Array->data());
                for (uint32_t i = 0; i < numVertices; ++i)
                    uv1Data[i] = {src[i], 0.0f};
                mVertexBuffer->setBufferAt(*engine, uv1Buffer,
                    filament::VertexBuffer::BufferDescriptor(
                        uv1Data, numVertices * sizeof(filament::math::float2),
                        [](void *buf, size_t, void *) {
                            delete[] static_cast<filament::math::float2 *>(buf);
                        }));
            }
        }
    }

    // Compute AABB
    mAabb = computeAabb(posData, numVertices);

    // Index buffer
    mIndexBuffer = filament::IndexBuffer::Builder()
        .indexCount(mIndexCount)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mIndexBuffer->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            indexData, mIndexCount * sizeof(uint32_t)));

    markCommitted();
}

// -- Sphere tessellation --

namespace {

struct SphereVertex {
    filament::math::float3 position;
    filament::math::float3 normal;
};

constexpr uint32_t SPHERE_SEGMENTS = 16;
constexpr uint32_t SPHERE_RINGS = 8;

// Returns (vertices, indices) for a unit sphere centered at origin
void generateUnitSphere(
    Corrade::Containers::Array<SphereVertex> &verts,
    Corrade::Containers::Array<uint32_t> &indices)
{
    uint32_t vertCount = (SPHERE_RINGS + 1) * (SPHERE_SEGMENTS + 1);
    uint32_t idxCount = SPHERE_RINGS * SPHERE_SEGMENTS * 6;

    verts = Corrade::Containers::Array<SphereVertex>{
        Corrade::NoInit, vertCount};
    indices = Corrade::Containers::Array<uint32_t>{
        Corrade::NoInit, idxCount};

    constexpr float pi = 3.14159265358979323846f;

    uint32_t v = 0;
    for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
        float phi = pi * static_cast<float>(ring) / SPHERE_RINGS;
        float sinPhi = std::sin(phi);
        float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= SPHERE_SEGMENTS; ++seg) {
            float theta = 2.0f * pi * static_cast<float>(seg) / SPHERE_SEGMENTS;
            float sinTheta = std::sin(theta);
            float cosTheta = std::cos(theta);

            filament::math::float3 n = {
                sinPhi * cosTheta, cosPhi, sinPhi * sinTheta};
            verts[v++] = {n, n};
        }
    }

    uint32_t idx = 0;
    for (uint32_t ring = 0; ring < SPHERE_RINGS; ++ring) {
        for (uint32_t seg = 0; seg < SPHERE_SEGMENTS; ++seg) {
            uint32_t a = ring * (SPHERE_SEGMENTS + 1) + seg;
            uint32_t b = a + SPHERE_SEGMENTS + 1;

            indices[idx++] = a;
            indices[idx++] = b;
            indices[idx++] = a + 1;
            indices[idx++] = a + 1;
            indices[idx++] = b;
            indices[idx++] = b + 1;
        }
    }
}

}

void Geometry::commitSphere()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *radiusArray =
        getParamObject<helium::Array1D>("vertex.radius");
    helium::Array1D *attr0Array =
        getParamObject<helium::Array1D>("vertex.attribute0");
    helium::Array1D *attr1Array =
        getParamObject<helium::Array1D>("vertex.attribute1");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "sphere geometry requires 'vertex.position'");
        return;
    }

    if (mVertexBuffer) {
        engine->destroy(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        engine->destroy(mIndexBuffer);
        mIndexBuffer = nullptr;
    }

    float globalRadius = getParam<float>("radius", 0.01f);
    uint32_t numSpheres = static_cast<uint32_t>(posArray->totalSize());
    const filament::math::float3 *centers =
        static_cast<const filament::math::float3 *>(posArray->data());
    const float *radii = radiusArray
        ? static_cast<const float *>(radiusArray->data())
        : nullptr;

    mHasColors = colArray != nullptr;
    mHasUV0 = attr0Array != nullptr;
    mHasUV1 = attr1Array != nullptr;

    // Generate unit sphere template
    Corrade::Containers::Array<SphereVertex> unitVerts;
    Corrade::Containers::Array<uint32_t> unitIndices;
    generateUnitSphere(unitVerts, unitIndices);

    uint32_t vertsPerSphere = static_cast<uint32_t>(unitVerts.size());
    uint32_t indicesPerSphere = static_cast<uint32_t>(unitIndices.size());
    uint32_t totalVerts = numSpheres * vertsPerSphere;
    uint32_t totalIndices = numSpheres * indicesPerSphere;

    // Build position and normal arrays for all spheres
    auto *positions = new filament::math::float3[totalVerts];
    auto *normals = new filament::math::float3[totalVerts];
    auto *allIndices = new uint32_t[totalIndices];

    for (uint32_t s = 0; s < numSpheres; ++s) {
        float r = radii ? radii[s] : globalRadius;
        filament::math::float3 c = centers[s];
        uint32_t vBase = s * vertsPerSphere;
        uint32_t iBase = s * indicesPerSphere;

        for (uint32_t v = 0; v < vertsPerSphere; ++v) {
            positions[vBase + v] = {
                c.x + unitVerts[v].position.x * r,
                c.y + unitVerts[v].position.y * r,
                c.z + unitVerts[v].position.z * r};
            normals[vBase + v] = unitVerts[v].normal;
        }

        for (uint32_t i = 0; i < indicesPerSphere; ++i)
            allIndices[iBase + i] = vBase + unitIndices[i];
    }

    // Generate tangent frames
    filament::geometry::SurfaceOrientation *orientation =
        filament::geometry::SurfaceOrientation::Builder()
            .vertexCount(totalVerts)
            .normals(normals)
            .build();

    auto *tangents = new filament::math::short4[totalVerts];
    orientation->getQuats(tangents, totalVerts);
    delete orientation;

    // Build vertex buffer
    uint8_t bufIdx = 0;
    uint8_t posBuffer = bufIdx++;
    uint8_t tangentBuffer = bufIdx++;
    uint8_t colorBuffer = mHasColors ? bufIdx++ : 0;
    uint8_t uv0Buffer = mHasUV0 ? bufIdx++ : 0;
    uint8_t uv1Buffer = mHasUV1 ? bufIdx++ : 0;
    uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(totalVerts)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        builder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }
    if (mHasUV0) {
        builder.attribute(filament::VertexAttribute::UV0, uv0Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);
    }
    if (mHasUV1) {
        builder.attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);
    }

    mVertexBuffer = builder.build(*engine);

    // Upload positions (transfer ownership)
    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            positions, totalVerts * sizeof(filament::math::float3),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::float3 *>(buf);
            }));

    // Upload tangents (transfer ownership)
    mVertexBuffer->setBufferAt(*engine, tangentBuffer,
        filament::VertexBuffer::BufferDescriptor(
            tangents, totalVerts * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));

    // Upload per-sphere colors (expand to per-vertex)
    if (mHasColors) {
        auto *colors = new filament::math::float4[totalVerts];
        const filament::math::float4 *srcColors =
            static_cast<const filament::math::float4 *>(colArray->data());
        for (uint32_t s = 0; s < numSpheres; ++s) {
            for (uint32_t v = 0; v < vertsPerSphere; ++v)
                colors[s * vertsPerSphere + v] = srcColors[s];
        }
        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                colors, totalVerts * sizeof(filament::math::float4),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float4 *>(buf);
                }));
    }

    // Upload per-sphere attribute0 (expand to per-vertex as float2)
    if (mHasUV0) {
        auto *uv0Data = new filament::math::float2[totalVerts];
        const float *src =
            static_cast<const float *>(attr0Array->data());
        for (uint32_t s = 0; s < numSpheres; ++s) {
            for (uint32_t v = 0; v < vertsPerSphere; ++v)
                uv0Data[s * vertsPerSphere + v] = {src[s], 0.0f};
        }
        mVertexBuffer->setBufferAt(*engine, uv0Buffer,
            filament::VertexBuffer::BufferDescriptor(
                uv0Data, totalVerts * sizeof(filament::math::float2),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float2 *>(buf);
                }));
    }

    // Upload per-sphere attribute1 (expand to per-vertex as float2)
    if (mHasUV1) {
        auto *uv1Data = new filament::math::float2[totalVerts];
        const float *src =
            static_cast<const float *>(attr1Array->data());
        for (uint32_t s = 0; s < numSpheres; ++s) {
            for (uint32_t v = 0; v < vertsPerSphere; ++v)
                uv1Data[s * vertsPerSphere + v] = {src[s], 0.0f};
        }
        mVertexBuffer->setBufferAt(*engine, uv1Buffer,
            filament::VertexBuffer::BufferDescriptor(
                uv1Data, totalVerts * sizeof(filament::math::float2),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float2 *>(buf);
                }));
    }

    // Compute AABB
    // For normals array: we allocated it on heap. Compute AABB from positions.
    mAabb = computeAabb(positions, totalVerts);

    // Can't delete positions yet — Filament owns them via the callback.
    // The normals are only used for orientation, can delete now.
    delete[] normals;

    mIndexCount = totalIndices;
    mIndexBuffer = filament::IndexBuffer::Builder()
        .indexCount(totalIndices)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mIndexBuffer->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            allIndices, totalIndices * sizeof(uint32_t),
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint32_t *>(buf);
            }));

    markCommitted();
}

// -- Cylinder tessellation --

namespace {

constexpr uint32_t CYLINDER_SEGMENTS = 12;

// Build perpendicular vectors to a given axis
void buildFrame(filament::math::float3 axis,
    filament::math::float3 &u, filament::math::float3 &v)
{
    filament::math::float3 a = {1.0f, 0.0f, 0.0f};
    if (std::abs(dot(axis, a)) > 0.9f)
        a = {0.0f, 1.0f, 0.0f};
    u = normalize(cross(axis, a));
    v = cross(axis, u);
}

}

void Geometry::commitCylinder()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *primRadiusArray =
        getParamObject<helium::Array1D>("primitive.radius");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "cylinder geometry requires 'vertex.position'");
        return;
    }

    if (mVertexBuffer) {
        engine->destroy(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        engine->destroy(mIndexBuffer);
        mIndexBuffer = nullptr;
    }

    float globalRadius = getParam<float>("radius", 1.0f);
    Corrade::Containers::String capsStr = getParamString("caps", "none");

    uint32_t numEndpoints = static_cast<uint32_t>(posArray->totalSize());
    uint32_t numCylinders = numEndpoints / 2;
    const filament::math::float3 *endpoints =
        static_cast<const filament::math::float3 *>(posArray->data());
    const float *primRadii = primRadiusArray
        ? static_cast<const float *>(primRadiusArray->data())
        : nullptr;

    mHasColors = colArray != nullptr;
    mHasUV0 = false;
    mHasUV1 = false;

    bool addCaps = (capsStr != "none"_s);
    constexpr uint32_t S = CYLINDER_SEGMENTS;

    // Per cylinder:
    //   Tube: (S + 1) * 2 vertices, S * 2 triangles (6 indices)
    //   Caps: S + 1 vertices per cap (center + S rim), S triangles per cap
    uint32_t tubeVerts = (S + 1) * 2;
    uint32_t tubeIndices = S * 6;
    uint32_t capVerts = addCaps ? (S + 1) * 2 : 0;
    uint32_t capIndices = addCaps ? S * 3 * 2 : 0;
    uint32_t vertsPerCyl = tubeVerts + capVerts;
    uint32_t indicesPerCyl = tubeIndices + capIndices;

    uint32_t totalVerts = numCylinders * vertsPerCyl;
    uint32_t totalIndices = numCylinders * indicesPerCyl;

    auto *positions = new filament::math::float3[totalVerts];
    auto *normals = new filament::math::float3[totalVerts];
    auto *allIndices = new uint32_t[totalIndices];

    constexpr float pi = 3.14159265358979323846f;

    for (uint32_t c = 0; c < numCylinders; ++c) {
        filament::math::float3 A = endpoints[c * 2 + 0];
        filament::math::float3 B = endpoints[c * 2 + 1];
        float r = primRadii ? primRadii[c] : globalRadius;

        filament::math::float3 axis = B - A;
        float len = length(axis);
        if (len < 1e-12f)
            axis = {0.0f, 1.0f, 0.0f};
        else
            axis /= len;

        filament::math::float3 u, v;
        buildFrame(axis, u, v);

        uint32_t vBase = c * vertsPerCyl;
        uint32_t iBase = c * indicesPerCyl;

        // Generate tube vertices: ring at A, then ring at B
        for (uint32_t seg = 0; seg <= S; ++seg) {
            float theta = 2.0f * pi * static_cast<float>(seg) / S;
            float ct = std::cos(theta);
            float st = std::sin(theta);
            filament::math::float3 n = u * ct + v * st;
            filament::math::float3 rim = n * r;

            uint32_t iA = vBase + seg;
            uint32_t iB = vBase + (S + 1) + seg;
            positions[iA] = A + rim;
            normals[iA] = n;
            positions[iB] = B + rim;
            normals[iB] = n;
        }

        // Tube indices
        uint32_t idx = iBase;
        for (uint32_t seg = 0; seg < S; ++seg) {
            uint32_t a0 = vBase + seg;
            uint32_t a1 = vBase + seg + 1;
            uint32_t b0 = vBase + (S + 1) + seg;
            uint32_t b1 = vBase + (S + 1) + seg + 1;
            allIndices[idx++] = a0;
            allIndices[idx++] = b0;
            allIndices[idx++] = a1;
            allIndices[idx++] = a1;
            allIndices[idx++] = b0;
            allIndices[idx++] = b1;
        }

        // Caps
        if (addCaps) {
            uint32_t capBase = vBase + tubeVerts;
            // Cap A (center + rim)
            positions[capBase] = A;
            normals[capBase] = -axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                float theta = 2.0f * pi * static_cast<float>(seg) / S;
                positions[capBase + 1 + seg] = A
                    + (u * std::cos(theta) + v * std::sin(theta)) * r;
                normals[capBase + 1 + seg] = -axis;
            }
            // Cap A indices (winding order: center, seg+1, seg for correct normal)
            for (uint32_t seg = 0; seg < S; ++seg) {
                allIndices[idx++] = capBase;
                allIndices[idx++] = capBase + 1 + ((seg + 1) % S);
                allIndices[idx++] = capBase + 1 + seg;
            }
            // Cap B
            uint32_t capBBase = capBase + S + 1;
            positions[capBBase] = B;
            normals[capBBase] = axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                float theta = 2.0f * pi * static_cast<float>(seg) / S;
                positions[capBBase + 1 + seg] = B
                    + (u * std::cos(theta) + v * std::sin(theta)) * r;
                normals[capBBase + 1 + seg] = axis;
            }
            // Cap B indices
            for (uint32_t seg = 0; seg < S; ++seg) {
                allIndices[idx++] = capBBase;
                allIndices[idx++] = capBBase + 1 + seg;
                allIndices[idx++] = capBBase + 1 + ((seg + 1) % S);
            }
        }
    }

    // Generate tangent frames
    filament::geometry::SurfaceOrientation *orientation =
        filament::geometry::SurfaceOrientation::Builder()
            .vertexCount(totalVerts)
            .normals(normals)
            .build();

    auto *tangents = new filament::math::short4[totalVerts];
    orientation->getQuats(tangents, totalVerts);
    delete orientation;

    // Build vertex buffer
    uint8_t bufIdx = 0;
    uint8_t posBuffer = bufIdx++;
    uint8_t tangentBuffer = bufIdx++;
    uint8_t colorBuffer = mHasColors ? bufIdx++ : 0;
    uint8_t bufferCount = bufIdx;

    auto vbBuilder = filament::VertexBuffer::Builder()
        .bufferCount(bufferCount)
        .vertexCount(totalVerts)
        .attribute(filament::VertexAttribute::POSITION, posBuffer,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        vbBuilder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }

    mVertexBuffer = vbBuilder.build(*engine);

    // Upload positions
    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            positions, totalVerts * sizeof(filament::math::float3),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::float3 *>(buf);
            }));

    // Upload tangents
    mVertexBuffer->setBufferAt(*engine, tangentBuffer,
        filament::VertexBuffer::BufferDescriptor(
            tangents, totalVerts * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));

    // Upload per-cylinder colors (expand to per-vertex)
    if (mHasColors) {
        auto *colors = new filament::math::float4[totalVerts];
        const filament::math::float4 *srcColors =
            static_cast<const filament::math::float4 *>(colArray->data());
        for (uint32_t c = 0; c < numCylinders; ++c) {
            filament::math::float4 cA = srcColors[c * 2 + 0];
            filament::math::float4 cB = srcColors[c * 2 + 1];
            uint32_t vBase = c * vertsPerCyl;
            // Tube: first ring gets colorA, second ring gets colorB
            for (uint32_t seg = 0; seg <= S; ++seg) {
                colors[vBase + seg] = cA;
                colors[vBase + (S + 1) + seg] = cB;
            }
            // Caps: cap A gets colorA, cap B gets colorB
            if (addCaps) {
                uint32_t capBase = vBase + tubeVerts;
                for (uint32_t i = 0; i <= S; ++i)
                    colors[capBase + i] = cA;
                uint32_t capBBase = capBase + S + 1;
                for (uint32_t i = 0; i <= S; ++i)
                    colors[capBBase + i] = cB;
            }
        }
        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                colors, totalVerts * sizeof(filament::math::float4),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float4 *>(buf);
                }));
    }

    delete[] normals;

    // Compute AABB
    mAabb = computeAabb(positions, totalVerts);

    mIndexCount = totalIndices;
    mIndexBuffer = filament::IndexBuffer::Builder()
        .indexCount(totalIndices)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mIndexBuffer->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            allIndices, totalIndices * sizeof(uint32_t),
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint32_t *>(buf);
            }));

    markCommitted();
}

void Geometry::commitQuad()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *norArray =
        getParamObject<helium::Array1D>("vertex.normal");
    helium::Array1D *attr0Array =
        getParamObject<helium::Array1D>("vertex.attribute0");
    helium::Array1D *attr1Array =
        getParamObject<helium::Array1D>("vertex.attribute1");
    helium::Array1D *primColArray =
        getParamObject<helium::Array1D>("primitive.color");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "quad geometry requires 'vertex.position'");
        return;
    }

    if (mVertexBuffer) {
        engine->destroy(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        engine->destroy(mIndexBuffer);
        mIndexBuffer = nullptr;
    }

    // Each group of 4 consecutive vertices forms a quad.
    uint32_t numSrcVertices = static_cast<uint32_t>(posArray->totalSize());
    uint32_t numQuads = numSrcVertices / 4;
    uint32_t numVertices = numSrcVertices;
    uint32_t numTriangles = numQuads * 2;

    mHasColors = colArray != nullptr || primColArray != nullptr;
    mHasUV0 = attr0Array != nullptr;
    mHasUV1 = attr1Array != nullptr;

    const filament::math::float3 *posData =
        static_cast<const filament::math::float3 *>(posArray->data());

    // Generate triangle indices from quads: (0,1,2), (0,2,3) for each quad
    Corrade::Containers::Array<uint32_t> indices{
        Corrade::NoInit, numTriangles * 3};
    for (uint32_t q = 0; q < numQuads; ++q) {
        uint32_t base = q * 4;
        uint32_t idx = q * 6;
        indices[idx + 0] = base + 0;
        indices[idx + 1] = base + 1;
        indices[idx + 2] = base + 2;
        indices[idx + 3] = base + 0;
        indices[idx + 4] = base + 2;
        indices[idx + 5] = base + 3;
    }

    mIndexCount = numTriangles * 3;
    const uint32_t *indexData = indices.data();

    // Handle primitive.color expansion
    Corrade::Containers::Array<filament::math::float3> expandedPos;
    Corrade::Containers::Array<filament::math::float4> expandedColors;
    Corrade::Containers::Array<uint32_t> expandedIndices;

    if (primColArray) {
        uint32_t expVerts = numTriangles * 3;
        expandedPos = Corrade::Containers::Array<filament::math::float3>{
            Corrade::NoInit, expVerts};
        expandedColors = Corrade::Containers::Array<filament::math::float4>{
            Corrade::NoInit, expVerts};
        expandedIndices = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, expVerts};

        Corrade::Containers::Array<filament::math::float4> primColConverted{
            Corrade::NoInit, numQuads};
        convertColors(primColConverted.data(), primColArray->data(),
            primColArray->elementType(), numQuads);

        for (uint32_t t = 0; t < numTriangles; ++t) {
            uint32_t i0 = indexData[t * 3 + 0];
            uint32_t i1 = indexData[t * 3 + 1];
            uint32_t i2 = indexData[t * 3 + 2];
            uint32_t base = t * 3;

            expandedPos[base + 0] = posData[i0];
            expandedPos[base + 1] = posData[i1];
            expandedPos[base + 2] = posData[i2];

            filament::math::float4 primColor = primColConverted[t / 2];
            expandedColors[base + 0] = primColor;
            expandedColors[base + 1] = primColor;
            expandedColors[base + 2] = primColor;

            expandedIndices[base + 0] = base + 0;
            expandedIndices[base + 1] = base + 1;
            expandedIndices[base + 2] = base + 2;
        }

        numVertices = expVerts;
        posData = expandedPos.data();
        indexData = expandedIndices.data();
    }

    // Generate tangent frames
    const filament::math::uint3 *triData =
        reinterpret_cast<const filament::math::uint3 *>(indexData);

    filament::geometry::SurfaceOrientation::Builder orientBuilder;
    orientBuilder.vertexCount(numVertices)
        .positions(posData)
        .triangleCount(numTriangles)
        .triangles(triData);

    if (norArray && !primColArray) {
        orientBuilder.normals(
            static_cast<const filament::math::float3 *>(norArray->data()));
    }

    filament::geometry::SurfaceOrientation *orientation =
        orientBuilder.build();

    Corrade::Containers::Array<filament::math::short4> tangents{
        Corrade::NoInit, numVertices};
    orientation->getQuats(tangents.data(), numVertices);
    delete orientation;

    // Build vertex buffer
    uint8_t bufIdx = 0;
    uint8_t posBuffer = bufIdx++;
    uint8_t tangentBuffer = bufIdx++;
    uint8_t colorBuffer = mHasColors ? bufIdx++ : 0;
    uint8_t uv0Buffer = mHasUV0 ? bufIdx++ : 0;
    uint8_t uv1Buffer = mHasUV1 ? bufIdx++ : 0;
    uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(numVertices)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        builder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }
    if (mHasUV0) {
        builder.attribute(filament::VertexAttribute::UV0, uv0Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);
    }
    if (mHasUV1) {
        builder.attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);
    }

    mVertexBuffer = builder.build(*engine);

    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            posData, numVertices * sizeof(filament::math::float3)));

    auto *tangentOwned = new filament::math::short4[numVertices];
    std::memcpy(tangentOwned, tangents.data(),
        numVertices * sizeof(filament::math::short4));
    mVertexBuffer->setBufferAt(*engine, tangentBuffer,
        filament::VertexBuffer::BufferDescriptor(
            tangentOwned,
            numVertices * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));

    if (mHasColors) {
        if (primColArray) {
            auto *colorData = new filament::math::float4[numVertices];
            std::memcpy(colorData, expandedColors.data(),
                numVertices * sizeof(filament::math::float4));
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colorData,
                    numVertices * sizeof(filament::math::float4),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float4 *>(buf);
                    }));
        } else if (colArray->elementType() == ANARI_FLOAT32_VEC4) {
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colArray->data(),
                    colArray->totalSize() * sizeof(float) * 4));
        } else {
            auto *colorData = new filament::math::float4[numVertices];
            convertColors(colorData, colArray->data(),
                colArray->elementType(), numVertices);
            mVertexBuffer->setBufferAt(*engine, colorBuffer,
                filament::VertexBuffer::BufferDescriptor(
                    colorData,
                    numVertices * sizeof(filament::math::float4),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float4 *>(buf);
                    }));
        }
    }

    if (mHasUV0) {
        ANARIDataType a0Type = attr0Array->elementType();
        if (a0Type == ANARI_FLOAT32_VEC2) {
            mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    attr0Array->data(),
                    attr0Array->totalSize() * sizeof(float) * 2));
        } else if (a0Type == ANARI_FLOAT32) {
            auto *uv0Data = new filament::math::float2[numVertices];
            const float *src =
                static_cast<const float *>(attr0Array->data());
            for (uint32_t i = 0; i < numVertices; ++i)
                uv0Data[i] = {src[i], 0.0f};
            mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    uv0Data, numVertices * sizeof(filament::math::float2),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float2 *>(buf);
                    }));
        }
    }

    if (mHasUV1) {
        ANARIDataType a1Type = attr1Array->elementType();
        if (a1Type == ANARI_FLOAT32_VEC2) {
            mVertexBuffer->setBufferAt(*engine, uv1Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    attr1Array->data(),
                    attr1Array->totalSize() * sizeof(float) * 2));
        } else if (a1Type == ANARI_FLOAT32) {
            auto *uv1Data = new filament::math::float2[numVertices];
            const float *src =
                static_cast<const float *>(attr1Array->data());
            for (uint32_t i = 0; i < numVertices; ++i)
                uv1Data[i] = {src[i], 0.0f};
            mVertexBuffer->setBufferAt(*engine, uv1Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    uv1Data, numVertices * sizeof(filament::math::float2),
                    [](void *buf, size_t, void *) {
                        delete[] static_cast<filament::math::float2 *>(buf);
                    }));
        }
    }

    mAabb = computeAabb(posData, numVertices);

    mIndexBuffer = filament::IndexBuffer::Builder()
        .indexCount(mIndexCount)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mIndexBuffer->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            indexData, mIndexCount * sizeof(uint32_t)));

    markCommitted();
}

void Geometry::commitCone()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *radiusArray =
        getParamObject<helium::Array1D>("vertex.radius");
    helium::Array1D *capArray =
        getParamObject<helium::Array1D>("vertex.cap");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "cone geometry requires 'vertex.position'");
        return;
    }

    if (mVertexBuffer) {
        engine->destroy(mVertexBuffer);
        mVertexBuffer = nullptr;
    }
    if (mIndexBuffer) {
        engine->destroy(mIndexBuffer);
        mIndexBuffer = nullptr;
    }

    float globalRadius = getParam<float>("radius", 1.0f);
    Corrade::Containers::String capsStr = getParamString("caps", "none");

    uint32_t numEndpoints = static_cast<uint32_t>(posArray->totalSize());
    uint32_t numCones = numEndpoints / 2;
    const filament::math::float3 *endpoints =
        static_cast<const filament::math::float3 *>(posArray->data());
    const float *radii = radiusArray
        ? static_cast<const float *>(radiusArray->data())
        : nullptr;
    const uint8_t *caps = capArray
        ? static_cast<const uint8_t *>(capArray->data())
        : nullptr;

    mHasColors = colArray != nullptr;
    mHasUV0 = false;
    mHasUV1 = false;

    bool globalCaps = (capsStr != "none"_s);
    constexpr uint32_t S = CYLINDER_SEGMENTS;
    constexpr float pi = 3.14159265358979323846f;

    // Per cone: tube same as cylinder, plus optional caps at each end
    uint32_t tubeVerts = (S + 1) * 2;
    uint32_t tubeIndices = S * 6;
    uint32_t maxCapVerts = (S + 1) * 2;
    uint32_t maxCapIndices = S * 3 * 2;
    uint32_t maxVertsPerCone = tubeVerts + maxCapVerts;
    uint32_t maxIndicesPerCone = tubeIndices + maxCapIndices;

    // Allocate for worst case, then trim
    Corrade::Containers::Array<filament::math::float3> allPositions{
        Corrade::NoInit, numCones * maxVertsPerCone};
    Corrade::Containers::Array<filament::math::float3> allNormals{
        Corrade::NoInit, numCones * maxVertsPerCone};
    Corrade::Containers::Array<uint32_t> allIndices{
        Corrade::NoInit, numCones * maxIndicesPerCone};

    uint32_t totalVerts = 0;
    uint32_t totalIndices = 0;

    for (uint32_t c = 0; c < numCones; ++c) {
        filament::math::float3 A = endpoints[c * 2 + 0];
        filament::math::float3 B = endpoints[c * 2 + 1];
        float rA = radii ? radii[c * 2 + 0] : globalRadius;
        float rB = radii ? radii[c * 2 + 1] : globalRadius;

        bool capA = globalCaps || (caps && (caps[c * 2 + 0] != 0));
        bool capB = globalCaps || (caps && (caps[c * 2 + 1] != 0));

        filament::math::float3 axis = B - A;
        float len = length(axis);
        if (len < 1e-12f)
            axis = {0.0f, 1.0f, 0.0f};
        else
            axis /= len;

        filament::math::float3 u, v;
        buildFrame(axis, u, v);

        // Compute normal slope for cone surface
        float dr = rA - rB;
        float slope = (len > 1e-12f) ? dr / len : 0.0f;

        uint32_t vBase = totalVerts;
        uint32_t iBase = totalIndices;

        // Generate tube vertices
        for (uint32_t seg = 0; seg <= S; ++seg) {
            float theta = 2.0f * pi * static_cast<float>(seg) / S;
            float ct = std::cos(theta);
            float st = std::sin(theta);
            filament::math::float3 circleDir = u * ct + v * st;

            // Normal: perpendicular to surface, accounting for cone slope
            filament::math::float3 n = normalize(circleDir + axis * slope);

            allPositions[vBase + seg] = A + circleDir * rA;
            allNormals[vBase + seg] = n;

            allPositions[vBase + (S + 1) + seg] = B + circleDir * rB;
            allNormals[vBase + (S + 1) + seg] = n;
        }

        // Tube indices
        uint32_t idx = iBase;
        for (uint32_t seg = 0; seg < S; ++seg) {
            uint32_t a0 = vBase + seg;
            uint32_t a1 = vBase + seg + 1;
            uint32_t b0 = vBase + (S + 1) + seg;
            uint32_t b1 = vBase + (S + 1) + seg + 1;
            allIndices[idx++] = a0;
            allIndices[idx++] = b0;
            allIndices[idx++] = a1;
            allIndices[idx++] = a1;
            allIndices[idx++] = b0;
            allIndices[idx++] = b1;
        }

        totalVerts += tubeVerts;
        totalIndices += tubeIndices;

        // Cap A
        if (capA && rA > 1e-12f) {
            uint32_t capBase = totalVerts;
            allPositions[capBase] = A;
            allNormals[capBase] = -axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                float theta = 2.0f * pi * static_cast<float>(seg) / S;
                allPositions[capBase + 1 + seg] = A
                    + (u * std::cos(theta) + v * std::sin(theta)) * rA;
                allNormals[capBase + 1 + seg] = -axis;
            }
            for (uint32_t seg = 0; seg < S; ++seg) {
                allIndices[totalIndices++] = capBase;
                allIndices[totalIndices++] = capBase + 1 + ((seg + 1) % S);
                allIndices[totalIndices++] = capBase + 1 + seg;
            }
            totalVerts += S + 1;
        }

        // Cap B
        if (capB && rB > 1e-12f) {
            uint32_t capBase = totalVerts;
            allPositions[capBase] = B;
            allNormals[capBase] = axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                float theta = 2.0f * pi * static_cast<float>(seg) / S;
                allPositions[capBase + 1 + seg] = B
                    + (u * std::cos(theta) + v * std::sin(theta)) * rB;
                allNormals[capBase + 1 + seg] = axis;
            }
            for (uint32_t seg = 0; seg < S; ++seg) {
                allIndices[totalIndices++] = capBase;
                allIndices[totalIndices++] = capBase + 1 + seg;
                allIndices[totalIndices++] = capBase + 1 + ((seg + 1) % S);
            }
            totalVerts += S + 1;
        }
    }

    // Generate tangent frames
    filament::geometry::SurfaceOrientation *orientation =
        filament::geometry::SurfaceOrientation::Builder()
            .vertexCount(totalVerts)
            .normals(allNormals.data())
            .build();

    auto *tangents = new filament::math::short4[totalVerts];
    orientation->getQuats(tangents, totalVerts);
    delete orientation;

    // Build vertex buffer
    uint8_t bufIdx = 0;
    uint8_t posBuffer = bufIdx++;
    uint8_t tangentBuffer = bufIdx++;
    uint8_t colorBuffer = mHasColors ? bufIdx++ : 0;
    uint8_t bufferCount = bufIdx;

    auto vbBuilder = filament::VertexBuffer::Builder()
        .bufferCount(bufferCount)
        .vertexCount(totalVerts)
        .attribute(filament::VertexAttribute::POSITION, posBuffer,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        vbBuilder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }

    mVertexBuffer = vbBuilder.build(*engine);

    // Upload positions (copy since allPositions may be oversized)
    auto *posOwned = new filament::math::float3[totalVerts];
    std::memcpy(posOwned, allPositions.data(),
        totalVerts * sizeof(filament::math::float3));
    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            posOwned, totalVerts * sizeof(filament::math::float3),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::float3 *>(buf);
            }));

    mVertexBuffer->setBufferAt(*engine, tangentBuffer,
        filament::VertexBuffer::BufferDescriptor(
            tangents, totalVerts * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));

    if (mHasColors) {
        // Expand per-endpoint colors to per-vertex
        auto *colors = new filament::math::float4[totalVerts];
        const filament::math::float4 *srcColors =
            static_cast<const filament::math::float4 *>(colArray->data());

        uint32_t vOff = 0;
        for (uint32_t c = 0; c < numCones; ++c) {
            filament::math::float4 cA = srcColors[c * 2 + 0];
            filament::math::float4 cB = srcColors[c * 2 + 1];
            float rA = radii ? radii[c * 2 + 0] : globalRadius;
            float rB = radii ? radii[c * 2 + 1] : globalRadius;
            bool capA = globalCaps || (caps && (caps[c * 2 + 0] != 0));
            bool capB = globalCaps || (caps && (caps[c * 2 + 1] != 0));

            // Tube: ring A gets colorA, ring B gets colorB
            for (uint32_t seg = 0; seg <= S; ++seg)
                colors[vOff + seg] = cA;
            for (uint32_t seg = 0; seg <= S; ++seg)
                colors[vOff + (S + 1) + seg] = cB;
            vOff += tubeVerts;

            if (capA && rA > 1e-12f) {
                for (uint32_t i = 0; i <= S; ++i)
                    colors[vOff + i] = cA;
                vOff += S + 1;
            }
            if (capB && rB > 1e-12f) {
                for (uint32_t i = 0; i <= S; ++i)
                    colors[vOff + i] = cB;
                vOff += S + 1;
            }
        }

        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                colors, totalVerts * sizeof(filament::math::float4),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float4 *>(buf);
                }));
    }

    mAabb = computeAabb(allPositions.data(), totalVerts);

    mIndexCount = totalIndices;
    auto *idxOwned = new uint32_t[totalIndices];
    std::memcpy(idxOwned, allIndices.data(), totalIndices * sizeof(uint32_t));
    mIndexBuffer = filament::IndexBuffer::Builder()
        .indexCount(totalIndices)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mIndexBuffer->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            idxOwned, totalIndices * sizeof(uint32_t),
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint32_t *>(buf);
            }));

    markCommitted();
}

}
