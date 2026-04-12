// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>

#include <anari/anari.h>

#include "ColorConversion.h"
#include <math/vec4.h>

namespace {

struct ColorConversionTest : Corrade::TestSuite::Tester {
    explicit ColorConversionTest();

    void float32();
    void ufixed8();
    void ufixed16();
    void ufixed32();
    void multiBatch();
};

ColorConversionTest::ColorConversionTest()
{
    addTests({&ColorConversionTest::float32,
        &ColorConversionTest::ufixed8,
        &ColorConversionTest::ufixed16,
        &ColorConversionTest::ufixed32,
        &ColorConversionTest::multiBatch});
}

using namespace AnariFilament;
using F4 = filament::math::float4;

void ColorConversionTest::float32()
{
    // VEC4 — passthrough
    {
        float src[] = {0.25f, 0.5f, 0.75f, 1.0f};
        F4 dst[1];
        convertColors(dst, src, ANARI_FLOAT32_VEC4, 1);
        CORRADE_COMPARE(dst[0].r, 0.25f);
        CORRADE_COMPARE(dst[0].g, 0.5f);
        CORRADE_COMPARE(dst[0].b, 0.75f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC3 — alpha filled to 1
    {
        float src[] = {0.1f, 0.2f, 0.3f};
        F4 dst[1];
        convertColors(dst, src, ANARI_FLOAT32_VEC3, 1);
        CORRADE_COMPARE(dst[0].r, 0.1f);
        CORRADE_COMPARE(dst[0].g, 0.2f);
        CORRADE_COMPARE(dst[0].b, 0.3f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC2 — blue=0, alpha=1
    {
        float src[] = {0.4f, 0.6f};
        F4 dst[1];
        convertColors(dst, src, ANARI_FLOAT32_VEC2, 1);
        CORRADE_COMPARE(dst[0].r, 0.4f);
        CORRADE_COMPARE(dst[0].g, 0.6f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // Scalar — grey, alpha=1
    {
        float src[] = {0.8f};
        F4 dst[1];
        convertColors(dst, src, ANARI_FLOAT32, 1);
        CORRADE_COMPARE(dst[0].r, 0.8f);
        CORRADE_COMPARE(dst[0].g, 0.8f);
        CORRADE_COMPARE(dst[0].b, 0.8f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }
}

void ColorConversionTest::ufixed8()
{
    // VEC4 — 0→0.0, 255→1.0
    {
        uint8_t src[] = {0, 128, 255, 255};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED8_VEC4, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_COMPARE(dst[0].g, 128.0f/255.0f);
        CORRADE_COMPARE(dst[0].b, 1.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC3 — alpha filled to 1
    {
        uint8_t src[] = {255, 0, 0};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED8_VEC3, 1);
        CORRADE_COMPARE(dst[0].r, 1.0f);
        CORRADE_COMPARE(dst[0].g, 0.0f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC2 — blue=0, alpha=1
    {
        uint8_t src[] = {255, 128};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED8_VEC2, 1);
        CORRADE_COMPARE(dst[0].r, 1.0f);
        CORRADE_COMPARE(dst[0].g, 128.0f/255.0f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // Scalar — grey, alpha=1
    {
        uint8_t src[] = {255};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED8, 1);
        CORRADE_COMPARE(dst[0].r, 1.0f);
        CORRADE_COMPARE(dst[0].g, 1.0f);
        CORRADE_COMPARE(dst[0].b, 1.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }
}

void ColorConversionTest::ufixed16()
{
    // VEC4 — 0→0.0, 65535→1.0
    {
        uint16_t src[] = {0, 32768, 65535, 65535};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED16_VEC4, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_COMPARE(dst[0].g, 32768.0f/65535.0f);
        CORRADE_COMPARE(dst[0].b, 1.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC3
    {
        uint16_t src[] = {65535, 0, 65535};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED16_VEC3, 1);
        CORRADE_COMPARE(dst[0].r, 1.0f);
        CORRADE_COMPARE(dst[0].g, 0.0f);
        CORRADE_COMPARE(dst[0].b, 1.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC2
    {
        uint16_t src[] = {0, 65535};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED16_VEC2, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_COMPARE(dst[0].g, 1.0f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // Scalar — grey, alpha=1
    {
        uint16_t src[] = {0};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED16, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_COMPARE(dst[0].g, 0.0f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }
}

void ColorConversionTest::ufixed32()
{
    // VEC4 — 0→0.0, 4294967295→1.0
    {
        uint32_t src[] = {0, 4294967295u, 0, 4294967295u};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED32_VEC4, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
        CORRADE_VERIFY(dst[0].g > 0.9999f);
    }

    // VEC3
    {
        uint32_t src[] = {4294967295u, 4294967295u, 4294967295u};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED32_VEC3, 1);
        CORRADE_VERIFY(dst[0].r > 0.9999f);
        CORRADE_VERIFY(dst[0].g > 0.9999f);
        CORRADE_VERIFY(dst[0].b > 0.9999f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // VEC2
    {
        uint32_t src[] = {0, 4294967295u};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED32_VEC2, 1);
        CORRADE_COMPARE(dst[0].r, 0.0f);
        CORRADE_VERIFY(dst[0].g > 0.9999f);
        CORRADE_COMPARE(dst[0].b, 0.0f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }

    // Scalar — grey, alpha=1
    {
        uint32_t src[] = {4294967295u};
        F4 dst[1];
        convertColors(dst, src, ANARI_UFIXED32, 1);
        CORRADE_VERIFY(dst[0].r > 0.9999f);
        CORRADE_VERIFY(dst[0].g > 0.9999f);
        CORRADE_VERIFY(dst[0].b > 0.9999f);
        CORRADE_COMPARE(dst[0].a, 1.0f);
    }
}

void ColorConversionTest::multiBatch()
{
    // Two elements — type dispatch happens once, both elements converted
    float src[] = {1.0f, 0.0f, 0.0f, 1.0f,  0.0f, 1.0f, 0.0f, 1.0f};
    F4 dst[2];
    convertColors(dst, src, ANARI_FLOAT32_VEC4, 2);
    CORRADE_COMPARE(dst[0].r, 1.0f);
    CORRADE_COMPARE(dst[0].g, 0.0f);
    CORRADE_COMPARE(dst[1].r, 0.0f);
    CORRADE_COMPARE(dst[1].g, 1.0f);
}

}

CORRADE_TEST_MAIN(ColorConversionTest)
