// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include "Surface.h"

#include "Sampler.h"


#include <filament/Engine.h>
#include <filament/IndexBuffer.h>
#include <filament/RenderableManager.h>
#include <filament/VertexBuffer.h>

#include <geometry/SurfaceOrientation.h>

#include <helium/array/Array1D.h>

#include <utils/EntityManager.h>

#include <cstring>

ANARI_FILAMENT_TYPEFOR_DEFINITION(AnariFilament::Surface *);

namespace AnariFilament {

Surface::Surface(DeviceState *s)
    : Object(ANARI_SURFACE, s)
{
    mEntity = utils::EntityManager::get().create();
}

Surface::~Surface()
{
    filament::Engine *engine = deviceState()->engine;
    if (mOwnedVB)
        engine->destroy(mOwnedVB);
    if (mOwnedIB)
        engine->destroy(mOwnedIB);
    engine->destroy(mEntity);
    utils::EntityManager::get().destroy(mEntity);
}

void Surface::buildPrimitiveSamplerBuffers()
{
    filament::Engine * const engine = deviceState()->engine;

    if (mOwnedVB) {
        engine->destroy(mOwnedVB);
        mOwnedVB = nullptr;
    }
    if (mOwnedIB) {
        engine->destroy(mOwnedIB);
        mOwnedIB = nullptr;
    }

    const Sampler *sampler = mMaterial->colorSampler();
    if (!sampler)
        return;

    const auto &primColors = sampler->primitiveColors();
    if (primColors.isEmpty())
        return;

    // Get the original geometry's vertex position and index data
    helium::Array1D *posArray =
        mGeometry->getParamObject<helium::Array1D>("vertex.position");
    helium::Array1D *idxArray =
        mGeometry->getParamObject<helium::Array1D>("primitive.index");

    if (!posArray)
        return;

    const filament::math::float3 *srcPos =
        static_cast<const filament::math::float3 *>(posArray->data());
    const uint32_t numSrcVerts = uint32_t(posArray->totalSize());

    // Build index list
    uint32_t numTris = 0;
    const uint32_t *srcIdx = nullptr;
    Corrade::Containers::Array<uint32_t> genIdx;

    if (idxArray) {
        numTris = uint32_t(idxArray->totalSize());
        srcIdx = static_cast<const uint32_t *>(idxArray->data());
    } else {
        numTris = numSrcVerts / 3;
        genIdx = Corrade::Containers::Array<uint32_t>{
            Corrade::NoInit, numSrcVerts};
        for (uint32_t i = 0; i < numSrcVerts; ++i)
            genIdx[i] = i;
        srcIdx = genIdx.data();
    }

    const uint32_t expandedCount = numTris * 3;

    // Expand positions, generate normals, and set per-vertex colors
    auto *positions = new filament::math::float3[expandedCount];
    auto *colors = new filament::math::float4[expandedCount];

    for (uint32_t t = 0; t < numTris; ++t) {
        const uint32_t i0 = srcIdx[t * 3 + 0];
        const uint32_t i1 = srcIdx[t * 3 + 1];
        const uint32_t i2 = srcIdx[t * 3 + 2];
        const uint32_t base = t * 3;

        positions[base + 0] = srcPos[i0];
        positions[base + 1] = srcPos[i1];
        positions[base + 2] = srcPos[i2];

        filament::math::float4 c = {0.5f, 0.5f, 0.5f, 1.0f};
        if (t < primColors.size())
            c = primColors[t];
        colors[base + 0] = c;
        colors[base + 1] = c;
        colors[base + 2] = c;
    }

    // Generate sequential indices
    auto *indices = new uint32_t[expandedCount];
    for (uint32_t i = 0; i < expandedCount; ++i)
        indices[i] = i;

    // Generate tangent frames
    filament::geometry::SurfaceOrientation *orientation =
        filament::geometry::SurfaceOrientation::Builder()
            .vertexCount(expandedCount)
            .positions(positions)
            .triangleCount(numTris)
            .triangles(reinterpret_cast<const filament::math::uint3 *>(indices))
            .build();

    auto *tangents = new filament::math::short4[expandedCount];
    orientation->getQuats(tangents, expandedCount);
    delete orientation;

    mOwnedVB = filament::VertexBuffer::Builder()
        .bufferCount(3)
        .vertexCount(expandedCount)
        .attribute(filament::VertexAttribute::POSITION, 0,
            filament::VertexBuffer::AttributeType::FLOAT3)
        .attribute(filament::VertexAttribute::TANGENTS, 1,
            filament::VertexBuffer::AttributeType::SHORT4)
        .normalized(filament::VertexAttribute::TANGENTS)
        .attribute(filament::VertexAttribute::COLOR, 2,
            filament::VertexBuffer::AttributeType::FLOAT4)
        .build(*engine);

    mOwnedVB->setBufferAt(*engine, 0,
        filament::VertexBuffer::BufferDescriptor(
            positions, expandedCount * sizeof(filament::math::float3),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::float3 *>(buf);
            }));
    mOwnedVB->setBufferAt(*engine, 1,
        filament::VertexBuffer::BufferDescriptor(
            tangents, expandedCount * sizeof(filament::math::short4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::short4 *>(buf);
            }));
    mOwnedVB->setBufferAt(*engine, 2,
        filament::VertexBuffer::BufferDescriptor(
            colors, expandedCount * sizeof(filament::math::float4),
            [](void *buf, size_t, void *) {
                delete[] static_cast<filament::math::float4 *>(buf);
            }));

    mOwnedIB = filament::IndexBuffer::Builder()
        .indexCount(expandedCount)
        .bufferType(filament::IndexBuffer::IndexType::UINT)
        .build(*engine);
    mOwnedIB->setBuffer(*engine,
        filament::IndexBuffer::BufferDescriptor(
            indices, expandedCount * sizeof(uint32_t),
            [](void *buf, size_t, void *) {
                delete[] static_cast<uint32_t *>(buf);
            }));
}

void Surface::commitParameters()
{
    mGeometry = getParamObject<Geometry>("geometry");
    mMaterial = getParamObject<Material>("material");

    markCommitted();
}

void Surface::finalize()
{
    if (!mGeometry || !mMaterial)
        return;

    if (!mGeometry->vertexBuffer() || !mGeometry->indexBuffer()
        || !mMaterial->materialInstance())
        return;

    filament::Engine * const engine = deviceState()->engine;

    if (mBuilt)
        engine->getRenderableManager().destroy(mEntity);

    filament::VertexBuffer *vb = mGeometry->vertexBuffer();
    filament::IndexBuffer *ib = mGeometry->indexBuffer();
    uint32_t idxCount = mGeometry->indexCount();

    // Handle primitive sampler: build expanded vertex data
    if (mMaterial->usesPrimitiveSampler()) {
        buildPrimitiveSamplerBuffers();
        if (mOwnedVB && mOwnedIB) {
            vb = mOwnedVB;
            ib = mOwnedIB;
            idxCount = uint32_t(mOwnedIB->getIndexCount());
        }
    }

    const Aabb &geomAabb = mGeometry->aabb();
    const filament::Box box = {geomAabb.center(), geomAabb.halfExtent()};

    filament::RenderableManager::Builder(1)
        .geometry(0, filament::RenderableManager::PrimitiveType::TRIANGLES,
            vb, ib, 0, idxCount)
        .material(0, mMaterial->materialInstance())
        .boundingBox(box)
        .receiveShadows(true)
        .castShadows(true)
        .build(*engine, mEntity);

    mBuilt = true;
}

bool Surface::isValid() const
{
    return mGeometry && mMaterial && mGeometry->vertexBuffer()
        && mMaterial->materialInstance();
}

}
