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

#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Geometry *);

namespace AnariFilament {

Geometry::Geometry(DeviceState *s)
    : Object(ANARI_GEOMETRY, s) {}

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
    filament::Engine *engine = deviceState()->engine;

    helium::Array1D *posArray =
        getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *colArray =
        getParamObject<helium::Array1D>("vertex.color");
    helium::Array1D *idxArray =
        getParamObject<helium::Array1D>("primitive.index");

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

    // Generate tangent frames using SurfaceOrientation (option 4: positions +
    // indices for flat shading)
    const filament::math::float3 *posData =
        static_cast<const filament::math::float3 *>(posArray->data());
    const filament::math::uint3 *triData =
        reinterpret_cast<const filament::math::uint3 *>(indexData);

    filament::geometry::SurfaceOrientation *orientation =
        filament::geometry::SurfaceOrientation::Builder()
            .vertexCount(numVertices)
            .positions(posData)
            .triangleCount(numTriangles)
            .triangles(triData)
            .build();

    Corrade::Containers::Array<filament::math::short4> tangents{
        Corrade::NoInit, numVertices};
    orientation->getQuats(tangents.data(), numVertices);
    delete orientation;

    // Build vertex buffer: POSITION + TANGENTS + optional COLOR
    uint8_t tangentBuffer = 1;
    uint8_t colorBuffer = mHasColors ? 2 : 0;
    uint8_t bufferCount = mHasColors ? 3 : 2;

    filament::VertexBuffer::Builder builder =
        filament::VertexBuffer::Builder()
            .bufferCount(bufferCount)
            .vertexCount(numVertices)
            .attribute(filament::VertexAttribute::POSITION, 0,
                filament::VertexBuffer::AttributeType::FLOAT3)
            .attribute(filament::VertexAttribute::TANGENTS, tangentBuffer,
                filament::VertexBuffer::AttributeType::SHORT4)
            .normalized(filament::VertexAttribute::TANGENTS);

    if (mHasColors) {
        builder.attribute(filament::VertexAttribute::COLOR, colorBuffer,
            filament::VertexBuffer::AttributeType::FLOAT4);
    }

    mVertexBuffer = builder.build(*engine);

    // Upload position data
    size_t posSize = numVertices * sizeof(float) * 3;
    mVertexBuffer->setBufferAt(*engine, 0,
        filament::VertexBuffer::BufferDescriptor(
            posData, posSize));

    // Upload tangent data (transfer ownership via callback)
    filament::math::short4 *tangentOwned =
        new filament::math::short4[numVertices];
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

}
