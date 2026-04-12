# Filament ANARI Implementation

[![pipeline](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/badges/master/pipeline.svg)](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/-/pipelines)
[![coverage](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/badges/master/coverage.svg)](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/-/pipelines)
[![license](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![ANARI](https://img.shields.io/badge/ANARI-0.16.0-green.svg)](https://www.khronos.org/anari/)
[![Filament](https://img.shields.io/badge/Filament-1.71.0-orange.svg)](https://github.com/google/filament)

An [ANARI](https://www.khronos.org/anari/) device implementation powered by
Google's [Filament](https://github.com/google/filament) physically-based
rendering engine.

Any application using the ANARI API can use this library to render through
Filament's real-time PBR pipeline — simply load `"filament"` as the ANARI
library.

## Supported Platforms

| Platform | Architecture | Build | Test |
|----------|-------------|-------|------|
| Windows  | x86_64      | ✅    | ✅   |
| macOS    | arm64       | ✅    | ✅   |
| Linux    | x86_64      | ✅    | ✅   |
| Linux    | arm64       | ✅    | ✅   |
| iOS      | arm64       | ✅    | —    |
| Android  | arm64       | ✅    | —    |

## Quick Start

### Prerequisites

- CMake ≥ 3.22
- Ninja
- C++20 compiler (Clang on macOS/Linux, MSVC 2022 on Windows)
- Git (for submodules)

### Clone & Build

```bash
git clone --recursive https://gitlab.com/wonderland-gmbh/filament-anari-implementation.git
cd filament-anari-implementation
```

#### macOS / Linux

```bash
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

#### Windows (Developer PowerShell)

```powershell
. scripts/vcvarsall.ps1
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
ctest --test-dir build-ninja --output-on-failure
```

Filament precompiled libraries are downloaded automatically during the CMake
configure step if not already present.

### Usage

Set the `ANARI_LIBRARY` environment variable or load programmatically:

```c
ANARILibrary lib = anariLoadLibrary("filament", statusFunc);
ANARIDevice dev = anariNewDevice(lib, "default");
```

Any ANARI application — including the SDK examples and the
[Blender addon](https://github.com/KhronosGroup/anari-blender-addon) — can use
this device without code changes.

## Project Structure

```
├── CMakeLists.txt          # Root build
├── ROADMAP.md              # Feature roadmap & milestones
├── external/
│   ├── anari-sdk/          # ANARI SDK (git submodule)
│   ├── corrade/            # Corrade (git submodule)
│   └── filament/           # Filament SDK (auto-downloaded, gitignored)
├── cmake/                  # CMake helpers (Filament download, etc.)
├── scripts/                # Build helper scripts
└── src/
    ├── Device.h/cpp        # ANARI device implementation
    ├── Library.cpp         # ANARI library entry point
    └── Test/               # Corrade TestSuite tests
```

## Implemented Features

### Geometry
| Subtype | Status |
|---------|--------|
| `triangle` | ✅ positions, normals, tangents, colors, UVs, indices |
| `sphere` | ✅ per-sphere radius, normals, per-primitive colors |
| `cylinder` | ✅ per-cylinder radius, capped, normals, per-primitive colors |
| `quad` | ✅ split into 2 triangles, normals, tangents, colors |
| `cone` | ✅ variable per-endpoint radius, surface normals |

### Light
| Subtype | Status |
|---------|--------|
| `directional` | ✅ direction, color, intensity/irradiance |
| `point` | ✅ position, color, intensity/power, falloff |
| `spot` | ✅ position, direction, openingAngle, falloffAngle, color, intensity |

### Camera
| Subtype | Status |
|---------|--------|
| `perspective` | ✅ fovy, aspect, near, far |
| `orthographic` | ✅ height, aspect, near, far |

### Material
| Subtype | `alphaMode` | Status |
|---------|-------------|--------|
| `matte` | `opaque` | ✅ color, opacity, normal sampler |
| `matte` | `blend` | ✅ |
| `matte` | `mask` + `alphaCutoff` | ✅ |
| `physicallyBased` | `opaque` | ✅ baseColor, metallic, roughness, emissive, ior, normal sampler |
| `physicallyBased` | `blend` | ✅ |
| `physicallyBased` | `mask` + `alphaCutoff` | ✅ |

### Sampler
| Subtype | Status |
|---------|--------|
| `image2D` | ✅ `UFIXED8_VEC4` and `FLOAT32_VEC4` |
| `image1D` | ✅ |
| `image3D` | ✅ |
| `transform` | ✅ 4×4 matrix applied to UV attribute |
| `primitive` | ✅ per-primitive attribute expansion |

### Renderer & Frame
| Feature | Status |
|---------|--------|
| `backgroundColor` | ✅ |
| `ambientColor` + `ambientRadiance` | ✅ uniform IBL via spherical harmonics |
| Frame `color` channel (`FLOAT32_VEC4`, `UFIXED8_VEC4`) | ✅ |

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full feature plan. Current status:

- **Milestone 0** ✅ Device skeleton, CI on all platforms
- **Milestone 1** ✅ Tutorial — triangle, matte material, perspective camera, directional light
- **Milestone 2** ✅ Sphere geometry, textures (image2D), PBR material, groups and instances
- **Milestone 3** ✅ Cylinder geometry, all sampler subtypes, per-primitive colors
- **Milestone 4** ✅ Complete feature coverage — quad/cone geometry, point/spot lights, orthographic camera, ambient lighting, material alpha modes
- **Milestone 5** 🔄 Planned — curve geometry, frame depth channel, object introspection
- **Milestone 6** 🔄 Planned — Blender addon integration

## Documentation

See [docs/building.md](docs/building.md) for detailed build instructions.

## License

MIT — see [LICENSE](LICENSE).
