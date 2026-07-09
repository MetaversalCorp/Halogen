// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#pragma once

#include <filament/Texture.h>
#include <backend/DriverEnums.h>

#include <Corrade/Containers/ArrayView.h>
#include <Corrade/Containers/StringView.h>

namespace Halogen {

// One EXT_SAMPLER_COMPRESSED_FORMAT_* 'format' string and the Filament enums
// Halogen uploads it as. 'name' is exactly the ANARI spec token, so the same
// string is used to probe capability and to report it back to clients.
struct CompressedFormat {
    const char *name;
    filament::Texture::InternalFormat internal;
    filament::backend::CompressedPixelDataType compressed;
};

// The complete set of compressed 'format' strings across the ANARI BC1-7,
// ETC2, EAC and ASTC extensions, each mapped to Filament. Single source of
// truth shared by the compressedImage2D sampler (upload) and the device
// 'halogen.textureFormats' property (capability probe), so the two can never
// drift apart.
Corrade::Containers::ArrayView<const CompressedFormat> compressedFormatTable();

// Looks up a spec 'format' string in the table; nullptr if Halogen does not
// map it.
const CompressedFormat *findCompressedFormat(
    Corrade::Containers::StringView name);

}
