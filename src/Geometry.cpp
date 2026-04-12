// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Geometry.h"

#include "Aabb.h"
#include "ColorConversion.h"
#include "Constants.h"

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
    else if (mSubtype == "curve"_s)
        commitCurve();
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

    uint32_t numVertices = uint32_t(posArray->totalSize());
    mHasColors = colArray != nullptr || primColArray != nullptr;
    mHasUV0 = attr0Array != nullptr;
    mHasUV1 = attr1Array != nullptr;

    // Generate indices if not provided
    uint32_t numTriangles = 0;
    Corrade::Containers::Array<uint32_t> generatedIndices;
    const uint32_t *indexData = nullptr;

    if (idxArray) {
        numTriangles = uint32_t(idxArray->totalSize());
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
        const uint32_t expVerts = numTriangles * 3;
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

        const ANARIDataType primColType = primColArray->elementType();
        const void *primColData = primColArray->data();

        Corrade::Containers::Array<filament::math::float4> primColConverted{
            Corrade::NoInit, numTriangles};
        convertColors(primColConverted.data(), primColData, primColType,
            numTriangles);

        for (uint32_t t = 0; t < numTriangles; ++t) {
            const uint32_t i0 = indexData[t * 3 + 0];
            const uint32_t i1 = indexData[t * 3 + 1];
            const uint32_t i2 = indexData[t * 3 + 2];
            const uint32_t base = t * 3;

            expandedPos[base + 0] = posData[i0];
            expandedPos[base + 1] = posData[i1];
            expandedPos[base + 2] = posData[i2];

            if (norData) {
                expandedNor[base + 0] = norData[i0];
                expandedNor[base + 1] = norData[i1];
                expandedNor[base + 2] = norData[i2];
            }

            if (attr0Array) {
                const ANARIDataType a0Type = attr0Array->elementType();
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
                const ANARIDataType a1Type = attr1Array->elementType();
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

            const filament::math::float4 primColor = primColConverted[t];
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

    // Count buffers: POSITION + TANGENTS + COLOR + UV0 + UV1
    uint8_t bufIdx = 0;
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(numVertices)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS)
            .attribute(filament::VertexAttribute::COLOR, colorBuffer,
                filament::VertexBuffer::AttributeType::FLOAT4)
            .attribute(filament::VertexAttribute::UV0, uv0Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2)
            .attribute(filament::VertexAttribute::UV1, uv1Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2);

    mVertexBuffer = builder.build(*engine);

    // Upload position data
    const size_t posSize = numVertices * sizeof(float) * 3;
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
            const ANARIDataType a0Type = attr0Array->elementType();
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
            const ANARIDataType a1Type = attr1Array->elementType();
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

    fillDefaultAttributes(engine, numVertices, colorBuffer, uv0Buffer,
        uv1Buffer);

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
    const uint32_t vertCount = (SPHERE_RINGS + 1) * (SPHERE_SEGMENTS + 1);
    const uint32_t idxCount = SPHERE_RINGS * SPHERE_SEGMENTS * 6;

    verts = Corrade::Containers::Array<SphereVertex>{
        Corrade::NoInit, vertCount};
    indices = Corrade::Containers::Array<uint32_t>{
        Corrade::NoInit, idxCount};

    uint32_t v = 0;
    for (uint32_t ring = 0; ring <= SPHERE_RINGS; ++ring) {
        const float phi = AnariFilament::Pi * float(ring) / SPHERE_RINGS;
        const float sinPhi = std::sin(phi);
        const float cosPhi = std::cos(phi);

        for (uint32_t seg = 0; seg <= SPHERE_SEGMENTS; ++seg) {
            const float theta = 2.0f * AnariFilament::Pi * float(seg) / SPHERE_SEGMENTS;
            const float sinTheta = std::sin(theta);
            const float cosTheta = std::cos(theta);

            const filament::math::float3 n = {
                sinPhi * cosTheta, cosPhi, sinPhi * sinTheta};
            verts[v++] = {n, n};
        }
    }

    uint32_t idx = 0;
    for (uint32_t ring = 0; ring < SPHERE_RINGS; ++ring) {
        for (uint32_t seg = 0; seg < SPHERE_SEGMENTS; ++seg) {
            const uint32_t a = ring * (SPHERE_SEGMENTS + 1) + seg;
            const uint32_t b = a + SPHERE_SEGMENTS + 1;

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

    const float globalRadius = getParam<float>("radius", 0.01f);
    const uint32_t numSpheres = uint32_t(posArray->totalSize());
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

    const uint32_t vertsPerSphere = uint32_t(unitVerts.size());
    const uint32_t indicesPerSphere = uint32_t(unitIndices.size());
    const uint32_t totalVerts = numSpheres * vertsPerSphere;
    const uint32_t totalIndices = numSpheres * indicesPerSphere;

    // Build position and normal arrays for all spheres
    auto *positions = new filament::math::float3[totalVerts];
    auto *normals = new filament::math::float3[totalVerts];
    auto *allIndices = new uint32_t[totalIndices];

    for (uint32_t s = 0; s < numSpheres; ++s) {
        const float r = radii ? radii[s] : globalRadius;
        const filament::math::float3 c = centers[s];
        const uint32_t vBase = s * vertsPerSphere;
        const uint32_t iBase = s * indicesPerSphere;

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
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(totalVerts)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS)
            .attribute(filament::VertexAttribute::COLOR, colorBuffer,
                filament::VertexBuffer::AttributeType::FLOAT4)
            .attribute(filament::VertexAttribute::UV0, uv0Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2)
            .attribute(filament::VertexAttribute::UV1, uv1Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2);

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

    fillDefaultAttributes(engine, totalVerts, colorBuffer, uv0Buffer,
        uv1Buffer);

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

    const float globalRadius = getParam<float>("radius", 1.0f);
    const Corrade::Containers::String capsStr = getParamString("caps", "none");

    const uint32_t numEndpoints = uint32_t(posArray->totalSize());
    const uint32_t numCylinders = numEndpoints / 2;
    const filament::math::float3 *endpoints =
        static_cast<const filament::math::float3 *>(posArray->data());
    const float *primRadii = primRadiusArray
        ? static_cast<const float *>(primRadiusArray->data())
        : nullptr;

    mHasColors = colArray != nullptr;
    mHasUV0 = false;
    mHasUV1 = false;

    const bool addCaps = (capsStr != "none"_s);
    constexpr uint32_t S = CYLINDER_SEGMENTS;

    // Per cylinder:
    //   Tube: (S + 1) * 2 vertices, S * 2 triangles (6 indices)
    //   Caps: S + 1 vertices per cap (center + S rim), S triangles per cap
    const uint32_t tubeVerts = (S + 1) * 2;
    const uint32_t tubeIndices = S * 6;
    const uint32_t capVerts = addCaps ? (S + 1) * 2 : 0;
    const uint32_t capIndices = addCaps ? S * 3 * 2 : 0;
    const uint32_t vertsPerCyl = tubeVerts + capVerts;
    const uint32_t indicesPerCyl = tubeIndices + capIndices;

    const uint32_t totalVerts = numCylinders * vertsPerCyl;
    const uint32_t totalIndices = numCylinders * indicesPerCyl;

    auto *positions = new filament::math::float3[totalVerts];
    auto *normals = new filament::math::float3[totalVerts];
    auto *allIndices = new uint32_t[totalIndices];

    for (uint32_t c = 0; c < numCylinders; ++c) {
        const filament::math::float3 A = endpoints[c * 2 + 0];
        const filament::math::float3 B = endpoints[c * 2 + 1];
        const float r = primRadii ? primRadii[c] : globalRadius;

        filament::math::float3 axis = B - A;
        const float len = length(axis);
        if (len < 1e-12f)
            axis = {0.0f, 1.0f, 0.0f};
        else
            axis /= len;

        filament::math::float3 u, v;
        buildFrame(axis, u, v);

        const uint32_t vBase = c * vertsPerCyl;
        const uint32_t iBase = c * indicesPerCyl;

        // Generate tube vertices: ring at A, then ring at B
        for (uint32_t seg = 0; seg <= S; ++seg) {
            const float theta = 2.0f * Pi * float(seg) / S;
            const float ct = std::cos(theta);
            const float st = std::sin(theta);
            const filament::math::float3 n = u * ct + v * st;
            const filament::math::float3 rim = n * r;

            const uint32_t iA = vBase + seg;
            const uint32_t iB = vBase + (S + 1) + seg;
            positions[iA] = A + rim;
            normals[iA] = n;
            positions[iB] = B + rim;
            normals[iB] = n;
        }

        // Tube indices
        uint32_t idx = iBase;
        for (uint32_t seg = 0; seg < S; ++seg) {
            const uint32_t a0 = vBase + seg;
            const uint32_t a1 = vBase + seg + 1;
            const uint32_t b0 = vBase + (S + 1) + seg;
            const uint32_t b1 = vBase + (S + 1) + seg + 1;
            allIndices[idx++] = a0;
            allIndices[idx++] = a1;
            allIndices[idx++] = b0;
            allIndices[idx++] = a1;
            allIndices[idx++] = b1;
            allIndices[idx++] = b0;
        }

        // Caps
        if (addCaps) {
            const uint32_t capBase = vBase + tubeVerts;
            // Cap A (center + rim)
            positions[capBase] = A;
            normals[capBase] = -axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                const float theta = 2.0f * Pi * float(seg) / S;
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
            const uint32_t capBBase = capBase + S + 1;
            positions[capBBase] = B;
            normals[capBBase] = axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                const float theta = 2.0f * Pi * float(seg) / S;
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
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    auto vbBuilder = filament::VertexBuffer::Builder()
        .bufferCount(bufferCount)
        .vertexCount(totalVerts)
        .attribute(filament::VertexAttribute::POSITION, posBuffer,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS)
        .attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4)
        .attribute(filament::VertexAttribute::UV0, uv0Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2)
        .attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);

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
            const filament::math::float4 cA = srcColors[c * 2 + 0];
            const filament::math::float4 cB = srcColors[c * 2 + 1];
            const uint32_t vBase = c * vertsPerCyl;
            // Tube: first ring gets colorA, second ring gets colorB
            for (uint32_t seg = 0; seg <= S; ++seg) {
                colors[vBase + seg] = cA;
                colors[vBase + (S + 1) + seg] = cB;
            }
            // Caps: cap A gets colorA, cap B gets colorB
            if (addCaps) {
                const uint32_t capBase = vBase + tubeVerts;
                for (uint32_t i = 0; i <= S; ++i)
                    colors[capBase + i] = cA;
                const uint32_t capBBase = capBase + S + 1;
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

    fillDefaultAttributes(engine, totalVerts, colorBuffer, uv0Buffer,
        uv1Buffer);

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

// -- Curve tessellation --

void Geometry::commitCurve()
{
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *vertRadiusArray =
        getParamObject<helium::Array1D>("vertex.radius");
    helium::Array1D *idxArray =
        getParamObject<helium::Array1D>("primitive.index");

    if (!posArray) {
        reportMessage(ANARI_SEVERITY_ERROR,
            "curve geometry requires 'vertex.position'");
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

    const float globalRadius = getParam<float>("radius", 1.0f);

    const uint32_t numPositions = uint32_t(posArray->totalSize());
    const filament::math::float3 *srcPositions =
        static_cast<const filament::math::float3 *>(posArray->data());
    const float *vertRadii = vertRadiusArray
        ? static_cast<const float *>(vertRadiusArray->data())
        : nullptr;

    mHasColors = colArray != nullptr;
    mHasUV0 = false;
    mHasUV1 = false;

    // Build segment list from primitive.index or sequential pairs
    Corrade::Containers::Array<uint32_t> segments;
    if (idxArray) {
        const uint32_t numPrims = uint32_t(idxArray->totalSize());
        segments = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, numPrims};
        std::memcpy(segments.data(), idxArray->data(),
            numPrims * sizeof(uint32_t));
    } else {
        if (numPositions < 2)
            return;
        segments = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, numPositions - 1};
        for (uint32_t i = 0; i + 1 < numPositions; ++i)
            segments[i] = i;
    }

    const uint32_t numSegments = uint32_t(segments.size());
    constexpr uint32_t S = CYLINDER_SEGMENTS;
    constexpr uint32_t SUBDIVISIONS = 4;

    // Group consecutive segments into chains for smooth, connected tubes.
    // First pass: count chains and their sizes.
    uint32_t numChains = 0;
    for (uint32_t s = 0; s < numSegments; ++s) {
        const uint32_t i0 = segments[s];
        const uint32_t i1 = i0 + 1;
        if (i1 >= numPositions)
            continue;
        if (numChains == 0 || segments[s - 1] + 1 != i0)
            ++numChains;
    }

    // Build flat control-point array + per-chain offset/length.
    Corrade::Containers::Array<uint32_t> chainOffset{
        Corrade::NoInit, numChains};
    Corrade::Containers::Array<uint32_t> chainLen{
        Corrade::NoInit, numChains};

    // Second pass: record chain boundaries.
    {
        uint32_t ci = 0;
        uint32_t totalCp = 0;
        for (uint32_t s = 0; s < numSegments; ++s) {
            const uint32_t i0 = segments[s];
            const uint32_t i1 = i0 + 1;
            if (i1 >= numPositions)
                continue;
            if (ci == 0 || segments[s - 1] + 1 != i0) {
                if (ci > 0)
                    chainLen[ci - 1] = totalCp;
                chainOffset[ci] = s;
                totalCp = 2;
                ++ci;
            } else {
                ++totalCp;
            }
        }
        if (ci > 0)
            chainLen[ci - 1] = totalCp;
    }

    // Count total geometry across all chains.
    // Each chain with N control points produces (N-1)*SUBDIVISIONS+1 rings.
    uint32_t totalVerts = 0;
    uint32_t totalIndices = 0;
    for (uint32_t c = 0; c < numChains; ++c) {
        const uint32_t nSegs = chainLen[c] - 1;
        const uint32_t nRings = nSegs * SUBDIVISIONS + 1;
        totalVerts += nRings * (S + 1);
        totalIndices += (nRings - 1) * S * 6;
    }

    if (totalVerts == 0) {
        markCommitted();
        return;
    }

    auto *positions = new filament::math::float3[totalVerts];
    auto *normals = new filament::math::float3[totalVerts];
    auto *allIndices = new uint32_t[totalIndices];

    // Per-ring color interpolation data (control-point pair + blend factor)
    const uint32_t totalRings = totalVerts / (S + 1);
    Corrade::Containers::Array<uint32_t> ringCp0{
        Corrade::NoInit, totalRings};
    Corrade::Containers::Array<uint32_t> ringCp1{
        Corrade::NoInit, totalRings};
    Corrade::Containers::Array<float> ringBlend{
        Corrade::NoInit, totalRings};

    // Catmull-Rom position evaluation
    auto crPos = [](filament::math::float3 p0, filament::math::float3 p1,
        filament::math::float3 p2, filament::math::float3 p3,
        float t) -> filament::math::float3 {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1)
            + (-p0 + p2) * t
            + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
            + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    // Catmull-Rom tangent (first derivative)
    auto crTan = [](filament::math::float3 p0, filament::math::float3 p1,
        filament::math::float3 p2, filament::math::float3 p3,
        float t) -> filament::math::float3 {
        const float t2 = t * t;
        return 0.5f * ((-p0 + p2)
            + (4.0f * p0 - 10.0f * p1 + 8.0f * p2 - 2.0f * p3) * t
            + (-3.0f * p0 + 9.0f * p1 - 9.0f * p2 + 3.0f * p3) * t2);
    };

    uint32_t vOff = 0;
    uint32_t iOff = 0;
    uint32_t rOff = 0;

    for (uint32_t c = 0; c < numChains; ++c) {
        const uint32_t nPts = chainLen[c];
        const uint32_t nOrigSegs = nPts - 1;
        const uint32_t nRings = nOrigSegs * SUBDIVISIONS + 1;

        // Build control-point index array for this chain
        const uint32_t firstSeg = chainOffset[c];
        Corrade::Containers::Array<uint32_t> cp{
            Corrade::NoInit, nPts};
        cp[0] = segments[firstSeg];
        for (uint32_t i = 0; i < nOrigSegs; ++i)
            cp[i + 1] = segments[firstSeg + i] + 1;

        // Compute ring centers, radii, and unit tangents via Catmull-Rom
        Corrade::Containers::Array<filament::math::float3> rc{
            Corrade::NoInit, nRings};
        Corrade::Containers::Array<float> rr{
            Corrade::NoInit, nRings};
        Corrade::Containers::Array<filament::math::float3> rt{
            Corrade::NoInit, nRings};

        for (uint32_t seg = 0; seg < nOrigSegs; ++seg) {
            const filament::math::float3 p1 = srcPositions[cp[seg]];
            const filament::math::float3 p2 = srcPositions[cp[seg + 1]];
            const filament::math::float3 p0 = (seg > 0)
                ? srcPositions[cp[seg - 1]]
                : (2.0f * p1 - p2);
            const filament::math::float3 p3 = (seg + 2 < nPts)
                ? srcPositions[cp[seg + 2]]
                : (2.0f * p2 - p1);

            const float r1 =
                vertRadii ? vertRadii[cp[seg]] : globalRadius;
            const float r2 =
                vertRadii ? vertRadii[cp[seg + 1]] : globalRadius;

            for (uint32_t sub = 0; sub < SUBDIVISIONS; ++sub) {
                const float t = float(sub) / SUBDIVISIONS;
                const uint32_t ri = seg * SUBDIVISIONS + sub;
                rc[ri] = crPos(p0, p1, p2, p3, t);
                rr[ri] = r1 + (r2 - r1) * t;
                filament::math::float3 tang = crTan(p0, p1, p2, p3, t);
                const float tLen = length(tang);
                rt[ri] = (tLen > 1e-12f) ? tang / tLen
                    : filament::math::float3{0, 1, 0};

                ringCp0[rOff + ri] = cp[seg];
                ringCp1[rOff + ri] = cp[seg + 1];
                ringBlend[rOff + ri] = t;
            }
        }

        // Last ring at the final control point
        {
            const uint32_t lastSeg = nOrigSegs - 1;
            const filament::math::float3 p1 = srcPositions[cp[lastSeg]];
            const filament::math::float3 p2 =
                srcPositions[cp[lastSeg + 1]];
            const filament::math::float3 p0 = (lastSeg > 0)
                ? srcPositions[cp[lastSeg - 1]]
                : (2.0f * p1 - p2);
            const filament::math::float3 p3 = (lastSeg + 2 < nPts)
                ? srcPositions[cp[lastSeg + 2]]
                : (2.0f * p2 - p1);

            rc[nRings - 1] = srcPositions[cp[nPts - 1]];
            rr[nRings - 1] = vertRadii
                ? vertRadii[cp[nPts - 1]] : globalRadius;
            filament::math::float3 tang = crTan(p0, p1, p2, p3, 1.0f);
            const float tLen = length(tang);
            rt[nRings - 1] = (tLen > 1e-12f) ? tang / tLen
                : filament::math::float3{0, 1, 0};

            ringCp0[rOff + nRings - 1] = cp[nPts - 1];
            ringCp1[rOff + nRings - 1] = cp[nPts - 1];
            ringBlend[rOff + nRings - 1] = 0.0f;
        }

        // Propagate frames along the chain via parallel transport
        Corrade::Containers::Array<filament::math::float3> ru{
            Corrade::NoInit, nRings};
        Corrade::Containers::Array<filament::math::float3> rv{
            Corrade::NoInit, nRings};
        buildFrame(rt[0], ru[0], rv[0]);

        for (uint32_t r = 1; r < nRings; ++r) {
            const filament::math::float3 crossT = cross(rt[r - 1], rt[r]);
            const float sinA = length(crossT);
            if (sinA > 1e-6f) {
                const filament::math::float3 ax = crossT / sinA;
                const float cosA = dot(rt[r - 1], rt[r]);
                ru[r] = normalize(
                    ru[r - 1] * cosA
                    + cross(ax, ru[r - 1]) * sinA
                    + ax * dot(ax, ru[r - 1]) * (1.0f - cosA));
                rv[r] = normalize(cross(rt[r], ru[r]));
            } else {
                ru[r] = ru[r - 1];
                rv[r] = rv[r - 1];
            }
        }

        // Generate ring vertices
        for (uint32_t r = 0; r < nRings; ++r) {
            for (uint32_t seg = 0; seg <= S; ++seg) {
                const float theta = 2.0f * Pi * float(seg) / S;
                const float ct = std::cos(theta);
                const float st = std::sin(theta);
                const filament::math::float3 dir = ru[r] * ct + rv[r] * st;
                const uint32_t vi = vOff + r * (S + 1) + seg;
                positions[vi] = rc[r] + dir * rr[r];
                normals[vi] = normalize(dir);
            }
        }

        // Connect adjacent rings with triangles
        for (uint32_t r = 0; r + 1 < nRings; ++r) {
            const uint32_t rA = vOff + r * (S + 1);
            const uint32_t rB = vOff + (r + 1) * (S + 1);
            for (uint32_t seg = 0; seg < S; ++seg) {
                allIndices[iOff++] = rA + seg;
                allIndices[iOff++] = rA + seg + 1;
                allIndices[iOff++] = rB + seg;
                allIndices[iOff++] = rA + seg + 1;
                allIndices[iOff++] = rB + seg + 1;
                allIndices[iOff++] = rB + seg;
            }
        }

        vOff += nRings * (S + 1);
        rOff += nRings;
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
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    mVertexBuffer = filament::VertexBuffer::Builder()
        .bufferCount(bufferCount)
        .vertexCount(totalVerts)
        .attribute(filament::VertexAttribute::POSITION, posBuffer,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS)
        .attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4)
        .attribute(filament::VertexAttribute::UV0, uv0Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2)
        .attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2)
        .build(*engine);

    mVertexBuffer->setBufferAt(*engine, posBuffer,
        filament::VertexBuffer::BufferDescriptor(
            positions, totalVerts * sizeof(filament::math::float3),
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
        auto *colors = new filament::math::float4[totalVerts];
        const filament::math::float4 *srcColors =
            static_cast<const filament::math::float4 *>(colArray->data());
        for (uint32_t r = 0; r < totalRings; ++r) {
            const filament::math::float4 cA =
                (ringCp0[r] < colArray->totalSize())
                    ? srcColors[ringCp0[r]]
                    : filament::math::float4{1, 1, 1, 1};
            const filament::math::float4 cB =
                (ringCp1[r] < colArray->totalSize())
                    ? srcColors[ringCp1[r]]
                    : filament::math::float4{1, 1, 1, 1};
            const filament::math::float4 interp =
                cA * (1.0f - ringBlend[r]) + cB * ringBlend[r];
            const uint32_t vBase = r * (S + 1);
            for (uint32_t seg = 0; seg <= S; ++seg)
                colors[vBase + seg] = interp;
        }
        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                colors, totalVerts * sizeof(filament::math::float4),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float4 *>(buf);
                }));
    }

    fillDefaultAttributes(engine, totalVerts, colorBuffer, uv0Buffer,
        uv1Buffer);

    delete[] normals;

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
    const uint32_t numSrcVertices = uint32_t(posArray->totalSize());
    const uint32_t numQuads = numSrcVertices / 4;
    uint32_t numVertices = numSrcVertices;
    const uint32_t numTriangles = numQuads * 2;

    mHasColors = colArray != nullptr || primColArray != nullptr;
    mHasUV0 = attr0Array != nullptr;
    mHasUV1 = attr1Array != nullptr;

    const filament::math::float3 *posData =
        static_cast<const filament::math::float3 *>(posArray->data());

    // Generate triangle indices from quads: (0,1,2), (0,2,3) for each quad
    Corrade::Containers::Array<uint32_t> indices{
        Corrade::NoInit, numTriangles * 3};
    for (uint32_t q = 0; q < numQuads; ++q) {
        const uint32_t base = q * 4;
        const uint32_t idx = q * 6;
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
        const uint32_t expVerts = numTriangles * 3;
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
            const uint32_t i0 = indexData[t * 3 + 0];
            const uint32_t i1 = indexData[t * 3 + 1];
            const uint32_t i2 = indexData[t * 3 + 2];
            const uint32_t base = t * 3;

            expandedPos[base + 0] = posData[i0];
            expandedPos[base + 1] = posData[i1];
            expandedPos[base + 2] = posData[i2];

            const filament::math::float4 primColor = primColConverted[t / 2];
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
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(numVertices)
            .attribute(filament::VertexAttribute::POSITION, posBuffer,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS)
            .attribute(filament::VertexAttribute::COLOR, colorBuffer,
                filament::VertexBuffer::AttributeType::FLOAT4)
            .attribute(filament::VertexAttribute::UV0, uv0Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2)
            .attribute(filament::VertexAttribute::UV1, uv1Buffer,
                filament::VertexBuffer::AttributeType::FLOAT2);

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
        const ANARIDataType a0Type = attr0Array->elementType();
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
        const ANARIDataType a1Type = attr1Array->elementType();
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

    fillDefaultAttributes(engine, numVertices, colorBuffer, uv0Buffer,
        uv1Buffer);

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

    const float globalRadius = getParam<float>("radius", 1.0f);
    const Corrade::Containers::String capsStr = getParamString("caps", "none");

    const uint32_t numEndpoints = uint32_t(posArray->totalSize());
    const uint32_t numCones = numEndpoints / 2;
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

    const bool globalCaps = (capsStr != "none"_s);
    constexpr uint32_t S = CYLINDER_SEGMENTS;

    // Per cone: tube same as cylinder, plus optional caps at each end
    const uint32_t tubeVerts = (S + 1) * 2;
    const uint32_t tubeIndices = S * 6;
    const uint32_t maxCapVerts = (S + 1) * 2;
    const uint32_t maxCapIndices = S * 3 * 2;
    const uint32_t maxVertsPerCone = tubeVerts + maxCapVerts;
    const uint32_t maxIndicesPerCone = tubeIndices + maxCapIndices;

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
        const filament::math::float3 A = endpoints[c * 2 + 0];
        const filament::math::float3 B = endpoints[c * 2 + 1];
        const float rA = radii ? radii[c * 2 + 0] : globalRadius;
        const float rB = radii ? radii[c * 2 + 1] : globalRadius;

        const bool capA = globalCaps || (caps && (caps[c * 2 + 0] != 0));
        const bool capB = globalCaps || (caps && (caps[c * 2 + 1] != 0));

        filament::math::float3 axis = B - A;
        const float len = length(axis);
        if (len < 1e-12f)
            axis = {0.0f, 1.0f, 0.0f};
        else
            axis /= len;

        filament::math::float3 u, v;
        buildFrame(axis, u, v);

        // Compute normal slope for cone surface
        const float dr = rA - rB;
        const float slope = (len > 1e-12f) ? dr / len : 0.0f;

        const uint32_t vBase = totalVerts;
        const uint32_t iBase = totalIndices;

        // Generate tube vertices
        for (uint32_t seg = 0; seg <= S; ++seg) {
            const float theta = 2.0f * Pi * float(seg) / S;
            const float ct = std::cos(theta);
            const float st = std::sin(theta);
            const filament::math::float3 circleDir = u * ct + v * st;

            // Normal: perpendicular to surface, accounting for cone slope
            const filament::math::float3 n = normalize(circleDir + axis * slope);

            allPositions[vBase + seg] = A + circleDir * rA;
            allNormals[vBase + seg] = n;

            allPositions[vBase + (S + 1) + seg] = B + circleDir * rB;
            allNormals[vBase + (S + 1) + seg] = n;
        }

        // Tube indices
        uint32_t idx = iBase;
        for (uint32_t seg = 0; seg < S; ++seg) {
            const uint32_t a0 = vBase + seg;
            const uint32_t a1 = vBase + seg + 1;
            const uint32_t b0 = vBase + (S + 1) + seg;
            const uint32_t b1 = vBase + (S + 1) + seg + 1;
            allIndices[idx++] = a0;
            allIndices[idx++] = a1;
            allIndices[idx++] = b0;
            allIndices[idx++] = a1;
            allIndices[idx++] = b1;
            allIndices[idx++] = b0;
        }

        totalVerts += tubeVerts;
        totalIndices += tubeIndices;

        // Cap A
        if (capA && rA > 1e-12f) {
            const uint32_t capBase = totalVerts;
            allPositions[capBase] = A;
            allNormals[capBase] = -axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                const float theta = 2.0f * Pi * float(seg) / S;
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
            const uint32_t capBase = totalVerts;
            allPositions[capBase] = B;
            allNormals[capBase] = axis;
            for (uint32_t seg = 0; seg < S; ++seg) {
                const float theta = 2.0f * Pi * float(seg) / S;
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
    const uint8_t posBuffer = bufIdx++;
    const uint8_t tangentBuffer = bufIdx++;
    const uint8_t colorBuffer = bufIdx++;
    const uint8_t uv0Buffer = bufIdx++;
    const uint8_t uv1Buffer = bufIdx++;
    const uint8_t bufferCount = bufIdx;

    auto vbBuilder = filament::VertexBuffer::Builder()
        .bufferCount(bufferCount)
        .vertexCount(totalVerts)
        .attribute(filament::VertexAttribute::POSITION, posBuffer,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS)
        .attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4)
        .attribute(filament::VertexAttribute::UV0, uv0Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2)
        .attribute(filament::VertexAttribute::UV1, uv1Buffer,
            filament::VertexBuffer::AttributeType::FLOAT2);

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
            const filament::math::float4 cA = srcColors[c * 2 + 0];
            const filament::math::float4 cB = srcColors[c * 2 + 1];
            const float rA = radii ? radii[c * 2 + 0] : globalRadius;
            const float rB = radii ? radii[c * 2 + 1] : globalRadius;
            const bool capA = globalCaps || (caps && (caps[c * 2 + 0] != 0));
            const bool capB = globalCaps || (caps && (caps[c * 2 + 1] != 0));

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

    fillDefaultAttributes(engine, totalVerts, colorBuffer, uv0Buffer,
        uv1Buffer);

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

void Geometry::fillDefaultAttributes(filament::Engine *engine,
    uint32_t vertexCount, uint8_t colorBuffer, uint8_t uv0Buffer,
    uint8_t uv1Buffer)
{
    if (!mHasColors) {
        Corrade::Containers::Array<filament::math::float4> colors{
            Corrade::NoInit, vertexCount};
        for (uint32_t i = 0; i < vertexCount; ++i)
            colors[i] = {1.0f, 1.0f, 1.0f, 1.0f};
        auto *released = colors.release();
        mVertexBuffer->setBufferAt(*engine, colorBuffer,
            filament::VertexBuffer::BufferDescriptor(
                released, vertexCount * sizeof(filament::math::float4),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float4 *>(buf);
                }));
    }
    if (!mHasUV0) {
        Corrade::Containers::Array<filament::math::float2> uvs{
            Corrade::ValueInit, vertexCount};
        auto *released = uvs.release();
        mVertexBuffer->setBufferAt(*engine, uv0Buffer,
            filament::VertexBuffer::BufferDescriptor(
                released, vertexCount * sizeof(filament::math::float2),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float2 *>(buf);
                }));
    }
    if (!mHasUV1) {
        Corrade::Containers::Array<filament::math::float2> uvs{
            Corrade::ValueInit, vertexCount};
        auto *released = uvs.release();
        mVertexBuffer->setBufferAt(*engine, uv1Buffer,
            filament::VertexBuffer::BufferDescriptor(
                released, vertexCount * sizeof(filament::math::float2),
                [](void *buf, size_t, void *) {
                    delete[] static_cast<filament::math::float2 *>(buf);
                }));
    }
}

}
