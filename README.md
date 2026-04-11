# Filament ANARI Implementation

[![pipeline](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/badges/master/pipeline.svg)](https://gitlab.com/wonderland-gmbh/filament-anari-implementation/-/pipelines)
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
│   ├── corrade/            # Corrade (git submodule, testing only)
│   └── filament/           # Filament SDK (auto-downloaded, gitignored)
├── cmake/                  # CMake helpers (Filament download, etc.)
├── scripts/                # Build helper scripts
└── src/
    ├── Device.h/cpp        # ANARI device implementation
    ├── Library.cpp         # ANARI library entry point
    └── Test/               # Corrade TestSuite tests
```

## Roadmap

See [ROADMAP.md](ROADMAP.md) for the full feature plan. Current status:

- **Milestone 0** ✅ Device skeleton, CI on all platforms
- **Milestone 1** 🔄 Tutorial — triangle, matte material, camera, light
- **Milestone 2–6** Planned — test scenes, viewer, Blender addon

## Documentation

See [docs/building.md](docs/building.md) for detailed build instructions.

## License

MIT — see [LICENSE](LICENSE).
- `backend`, Filament render backend library
- `ibl`, Image-based lighting support library
- `utils`, Support library for Filament
- `geometry`, Geometry helper library for Filament
- `smol-v`, SPIR-V compression library, used only with Vulkan support

To use Filament from Java you must use the following two libraries instead:
- `filament-java.jar`, Contains Filament's Java classes
- `filament-jni`, Filament's JNI bindings

To link against debug builds of Filament, you must also link against:

- `matdbg`, Support library that adds an interactive web-based debugger to Filament

To use the Vulkan backend on macOS you must install the LunarG SDK, enable "System Global
Components", and reboot your machine.

The easiest way to install those files is to use the macOS
[LunarG Vulkan SDK](https://www.lunarg.com/vulkan-sdk/) installer.

## Linking against Filament

This walkthrough will get you successfully compiling and linking native code
against Filament with minimum dependencies.

To start, download Filament's [latest binary release](https://github.com/google/filament/releases)
and extract into a directory of your choosing. Binary releases are suffixed
with the platform name, for example, `filament-20181009-linux.tgz`.

Create a file, `main.cpp`, in the same directory with the following contents:

```c++
#include <filament/FilamentAPI.h>
#include <filament/Engine.h>

using namespace filament;

int main(int argc, char** argv)
{
    Engine *engine = Engine::create();
    engine->destroy(&engine);
    return 0;
}
```

The directory should look like:

```
|-- README.md
|-- bin
|-- docs
|-- include
|-- lib
|-- main.cpp
```

We'll use a platform-specific Makefile to compile and link `main.cpp` with Filament's libraries.
Copy your platform's Makefile below into a `Makefile` inside the same directory.

### Linux

```make
FILAMENT_LIBS=-lfilament -lbackend -lbluegl -lbluevk -lfilabridge -lfilaflat -lutils -lgeometry -lsmol-v -libl -labseil
CC=clang++

main: main.o
	$(CC) -Llib/x86_64/ main.o $(FILAMENT_LIBS) -lpthread -lc++ -ldl -o main

main.o: main.cpp
	$(CC) -Iinclude/ -std=c++20 -pthread -c main.cpp

clean:
	rm -f main main.o

.PHONY: clean
```

### macOS

```make
FILAMENT_LIBS=-lfilament -lbackend -lbluegl -lbluevk -lfilabridge -lfilaflat -lutils -lgeometry -lsmol-v -libl -labseil
FRAMEWORKS=-framework Cocoa -framework Metal -framework CoreVideo
CC=clang++
ARCH ?= $(shell uname -m)

main: main.o
	$(CC) -Llib/$(ARCH)/ main.o $(FILAMENT_LIBS) $(FRAMEWORKS) -o main

main.o: main.cpp
	$(CC) -Iinclude/ -std=c++20 -c main.cpp

clean:
	rm -f main main.o

.PHONY: clean
```

### Windows

Note that the static libraries distributed for Windows include several
variants: mt, md, mtd, mdd. These correspond to the [run-time library
flags](https://docs.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library?view=vs-2017)
`/MT`, `/MD`, `/MTd`, and `/MDd`, respectively. Here we use the mt variant. For the debug variants,
be sure to also include `matdbg.lib` in `FILAMENT_LIBS`.

When building Filament from source, the `USE_STATIC_CRT` CMake option can be
used to change the run-time library version.

```make
FILAMENT_LIBS=filament.lib backend.lib bluegl.lib bluevk.lib filabridge.lib filaflat.lib \
              utils.lib geometry.lib smol-v.lib ibl.lib abseil.lib
CC=cl.exe

main.exe: main.obj
	$(CC) main.obj /link /libpath:"lib\\x86_64\\mt\\" $(FILAMENT_LIBS) \
	gdi32.lib user32.lib opengl32.lib

main.obj: main.cpp
	$(CC) /MT /Iinclude\\ /std:c++20 /c main.cpp

clean:
	del main.exe main.obj

.PHONY: clean
```

### Compiling

You should be able to invoke `make` and run the executable successfully:

```
$ make
$ ./main
FEngine (64 bits) created at 0x106471000 (threading is enabled)
```

On Windows, you'll need to open up a Visual Studio Native Tools Command Prompt
and invoke `nmake` instead of `make`.


### Generating C++ documentation

To generate the documentation you must first install `doxygen` and `graphviz`, then run the
following commands:

```shell
cd filament/filament
doxygen docs/doxygen/filament.doxygen
```

Finally simply open `docs/html/index.html` in your web browser.
