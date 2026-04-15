// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>

#include "Aabb.h"

namespace {

struct AabbTest : Corrade::TestSuite::Tester {
    explicit AabbTest();

    void defaultIsInvalid();
    void extendSinglePoint();
    void extendMultiplePoints();
    void computeAabbFromPositions();
    void centerAndHalfExtent();
};

AabbTest::AabbTest()
{
    addTests({&AabbTest::defaultIsInvalid,
        &AabbTest::extendSinglePoint,
        &AabbTest::extendMultiplePoints,
        &AabbTest::computeAabbFromPositions,
        &AabbTest::centerAndHalfExtent});
}

void AabbTest::defaultIsInvalid()
{
    Halogen::Aabb aabb;
    CORRADE_VERIFY(aabb.min.x > aabb.max.x);
}

void AabbTest::extendSinglePoint()
{
    Halogen::Aabb aabb;
    aabb.extend({1.0f, 2.0f, 3.0f});
    CORRADE_COMPARE(aabb.min.x, 1.0f);
    CORRADE_COMPARE(aabb.min.y, 2.0f);
    CORRADE_COMPARE(aabb.min.z, 3.0f);
    CORRADE_COMPARE(aabb.max.x, 1.0f);
    CORRADE_COMPARE(aabb.max.y, 2.0f);
    CORRADE_COMPARE(aabb.max.z, 3.0f);
}

void AabbTest::extendMultiplePoints()
{
    Halogen::Aabb aabb;
    aabb.extend({-1.0f, 0.0f, 0.0f});
    aabb.extend({1.0f, 2.0f, 3.0f});
    CORRADE_COMPARE(aabb.min.x, -1.0f);
    CORRADE_COMPARE(aabb.min.y, 0.0f);
    CORRADE_COMPARE(aabb.min.z, 0.0f);
    CORRADE_COMPARE(aabb.max.x, 1.0f);
    CORRADE_COMPARE(aabb.max.y, 2.0f);
    CORRADE_COMPARE(aabb.max.z, 3.0f);
}

void AabbTest::computeAabbFromPositions()
{
    filament::math::float3 positions[] = {
        {0.0f, 0.0f, 0.0f},
        {1.0f, 2.0f, 3.0f},
        {-1.0f, -2.0f, -3.0f}};

    Halogen::Aabb aabb =
        Halogen::computeAabb(positions, 3);
    CORRADE_COMPARE(aabb.min.x, -1.0f);
    CORRADE_COMPARE(aabb.min.y, -2.0f);
    CORRADE_COMPARE(aabb.min.z, -3.0f);
    CORRADE_COMPARE(aabb.max.x, 1.0f);
    CORRADE_COMPARE(aabb.max.y, 2.0f);
    CORRADE_COMPARE(aabb.max.z, 3.0f);
}

void AabbTest::centerAndHalfExtent()
{
    Halogen::Aabb aabb;
    aabb.extend({0.0f, 0.0f, 0.0f});
    aabb.extend({2.0f, 4.0f, 6.0f});

    filament::math::float3 center = aabb.center();
    CORRADE_COMPARE(center.x, 1.0f);
    CORRADE_COMPARE(center.y, 2.0f);
    CORRADE_COMPARE(center.z, 3.0f);

    filament::math::float3 half = aabb.halfExtent();
    CORRADE_COMPARE(half.x, 1.0f);
    CORRADE_COMPARE(half.y, 2.0f);
    CORRADE_COMPARE(half.z, 3.0f);
}

}

CORRADE_TEST_MAIN(AabbTest)
