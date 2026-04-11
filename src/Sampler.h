// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include "Object.h"

namespace filament {
class Texture;
class TextureSampler;
}

namespace AnariFilament {

struct Sampler : public Object
{
    Sampler(DeviceState *s);
    ~Sampler() override;

    void commitParameters() override;

    filament::Texture *texture() const { return mTexture; }
    bool isNearest() const { return mNearest; }

private:
    filament::Texture *mTexture = nullptr;
    bool mNearest = false;
};

}

ANARI_FILAMENT_TYPEFOR_SPECIALIZATION(
    AnariFilament::Sampler *, ANARI_SAMPLER);
