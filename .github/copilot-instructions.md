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
- **External dependencies** (`external/`): Git submodules for ANARI SDK and
  Corrade (testing).
- **Filament**: Used as precompiled libraries, downloaded automatically during
  the CMake configure step if not found locally.
- **Tests** (`src/Test/`): Written using Corrade TestSuite.
- **CI**: GitLab CI targeting Windows (MSVC), macOS (Clang), with plans for
  Linux, Android, and iOS.

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

## CI / Merge Request Workflow

- CI runs on GitLab. Merge requests are created via `glab mr create`.
- All MRs must pass CI before merge.
- Target platforms: Windows x86_64, macOS x86_64/arm64 (more planned).
