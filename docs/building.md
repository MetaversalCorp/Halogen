# Building Halogen

## Prerequisites

| Tool | Version | Notes |
|------|---------|-------|
| CMake | ≥ 3.22 | [cmake.org](https://cmake.org/download/) |
| Ninja | Latest | [ninja-build.org](https://ninja-build.org/) |
| C++20 Compiler | See below | Platform-specific |
| Git | Latest | For submodules |

### Platform-specific compilers

- **Windows**: Visual Studio 2022 (MSVC v143)
- **macOS**: Xcode Command Line Tools (Apple Clang)
- **Linux**: Clang 14+ or GCC 12+

## Getting the Source

```bash
git clone --recursive https://github.com/MetaversalCorp/Halogen.git
cd Halogen
```

If you already cloned without `--recursive`:

```bash
git submodule update --init --recursive
```

## Filament SDK

Precompiled Filament libraries are **downloaded automatically** during the CMake
configure step if not found at `external/filament/`. No manual action needed.

To use a custom Filament build, set `FILAMENT_SDK_DIR`:

```bash
cmake -B build-ninja -G Ninja -DFILAMENT_SDK_DIR=/path/to/your/filament
```

To change the Filament version:

```bash
cmake -B build-ninja -G Ninja -DFILAMENT_VERSION=1.71.0
```

## Building

### macOS / Linux

```bash
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
```

### Windows

Open a **Developer PowerShell for VS 2022**, or manually load the MSVC
environment:

```powershell
. scripts/vcvarsall.ps1
cmake -B build-ninja -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-ninja
```

### Build Types

| Type | Use |
|------|-----|
| `Debug` | Development, full debug symbols |
| `Release` | Optimized, for benchmarks and distribution |
| `RelWithDebInfo` | Optimized with debug info |

## Running Tests

```bash
ctest --test-dir build-ninja --output-on-failure
```

Or run the test executable directly:

```bash
./build-ninja/DeviceTest
```

## Project Layout

```
CMakeLists.txt            Root build file
├── cmake/
│   └── DownloadFilament.cmake   Auto-download logic
├── external/
│   ├── anari-sdk/         ANARI SDK (submodule)
│   ├── corrade/           Corrade (submodule, testing only)
│   └── filament/          Downloaded automatically (git-ignored)
├── src/
│   ├── CMakeLists.txt     Library + test targets
│   ├── Device.h/.cpp      ANARI device → Filament
│   ├── Library.cpp        ANARI library entry point
│   └── Test/
│       └── DeviceTest.cpp Corrade TestSuite tests
└── filament-sdk/          Downloaded automatically (git-ignored)
```

## Troubleshooting

### Filament download fails

If behind a proxy or firewall, download manually from
[GitHub Releases](https://github.com/google/filament/releases) and extract to
`filament-sdk/`.

### Ninja not found

Install via your package manager:
- **macOS**: `brew install ninja`
- **Linux**: `sudo apt install ninja-build`
- **Windows**: `winget install Ninja-build.Ninja` or via
  [Visual Studio Installer](https://visualstudio.microsoft.com/)

### MSVC environment not set up

On Windows, make sure you run from a VS Developer PowerShell or source the
environment first with `scripts/vcvarsall.ps1`.
