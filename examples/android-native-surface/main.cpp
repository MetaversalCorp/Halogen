// Copyright 2026 Metaversal Corporation
// SPDX-License-Identifier: MIT
//
// Minimal Android Activity + SurfaceView + JNI test for Halogen's
// HALOGEN_NATIVE_SURFACE extension. Mirrors the structure of Filament's own
// filament-jni / gltf-viewer sample so the two can be compared apples-to-
// apples. SurfaceView callbacks dispatch through JNI into this file; the
// Surface object is converted to an ANativeWindow via ANativeWindow_fromSurface
// and handed to Halogen exactly as filament-jni does.

#include <android/log.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>

#include <anari/anari.h>

#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#define LOGI(fmt, ...) __android_log_print (ANDROID_LOG_INFO,  "HalogenSurfaceTest", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print (ANDROID_LOG_ERROR, "HalogenSurfaceTest", fmt, ##__VA_ARGS__)

namespace {

void AnariStatus (const void*, ANARIDevice, ANARIObject, ANARIDataType,
   ANARIStatusSeverity severity, ANARIStatusCode, const char* msg)
{
   if (severity <= ANARI_SEVERITY_WARNING) LOGE ("[anari] %s", msg);
   else                                     LOGI ("[anari] %s", msg);
}

bool HasExtension (const char* const* list, const char* name)
{
   if (!list || !name) return false;
   for (const char* const* p = list; *p; ++p)
      if (std::strcmp (*p, name) == 0) return true;
   return false;
}

struct Scene
{
   ANARILibrary  lib = nullptr;
   ANARIDevice   device = nullptr;
   ANARIObject   nativeSurface = nullptr;
   ANARIGeometry sphereGeom = nullptr;
   ANARIMaterial sphereMat = nullptr;
   ANARISurface  sphereSurf = nullptr;
   ANARIWorld    world = nullptr;
   ANARILight    light = nullptr;
   ANARICamera   camera = nullptr;
   ANARIRenderer renderer = nullptr;
   ANARIFrame    frame = nullptr;
   bool          valid = false;
};

struct AppState
{
   Scene             scene;
   ANativeWindow*    window   = nullptr;
   std::atomic<bool> running { false };
   std::atomic<bool> paused  { true };
   float             time     = 0.0f;
   int64_t           frameCount = 0;
};

AppState g_app;

bool InitScene (Scene& s, ANativeWindow* window, uint32_t w, uint32_t h)
{
   LOGI ("step 1: setenv FILAMENT_BACKEND=vulkan");
   setenv ("FILAMENT_BACKEND", "vulkan", 1);

   LOGI ("step 2: anariLoadLibrary(\"halogen\")");
   s.lib = anariLoadLibrary ("halogen", AnariStatus, nullptr);
   if (!s.lib) { LOGE ("  -> failed"); return false; }

   LOGI ("step 3: anariNewDevice(\"default\")");
   s.device = anariNewDevice (s.lib, "default");
   if (!s.device) { LOGE ("  -> failed"); return false; }

   LOGI ("step 4: anariCommitParameters(device)");
   anariCommitParameters (s.device, s.device);

   const char* const* exts = anariGetDeviceExtensions (s.lib, "default");
   if (!HasExtension (exts, "HALOGEN_NATIVE_SURFACE"))
   { LOGE ("step 5: HALOGEN_NATIVE_SURFACE NOT advertised"); return false; }
   LOGI ("step 5: HALOGEN_NATIVE_SURFACE advertised");

   LOGI ("step 6: nativeSurface from ANativeWindow=%p", window);
   s.nativeSurface = anariNewObject (s.device, "nativeSurface", "default");
   if (!s.nativeSurface) { LOGE ("  -> failed"); return false; }
   // Unlike other ANARI types, ANARI_VOID_POINTER takes the pointer value
   // directly as the 5th arg to anariSetParameter — NOT a pointer to it. See
   // anari_cpp_impl.hpp:530 which dereferences one level for VOID_POINTER.
   anariSetParameter (s.device, s.nativeSurface, "nativeWindow", ANARI_VOID_POINTER, window);
   anariCommitParameters (s.device, s.nativeSurface);

   LOGI ("step 7: sphere + material + surface");
   float spherePos[] = { 0.0f, 0.0f, 0.0f };
   float sphereRad = 1.0f;
   ANARIArray1D posArr = anariNewArray1D (s.device, spherePos, nullptr, nullptr, ANARI_FLOAT32_VEC3, 1);
   s.sphereGeom = anariNewGeometry (s.device, "sphere");
   anariSetParameter (s.device, s.sphereGeom, "vertex.position", ANARI_ARRAY1D, &posArr);
   anariSetParameter (s.device, s.sphereGeom, "radius", ANARI_FLOAT32, &sphereRad);
   anariCommitParameters (s.device, s.sphereGeom);
   anariRelease (s.device, posArr);

   s.sphereMat = anariNewMaterial (s.device, "physicallyBased");
   float baseColor[3] = { 0.9f, 0.3f, 0.2f };
   float metallic = 0.1f, roughness = 0.4f;
   anariSetParameter (s.device, s.sphereMat, "baseColor", ANARI_FLOAT32_VEC3, baseColor);
   anariSetParameter (s.device, s.sphereMat, "metallic",  ANARI_FLOAT32, &metallic);
   anariSetParameter (s.device, s.sphereMat, "roughness", ANARI_FLOAT32, &roughness);
   anariCommitParameters (s.device, s.sphereMat);

   s.sphereSurf = anariNewSurface (s.device);
   anariSetParameter (s.device, s.sphereSurf, "geometry", ANARI_GEOMETRY, &s.sphereGeom);
   anariSetParameter (s.device, s.sphereSurf, "material", ANARI_MATERIAL, &s.sphereMat);
   anariCommitParameters (s.device, s.sphereSurf);

   LOGI ("step 8: world + directional light");
   s.world = anariNewWorld (s.device);
   ANARIArray1D surfArr = anariNewArray1D (s.device, &s.sphereSurf, nullptr, nullptr, ANARI_SURFACE, 1);
   anariSetParameter (s.device, s.world, "surface", ANARI_ARRAY1D, &surfArr);
   anariRelease (s.device, surfArr);

   s.light = anariNewLight (s.device, "directional");
   float ldir[3]   = { -0.3f, -1.0f, -0.5f };
   float lcolor[3] = { 1.0f, 1.0f, 1.0f };
   float irr = 3.0f;
   anariSetParameter (s.device, s.light, "direction",  ANARI_FLOAT32_VEC3, ldir);
   anariSetParameter (s.device, s.light, "color",      ANARI_FLOAT32_VEC3, lcolor);
   anariSetParameter (s.device, s.light, "irradiance", ANARI_FLOAT32, &irr);
   anariCommitParameters (s.device, s.light);
   ANARIArray1D lightArr = anariNewArray1D (s.device, &s.light, nullptr, nullptr, ANARI_LIGHT, 1);
   anariSetParameter (s.device, s.world, "light", ANARI_ARRAY1D, &lightArr);
   anariRelease (s.device, lightArr);
   anariCommitParameters (s.device, s.world);

   LOGI ("step 9: camera + renderer + frame");
   s.camera = anariNewCamera (s.device, "perspective");
   float cp[3] = { 0.0f, 0.0f, 3.0f };
   float cd[3] = { 0.0f, 0.0f, -1.0f };
   float cu[3] = { 0.0f, 1.0f,  0.0f };
   float aspect = float (w) / float (h);
   float fovY = 0.9f;
   anariSetParameter (s.device, s.camera, "position",  ANARI_FLOAT32_VEC3, cp);
   anariSetParameter (s.device, s.camera, "direction", ANARI_FLOAT32_VEC3, cd);
   anariSetParameter (s.device, s.camera, "up",        ANARI_FLOAT32_VEC3, cu);
   anariSetParameter (s.device, s.camera, "aspect",    ANARI_FLOAT32, &aspect);
   anariSetParameter (s.device, s.camera, "fovy",      ANARI_FLOAT32, &fovY);
   anariCommitParameters (s.device, s.camera);

   s.renderer = anariNewRenderer (s.device, "default");
   float bg[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
   anariSetParameter (s.device, s.renderer, "background", ANARI_FLOAT32_VEC4, bg);
   anariCommitParameters (s.device, s.renderer);

   s.frame = anariNewFrame (s.device);
   uint32_t size[2] = { w, h };
   anariSetParameter (s.device, s.frame, "size", ANARI_UINT32_VEC2, size);
   ANARIDataType ct = ANARI_UFIXED8_RGBA_SRGB;
   anariSetParameter (s.device, s.frame, "channel.color", ANARI_DATA_TYPE, &ct);
   anariSetParameter (s.device, s.frame, "world",         ANARI_WORLD,    &s.world);
   anariSetParameter (s.device, s.frame, "camera",        ANARI_CAMERA,   &s.camera);
   anariSetParameter (s.device, s.frame, "renderer",      ANARI_RENDERER, &s.renderer);
   anariSetParameter (s.device, s.frame, "nativeSurface", ANARI_OBJECT,   &s.nativeSurface);
   anariCommitParameters (s.device, s.frame);

   LOGI ("step 10: ready to render");
   s.valid = true;
   return true;
}

void TeardownScene (Scene& s)
{
   if (s.frame)         anariRelease (s.device, s.frame);
   if (s.renderer)      anariRelease (s.device, s.renderer);
   if (s.camera)        anariRelease (s.device, s.camera);
   if (s.light)         anariRelease (s.device, s.light);
   if (s.world)         anariRelease (s.device, s.world);
   if (s.sphereSurf)    anariRelease (s.device, s.sphereSurf);
   if (s.sphereMat)     anariRelease (s.device, s.sphereMat);
   if (s.sphereGeom)    anariRelease (s.device, s.sphereGeom);
   if (s.nativeSurface) anariRelease (s.device, s.nativeSurface);
   if (s.device)        anariRelease (s.device, s.device);
   if (s.lib)           anariUnloadLibrary (s.lib);
   s = {};
}

// Filament's JobSystem requires all Engine work to happen on the thread that
// created the Engine. Our Engine is created on the UI thread (the thread that
// invokes surfaceCreated JNI), so rendering must happen there too. Java side
// drives nativeDoFrame() from a Choreographer.FrameCallback.
void DoFrame ()
{
   if (!g_app.running.load () || g_app.paused.load () || !g_app.scene.valid)
      return;

   float const t = g_app.time;
   float const r = 3.0f;
   float const x = r * __builtin_sinf (t * 0.6f);
   float const z = r * __builtin_cosf (t * 0.6f);
   float cp[3] = { x, 0.5f, z };
   float cd[3] = { -x, -0.5f, -z };
   anariSetParameter (g_app.scene.device, g_app.scene.camera, "position",  ANARI_FLOAT32_VEC3, cp);
   anariSetParameter (g_app.scene.device, g_app.scene.camera, "direction", ANARI_FLOAT32_VEC3, cd);
   anariCommitParameters (g_app.scene.device, g_app.scene.camera);

   if (g_app.frameCount < 3) LOGI ("-> frame %lld: before anariRenderFrame", (long long) g_app.frameCount);
   anariRenderFrame (g_app.scene.device, g_app.scene.frame);
   anariFrameReady  (g_app.scene.device, g_app.scene.frame, ANARI_WAIT);
   if (g_app.frameCount < 3) LOGI ("<- frame %lld: after anariRenderFrame",  (long long) g_app.frameCount);

   g_app.time += 1.0f / 60.0f;
   if ((++g_app.frameCount % 60) == 0)
      LOGI ("rendered %lld frames", (long long) g_app.frameCount);
}

} // namespace

// ---------------------------------------------------------------------------
// JNI entry points — called from Java/Kotlin Activity
// ---------------------------------------------------------------------------

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeSurfaceCreated (
   JNIEnv* env, jobject /*thiz*/, jobject surface)
{
   LOGI ("JNI: surfaceCreated");
   g_app.window = ANativeWindow_fromSurface (env, surface);
   if (!g_app.window) { LOGE ("  ANativeWindow_fromSurface returned null"); return; }

   int32_t w = ANativeWindow_getWidth (g_app.window);
   int32_t h = ANativeWindow_getHeight (g_app.window);
   LOGI ("  ANativeWindow=%p, %dx%d", g_app.window, w, h);

   if (!InitScene (g_app.scene, g_app.window, uint32_t (w), uint32_t (h)))
   { LOGE ("  InitScene failed"); return; }

   g_app.running = true;
   g_app.paused  = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeSurfaceChanged (
   JNIEnv*, jobject, jobject, jint format, jint width, jint height)
{
   LOGI ("JNI: surfaceChanged format=%d size=%dx%d", format, width, height);
}

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeSurfaceDestroyed (
   JNIEnv*, jobject)
{
   LOGI ("JNI: surfaceDestroyed");
   g_app.running = false;
   TeardownScene (g_app.scene);
   if (g_app.window) { ANativeWindow_release (g_app.window); g_app.window = nullptr; }
}

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeDoFrame (
   JNIEnv*, jobject)
{
   DoFrame ();
}

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeOnResume (JNIEnv*, jobject)
{
   LOGI ("JNI: onResume");
   g_app.paused = false;
}

extern "C" JNIEXPORT void JNICALL
Java_com_metaversal_halogen_surfacetest_MainActivity_nativeOnPause (JNIEnv*, jobject)
{
   LOGI ("JNI: onPause");
   g_app.paused = true;
}
