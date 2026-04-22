# Halogen Android native-surface smoke test

Minimal Android `NativeActivity` that exercises the `HALOGEN_NATIVE_SURFACE`
extension path with no other dependencies (no SDL, no GLFW, no ANARI client
code). It:

1. Receives `APP_CMD_INIT_WINDOW` from the NDK.
2. `setenv(FILAMENT_BACKEND=vulkan)` — forces Filament Vulkan on Android.
3. `anariLoadLibrary("halogen")` + `anariNewDevice("default")` + commit.
4. Queries `HALOGEN_NATIVE_SURFACE` extension availability.
5. `anariNewObject("nativeSurface")` + `nativeWindow = ANativeWindow*` + commit.

Logs every step to logcat tag `HalogenSurfaceTest`, so a crash is
immediately bisected from the last surviving step.

## Build

Non-Gradle. Requires:
- Android SDK with `build-tools/` (any recent) + `platforms/android-34/android.jar`
- Android NDK r27c or later
- Prebuilt `libanari.so` + `libanari_library_halogen.so` for `arm64-v8a`
- `anari/anari.h` header

```bash
export ANDROID_SDK=...
export ANDROID_NDK=...
export HALOGEN_LIBS_DIR=<dir containing libanari*.so>
export HALOGEN_INCLUDE_DIR=<dir containing anari/anari.h>
./build-apk.sh
```

## Run

```bash
adb install -r build/halogen-surface-test-signed.apk
adb shell am start -n com.metaversal.halogen.surfacetest/android.app.NativeActivity
adb logcat -s HalogenSurfaceTest:V Filament:V DEBUG:V libc:F
```

Happy path reaches `step 9: clean shutdown`. Any other terminal log line
pinpoints where Halogen's Vulkan-on-Android setup fails.
