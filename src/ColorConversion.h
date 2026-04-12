// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <anari/anari.h>
#include <math/vec4.h>

#include <algorithm>
#include <cstdint>

namespace AnariFilament {

/// Convert N color elements from an ANARI typed source array to float4 RGBA.
/// Type dispatch happens once outside the per-element loop for efficiency.
inline void convertColors(filament::math::float4 *dst,
    const void *src, ANARIDataType type, size_t count)
{
    switch (type) {
    case ANARI_FLOAT32_VEC4: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 4)
            dst[i] = {p[0], p[1], p[2], p[3]};
    } break;
    case ANARI_FLOAT32_VEC3: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 3)
            dst[i] = {p[0], p[1], p[2], 1.0f};
    } break;
    case ANARI_FLOAT32_VEC2: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 2)
            dst[i] = {p[0], p[1], 0.0f, 1.0f};
    } break;
    case ANARI_FLOAT32: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i)
            dst[i] = {p[i], p[i], p[i], 1.0f};
    } break;
    case ANARI_UFIXED8_VEC4: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 4)
            dst[i] = {p[0]/255.0f, p[1]/255.0f, p[2]/255.0f, p[3]/255.0f};
    } break;
    case ANARI_UFIXED8_VEC3: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 3)
            dst[i] = {p[0]/255.0f, p[1]/255.0f, p[2]/255.0f, 1.0f};
    } break;
    case ANARI_UFIXED8_VEC2: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 2)
            dst[i] = {p[0]/255.0f, p[1]/255.0f, 0.0f, 1.0f};
    } break;
    case ANARI_UFIXED8: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i)
            dst[i] = {p[i]/255.0f, p[i]/255.0f, p[i]/255.0f, 1.0f};
    } break;
    case ANARI_UFIXED16_VEC4: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 4)
            dst[i] = {p[0]/65535.0f, p[1]/65535.0f, p[2]/65535.0f, p[3]/65535.0f};
    } break;
    case ANARI_UFIXED16_VEC3: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 3)
            dst[i] = {p[0]/65535.0f, p[1]/65535.0f, p[2]/65535.0f, 1.0f};
    } break;
    case ANARI_UFIXED16_VEC2: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 2)
            dst[i] = {p[0]/65535.0f, p[1]/65535.0f, 0.0f, 1.0f};
    } break;
    case ANARI_UFIXED16: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i)
            dst[i] = {p[i]/65535.0f, p[i]/65535.0f, p[i]/65535.0f, 1.0f};
    } break;
    case ANARI_UFIXED32_VEC4: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        constexpr float s = 1.0f / 4294967295.0f;
        for (size_t i = 0; i < count; ++i, p += 4)
            dst[i] = {p[0]*s, p[1]*s, p[2]*s, p[3]*s};
    } break;
    case ANARI_UFIXED32_VEC3: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        constexpr float s = 1.0f / 4294967295.0f;
        for (size_t i = 0; i < count; ++i, p += 3)
            dst[i] = {p[0]*s, p[1]*s, p[2]*s, 1.0f};
    } break;
    case ANARI_UFIXED32_VEC2: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        constexpr float s = 1.0f / 4294967295.0f;
        for (size_t i = 0; i < count; ++i, p += 2)
            dst[i] = {p[0]*s, p[1]*s, 0.0f, 1.0f};
    } break;
    case ANARI_UFIXED32: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        constexpr float s = 1.0f / 4294967295.0f;
        for (size_t i = 0; i < count; ++i)
            dst[i] = {p[i]*s, p[i]*s, p[i]*s, 1.0f};
    } break;
    default:
        for (size_t i = 0; i < count; ++i)
            dst[i] = {0.0f, 0.0f, 0.0f, 1.0f};
        break;
    }
}

/// Convert N color elements from an ANARI typed source array to RGBA8.
/// Type dispatch happens once outside the per-element loop for efficiency.
inline void convertToRGBA8(uint8_t *dst,
    const void *src, ANARIDataType type, size_t count)
{
    switch (type) {
    case ANARI_FLOAT32_VEC4: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 4, dst += 4) {
            dst[0] = static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f);
            dst[1] = static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f);
            dst[2] = static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f);
            dst[3] = static_cast<uint8_t>(std::clamp(p[3], 0.0f, 1.0f) * 255.0f);
        }
    } break;
    case ANARI_FLOAT32_VEC3: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 3, dst += 4) {
            dst[0] = static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f);
            dst[1] = static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f);
            dst[2] = static_cast<uint8_t>(std::clamp(p[2], 0.0f, 1.0f) * 255.0f);
            dst[3] = 255;
        }
    } break;
    case ANARI_FLOAT32_VEC2: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, p += 2, dst += 4) {
            dst[0] = static_cast<uint8_t>(std::clamp(p[0], 0.0f, 1.0f) * 255.0f);
            dst[1] = static_cast<uint8_t>(std::clamp(p[1], 0.0f, 1.0f) * 255.0f);
            dst[2] = 0;
            dst[3] = 255;
        }
    } break;
    case ANARI_FLOAT32: {
        const float *p = static_cast<const float *>(src);
        for (size_t i = 0; i < count; ++i, ++p, dst += 4) {
            uint8_t v = static_cast<uint8_t>(std::clamp(*p, 0.0f, 1.0f) * 255.0f);
            dst[0] = v; dst[1] = v; dst[2] = v; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED8_VEC4: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 4, dst += 4) {
            dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2]; dst[3] = p[3];
        }
    } break;
    case ANARI_UFIXED8_VEC3: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 3, dst += 4) {
            dst[0] = p[0]; dst[1] = p[1]; dst[2] = p[2]; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED8_VEC2: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 2, dst += 4) {
            dst[0] = p[0]; dst[1] = p[1]; dst[2] = 0; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED8: {
        const uint8_t *p = static_cast<const uint8_t *>(src);
        for (size_t i = 0; i < count; ++i, dst += 4) {
            uint8_t v = p[i];
            dst[0] = v; dst[1] = v; dst[2] = v; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED16_VEC4: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 4, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 8);
            dst[1] = static_cast<uint8_t>(p[1] >> 8);
            dst[2] = static_cast<uint8_t>(p[2] >> 8);
            dst[3] = static_cast<uint8_t>(p[3] >> 8);
        }
    } break;
    case ANARI_UFIXED16_VEC3: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 3, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 8);
            dst[1] = static_cast<uint8_t>(p[1] >> 8);
            dst[2] = static_cast<uint8_t>(p[2] >> 8);
            dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED16_VEC2: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 2, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 8);
            dst[1] = static_cast<uint8_t>(p[1] >> 8);
            dst[2] = 0; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED16: {
        const uint16_t *p = static_cast<const uint16_t *>(src);
        for (size_t i = 0; i < count; ++i, dst += 4) {
            uint8_t b = static_cast<uint8_t>(p[i] >> 8);
            dst[0] = b; dst[1] = b; dst[2] = b; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED32_VEC4: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 4, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 24);
            dst[1] = static_cast<uint8_t>(p[1] >> 24);
            dst[2] = static_cast<uint8_t>(p[2] >> 24);
            dst[3] = static_cast<uint8_t>(p[3] >> 24);
        }
    } break;
    case ANARI_UFIXED32_VEC3: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 3, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 24);
            dst[1] = static_cast<uint8_t>(p[1] >> 24);
            dst[2] = static_cast<uint8_t>(p[2] >> 24);
            dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED32_VEC2: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        for (size_t i = 0; i < count; ++i, p += 2, dst += 4) {
            dst[0] = static_cast<uint8_t>(p[0] >> 24);
            dst[1] = static_cast<uint8_t>(p[1] >> 24);
            dst[2] = 0; dst[3] = 255;
        }
    } break;
    case ANARI_UFIXED32: {
        const uint32_t *p = static_cast<const uint32_t *>(src);
        for (size_t i = 0; i < count; ++i, dst += 4) {
            uint8_t b = static_cast<uint8_t>(p[i] >> 24);
            dst[0] = b; dst[1] = b; dst[2] = b; dst[3] = 255;
        }
    } break;
    default: {
        for (size_t i = 0; i < count; ++i, dst += 4) {
            dst[0] = 0; dst[1] = 0; dst[2] = 0; dst[3] = 255;
        }
    } break;
    }
}

} // namespace AnariFilament
