#!/usr/bin/env bash
# Build the minimal Halogen Android native-surface test APK without Gradle.
#
# Required environment:
#   ANDROID_SDK   - path to Android SDK (must have build-tools + platforms/android-34)
#   ANDROID_NDK   - path to Android NDK r27c or later
#   ANARI_LIB_DIR   - dir containing libanari.so (arm64-v8a)
#   HALOGEN_LIB_DIR - dir containing libanari_library_halogen.so (arm64-v8a)
#   ANARI_INCLUDE_DIR - dir containing anari/anari.h
#
# Produces: build/halogen-surface-test-signed.apk

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
APK_STAGE="$BUILD_DIR/stage"
ABI=arm64-v8a
API_LEVEL=26

: "${ANDROID_SDK:?ANDROID_SDK must be set}"
: "${ANDROID_NDK:?ANDROID_NDK must be set}"
: "${ANARI_LIB_DIR:?ANARI_LIB_DIR must be set (dir with libanari.so)}"
: "${HALOGEN_LIB_DIR:?HALOGEN_LIB_DIR must be set (dir with libanari_library_halogen.so)}"
: "${ANARI_INCLUDE_DIR:?ANARI_INCLUDE_DIR must be set (dir with anari/anari.h)}"

BUILD_TOOLS="${BUILD_TOOLS:-$ANDROID_SDK/build-tools/$(ls "$ANDROID_SDK/build-tools" | sort -V | tail -1)}"
PLATFORM_JAR="$ANDROID_SDK/platforms/android-34/android.jar"

# Android SDK build-tools ship Windows batch-file wrappers for the JVM-based
# tools (apksigner, d8). Detect and prefer them on MSYS/Cygwin.
_bt () {
   if   [ -f "$BUILD_TOOLS/$1" ]; then echo "$BUILD_TOOLS/$1"
   elif [ -f "$BUILD_TOOLS/$1.bat" ]; then echo "$BUILD_TOOLS/$1.bat"
   else echo "$BUILD_TOOLS/$1"
   fi
}
APKSIGNER="$(_bt apksigner)"

rm -rf "$BUILD_DIR"
mkdir -p "$APK_STAGE/lib/$ABI"

TOOLCHAIN="$ANDROID_NDK/toolchains/llvm/prebuilt"
HOST_DIR=$(ls "$TOOLCHAIN" | head -1)
CLANG="$TOOLCHAIN/$HOST_DIR/bin/clang++"
SYSROOT="$TOOLCHAIN/$HOST_DIR/sysroot"

# ---------------------------------------------------------------------------
# 1. Compile native_app_glue + our main.cpp into libhalogen_surface_test.so
# ---------------------------------------------------------------------------
echo "==> Compiling native lib..."
# Activity-based test: no native_app_glue, no ANativeActivity_onCreate.
# JNI calls from the Java MainActivity drive everything.
"$CLANG" \
    --target=aarch64-linux-android$API_LEVEL --sysroot="$SYSROOT" \
    -fPIC -std=gnu++17 -O2 -Wall \
    -I"$ANARI_INCLUDE_DIR" \
    -c "$SCRIPT_DIR/main.cpp" \
    -o "$BUILD_DIR/main.o"

"$CLANG" \
    --target=aarch64-linux-android$API_LEVEL --sysroot="$SYSROOT" \
    -shared \
    -Wl,--no-undefined \
    -Wl,-soname,libhalogen_surface_test.so \
    -o "$APK_STAGE/lib/$ABI/libhalogen_surface_test.so" \
    "$BUILD_DIR/main.o" \
    "$ANARI_LIB_DIR/libanari.so" \
    -landroid -llog

# ---------------------------------------------------------------------------
# Compile Java Activity + convert to DEX
# ---------------------------------------------------------------------------
echo "==> Compiling Java activity..."
CLASSES_DIR="$BUILD_DIR/classes"
mkdir -p "$CLASSES_DIR"
JAVAC="${JAVAC:-javac}"
find "$SCRIPT_DIR/java" -name "*.java" | xargs "$JAVAC" -encoding UTF-8 -source 1.8 -target 1.8 \
    -cp "$PLATFORM_JAR" -d "$CLASSES_DIR"

echo "==> d8 to DEX..."
D8="$(_bt d8)"
"$D8" --lib "$PLATFORM_JAR" --output "$APK_STAGE" $(find "$CLASSES_DIR" -name "*.class")

# libc++_shared for NDK c++_shared STL
cp "$SYSROOT/usr/lib/aarch64-linux-android/libc++_shared.so" "$APK_STAGE/lib/$ABI/"

# Prebuilt anari + halogen provider
cp "$ANARI_LIB_DIR/libanari.so" "$APK_STAGE/lib/$ABI/"
cp "$HALOGEN_LIB_DIR/libanari_library_halogen.so" "$APK_STAGE/lib/$ABI/"

# ---------------------------------------------------------------------------
# 2. aapt2 link (no resources needed, hasCode=false)
# ---------------------------------------------------------------------------
echo "==> Linking APK..."
"$BUILD_TOOLS/aapt2" link \
    -o "$APK_STAGE/unaligned.apk" \
    -I "$PLATFORM_JAR" \
    --manifest "$SCRIPT_DIR/AndroidManifest.xml" \
    --min-sdk-version $API_LEVEL --target-sdk-version 34 \
    --version-code 1 --version-name "1.0"

# ---------------------------------------------------------------------------
# 3. Add native libs (jar works cross-platform unlike GNU zip)
# ---------------------------------------------------------------------------
echo "==> Adding native libs + classes.dex..."
( cd "$APK_STAGE" && jar uf unaligned.apk lib classes.dex )

# ---------------------------------------------------------------------------
# 4. Align + sign
# ---------------------------------------------------------------------------
echo "==> Aligning + signing..."
ALIGNED="$BUILD_DIR/halogen-surface-test-aligned.apk"
"$BUILD_TOOLS/zipalign" -f 4 "$APK_STAGE/unaligned.apk" "$ALIGNED"

KEYSTORE="$BUILD_DIR/debug.keystore"
keytool -genkeypair -keystore "$KEYSTORE" -alias debug \
    -keyalg RSA -keysize 2048 -validity 3650 \
    -storepass android -keypass android \
    -dname "CN=Halogen Surface Test,O=Metaversal,C=US" 2>/dev/null

SIGNED="$BUILD_DIR/halogen-surface-test-signed.apk"
"$APKSIGNER" sign \
    --ks "$KEYSTORE" --ks-key-alias debug \
    --ks-pass pass:android --key-pass pass:android \
    --out "$SIGNED" "$ALIGNED"

echo ""
echo "Done: $SIGNED"
echo "Install: adb install -r \"$SIGNED\""
echo "Launch:  adb shell am start -n com.metaversal.halogen.surfacetest/android.app.NativeActivity"
echo "Logs:    adb logcat -s HalogenSurfaceTest:V Filament:V DEBUG:V libc:F"
