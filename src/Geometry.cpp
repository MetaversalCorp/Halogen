// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Geometry.h"

#include "Aabb.h"

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
    mHasColors = colArray != nullptr;
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
    const filament::math::uint3 *triData =
        reinterpret_cast<const filament::math::uint3 *>(indexData);

    // Generate tangent frames using SurfaceOrientation
    filament::geometry::SurfaceOrientation::Builder orientBuilder;
    orientBuilder.vertexCount(numVertices)
        .positions(posData)
        .triangleCount(numTriangles)
        .triangles(triData);

    if (norArray) {
        orientBuilder.normals(
            static_cast<const filament::math::float3 *>(norArray->data()));
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
        uint32_t colSize = static_cast<uint32_t>(colArray->totalSize())
            * sizeof(float) * 4;
        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                colArray->data(), colSize));
    }

    if (mHasUV0) {
        ANARIDataType a0Type = attr0Array->elementType();
        if (a0Type == ANARI_FLOAT32_VEC2) {
            mVertexBuffer->setBufferAt(*engine, uv0Buffer,
                filament::VertexBuffer::BufferDescriptor(
                    attr0Array->data(),
                    attr0Array->totalSize() * sizeof(float) * 2));
        } else if (a0Type == ANARI_FLOAT32) {
            // Scalar float → pack as float2 (value, 0)
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

}
