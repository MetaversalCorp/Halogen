# Copilot Instructions — Filament ANARI Implementation

## Project Purpose

This repository implements an [ANARI](https://www.khronos.org/anari/) device
backed by Google's [Filament](https://github.com/google/filament) 3D rendering
engine. ANARI is a Khronos standard that provides an abstraction layer for 3D
rendering libraries. This implementation allows any ANARI-based application to
render through Filament's physically-based renderer.

## Architecture Overview

- **ANARI Device** (`src/`): The core implementation that maps ANARI API calls
  to Filament engine operations. The main entry point is `Device.h/cpp`.
- **Helium base classes**: Our objects inherit from helium (the ANARI SDK's
  reference device framework). `helium::BaseFrame`, `helium::Array1D`, etc.
  provide commit buffering, ref-counting, and parameter management.
- **External dependencies** (`external/`): Git submodules for ANARI SDK and
  Corrade (testing).
- **Filament**: Used as precompiled libraries, downloaded automatically during
  the CMake configure step if not found locally.
- **Tests** (`src/Test/`): Written using Corrade TestSuite.
- **CI**: GitLab CI targeting Windows (MSVC), macOS (Clang), with plans for
  Linux, Android, and iOS.

### Key Object Hierarchy

| ANARI Object | Source file | Filament mapping |
|-------------|------------|-----------------|
| `Device` | `Device.cpp` | `Engine`, `Renderer` |
| `Frame` | `Frame.cpp` | `View`, `RenderTarget`, `SwapChain`, `IndirectLight` |
| `World` | `World.cpp` | `Scene`, `TransformManager` |
| `Camera` | `Camera.cpp` | `Camera` (`lookAt`, `setProjection`, `setExposure`) |
| `Light` | `Light.cpp` | `LightManager::Builder` (directional, point, spot) |
| `Surface` | `Surface.cpp` | `RenderableManager::Builder` |
| `Geometry` | `Geometry.cpp` | `VertexBuffer`, `IndexBuffer`, `SurfaceOrientation` |
| `Material` | `Material.cpp` | `MaterialInstance` (matte, physicallyBased) |
| `Sampler` | `Sampler.cpp` | `Texture`, `TextureSampler` |
| `Instance`/`Group` | `Instance.cpp`, `Group.cpp` | `TransformManager` entity hierarchy |
| `Renderer` | `Renderer.h` | Renderer parameters (background, ambient) |

### Commit & Finalize Order

Helium's `DeferredCommitBuffer` processes `commitParameters()` in insertion
order, then `finalize()` in priority order. Our priorities ensure dependencies
are finalized before dependents:

| Priority | Object | Reason |
|----------|--------|--------|
| 0 | Geometry | Must be ready before Surface reads its buffers |
| 2 | Surface | Must have geometry + material before Group iterates |
| 3 | Group | Must have surfaces before World builds scene |
| 5 | World | Scene building is the last step |

### Critical: Filament Async Buffer Uploads

**Filament's `setBufferAt()` / `setBuffer()` calls are asynchronous.** The
data pointer passed to `BufferDescriptor` must remain valid until the GPU
upload completes. This means:
- **Never pass stack-local or temporary data** to these calls.
- **Heap-allocate and provide a cleanup callback:**
  ```cpp
  auto *owned = new float3[count];
  std::memcpy(owned, src, count * sizeof(float3));
  vb->setBufferAt(*engine, slot, BufferDescriptor(
      owned, count * sizeof(float3),
      [](void *buf, size_t, void *) {
          delete[] static_cast<float3 *>(buf);
      }));
  ```
- The same applies to `IndexBuffer::setBuffer()` and `Texture::setImage()`.
- Always call `engine->flushAndWait()` before reading back pixels.

### Image Orientation

Filament's `Renderer::readPixels()` returns framebuffer data bottom-to-top
(GPU convention). `Frame::renderFrame()` flips the pixel buffer vertically
after readback so that ANARI consumers receive row 0 at the top.

### Lighting & IBL

- **Shadows** are enabled by default (`castShadows(true)` /
  `receiveShadows(true)`) on all lights and renderables.
- **Default IBL**: When no `ambientRadiance` is set, a 3-band spherical
  harmonics environment (from Filament's lightroom_14b asset) is applied
  automatically so PBR materials look reasonable out of the box.
- **User ambient**: Setting `ambientRadiance` on the renderer overrides the
  default IBL with a uniform ambient of the specified color and intensity.
- **Camera exposure**: Uses `setExposure(1.0)` by default for unit-less light
  intensity matching. Adjustable via a `"exposure"` extension parameter.

## Code Style

We follow [Filament's coding style](https://github.com/google/filament) for
consistency with the rendering backend.

- **C++ Standard**: C++20 (`/std:c++20` on MSVC, `-std=c++20` elsewhere).
- **Naming**:
  - Classes and structs: `PascalCase`
  - Functions and methods: `camelCase`
  - Member variables: `m` prefix + `PascalCase` (e.g., `mEngine`, `mRenderer`)
  - Constants and enums: `UPPER_SNAKE_CASE`
  - Namespaces: `PascalCase` (e.g., `AnariFilament`)
  - File names match class names: `Device.h`, `Device.cpp`
- **Formatting**: Prefer clang-format. Braces on same line for functions and
  control structures. 4-space indentation, no tabs.
- **Includes**: Group in order: related header, C++ standard library, external
  libraries (ANARI, Filament, Corrade), project headers. Separate groups with
  blank lines.
- **Closing braces**: Do not annotate closing braces with comments
  (no `} // namespace Foo`).
- **Error handling**: Use ANARI status callbacks for reporting errors to the
  application. Avoid exceptions in the rendering path.
- **Memory**: Follow Filament's ownership model — the Engine owns most objects.
  Use RAII where appropriate. Prefer `std::unique_ptr` for owned resources.

## Build System

- **Generator**: Ninja (`-GNinja`).
- **Compiler**: Clang on macOS/Linux, MSVC on Windows.
- **CMake minimum**: 3.22.
- **Build directory convention**: `build-ninja/` for local development.
- **Testing**: `ctest --output-on-failure` after building.
- **Warnings policy**: Zero warnings. Our targets compile with warnings-as-errors
  (`/W4 /WX` on MSVC, `-Wall -Wextra -Werror -Wpedantic` on GCC/Clang).
  External headers are silenced via MSVC `/external:anglebrackets /external:W0`
  and CMake `SYSTEM` include directories. All new code must compile without
  warnings on all platforms.

## Dependencies

| Dependency | Method | Notes |
|------------|--------|-------|
| Filament | Precompiled download | Auto-downloaded via CMake if missing |
| ANARI SDK | Git submodule + `add_subdirectory` | `external/anari-sdk` |
| Corrade | Git submodule + `add_subdirectory` | `external/corrade`, TestSuite only |

## Testing Guidelines

- Every ANARI object type should have corresponding tests.
- Use Corrade TestSuite macros: `CORRADE_VERIFY`, `CORRADE_COMPARE`.
- Test files go in `src/Test/` and are named `<Thing>Test.cpp`.
- Tests should be self-contained — create and destroy their own ANARI devices.
- **Screenshot tests** (`ScreenshotTest.cpp`) compare rendered output against
  golden PNG images in `src/Test/golden/`. When rendering changes (e.g. enabling
  shadows), regenerate goldens by copying the test outputs over and committing.
  Tolerance is `MAX_CHANNEL_DIFF = 90` per channel.

## Blender Integration

The device works with the [ANARI Blender addon](https://github.com/KhronosGroup/anari-blender-addon)
(`CustomAnariRender.py`). Key integration notes:

- **DLL deployment**: Copy `anari_library_filament.dll` (and any dependencies)
  to Blender's `scripts/modules/anari/` directory.
- **Compatibility shim**: The addon calls `anariLoadLibrary()` which was renamed
  to `loadLibrary()` in newer SDKs. A `compat.py` shim in the anari module
  provides the old function names.
- **Coordinate system**: Blender uses Z-up, Filament uses Y-up. The addon
  extracts camera orientation from `matrix_world` and sends it as ANARI
  `position`/`direction`/`up` parameters. No coordinate conversion is needed
  in the device — we pass through to `Camera::lookAt()`.
- **Normals**: The addon should use `mesh.corner_normals.foreach_get('vector')`
  (Blender 4.1+) instead of the deprecated `mesh.loops.foreach_get('normal')`.
- **Unindexed geometry**: Blender often sends flattened per-loop vertices with
  no index buffer. Our `commitTriangle()` generates sequential indices in this
  case.

## CI / Merge Request Workflow

- CI runs on GitLab. Merge requests are created via `glab mr create`.
- All MRs must pass CI before merge.
- Target platforms: Windows x86_64, macOS arm64, Linux x86_64/arm64,
  iOS arm64 (build-only), Android arm64 (build-only).
- **Compiler preference**: Clang everywhere possible. MSVC on Windows only.
