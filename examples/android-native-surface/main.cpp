// Copyright 2026 Metaversal Corporation
// SPDX-License-Identifier: MIT
//
// Minimal Android NativeActivity that exercises Halogen's HALOGEN_NATIVE_SURFACE
// extension: no SDL, no windowing-toolkit, no secondary graphics dependencies.
// Waits for the first APP_CMD_INIT_WINDOW, creates a Halogen ANARI device,
// forces the Vulkan backend, builds a nativeSurface from the NativeActivity's
// ANativeWindow*, attaches it to a frame, and spins a render loop that clears
// to a cycling solid color so the screen visibly reflects live rendering.
// Logs status at every init step so crashes bisect from logcat alone.
//
// Useful as a reproducer for investigations into Halogen's Android Vulkan
// path without the rest of an application's memory/lib footprint.

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <anari/anari.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#define LOGI(fmt, ...) __android_log_print (ANDROID_LOG_INFO,  "HalogenSurfaceTest", fmt, ##__VA_ARGS__)
#define LOGE(fmt, ...) __android_log_print (ANDROID_LOG_ERROR, "HalogenSurfaceTest", fmt, ##__VA_ARGS__)

namespace {

void AnariStatus (const void*, ANARIDevice, ANARIObject, ANARIDataType,
   ANARIStatusSeverity severity, ANARIStatusCode, const char* msg)
{
   if (severity <= ANARI_SEVERITY_WARNING)
      LOGE ("[anari] %s", msg);
   else
      LOGI ("[anari] %s", msg);
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
   bool const hasNativeSurface = HasExtension (exts, "HALOGEN_NATIVE_SURFACE");
   LOGI ("step 5: HALOGEN_NATIVE_SURFACE advertised = %d", hasNativeSurface);
   if (!hasNativeSurface) return false;

   LOGI ("step 6: anariNewObject(\"nativeSurface\") with ANativeWindow=%p", window);
   s.nativeSurface = anariNewObject (s.device, "nativeSurface", "default");
   if (!s.nativeSurface) { LOGE ("  -> failed"); return false; }
   void* windowVoid = window;
   anariSetParameter (s.device, s.nativeSurface, "nativeWindow", ANARI_VOID_POINTER, &windowVoid);

   LOGI ("step 7: anariCommitParameters(nativeSurface)");
   anariCommitParameters (s.device, s.nativeSurface);

   LOGI ("step 8: one sphere at origin");
   float spherePos[] = { 0.0f, 0.0f, 0.0f };
   float sphereRad   = 1.0f;
   ANARIArray1D posArr = anariNewArray1D (s.device, spherePos, nullptr, nullptr, ANARI_FLOAT32_VEC3, 1);

   s.sphereGeom = anariNewGeometry (s.device, "sphere");
   anariSetParameter (s.device, s.sphereGeom, "vertex.position", ANARI_ARRAY1D, &posArr);
   anariSetParameter (s.device, s.sphereGeom, "radius", ANARI_FLOAT32, &sphereRad);
   anariCommitParameters (s.device, s.sphereGeom);
   anariRelease (s.device, posArr);

   s.sphereMat = anariNewMaterial (s.device, "physicallyBased");
   float baseColor[3] = { 0.9f, 0.3f, 0.2f };
   float metallic  = 0.1f;
   float roughness = 0.4f;
   anariSetParameter (s.device, s.sphereMat, "baseColor", ANARI_FLOAT32_VEC3, baseColor);
   anariSetParameter (s.device, s.sphereMat, "metallic",  ANARI_FLOAT32, &metallic);
   anariSetParameter (s.device, s.sphereMat, "roughness", ANARI_FLOAT32, &roughness);
   anariCommitParameters (s.device, s.sphereMat);

   s.sphereSurf = anariNewSurface (s.device);
   anariSetParameter (s.device, s.sphereSurf, "geometry", ANARI_GEOMETRY, &s.sphereGeom);
   anariSetParameter (s.device, s.sphereSurf, "material", ANARI_MATERIAL, &s.sphereMat);
   anariCommitParameters (s.device, s.sphereSurf);

   LOGI ("step 9: world + directional light");
   s.world = anariNewWorld (s.device);
   ANARIArray1D surfArr = anariNewArray1D (s.device, &s.sphereSurf, nullptr, nullptr, ANARI_SURFACE, 1);
   anariSetParameter (s.device, s.world, "surface", ANARI_ARRAY1D, &surfArr);
   anariRelease (s.device, surfArr);

   s.light = anariNewLight (s.device, "directional");
   float lightDir[3]   = { -0.3f, -1.0f, -0.5f };
   float lightColor[3] = { 1.0f, 1.0f, 1.0f };
   float irradiance = 3.0f;
   anariSetParameter (s.device, s.light, "direction", ANARI_FLOAT32_VEC3, lightDir);
   anariSetParameter (s.device, s.light, "color",     ANARI_FLOAT32_VEC3, lightColor);
   anariSetParameter (s.device, s.light, "irradiance", ANARI_FLOAT32, &irradiance);
   anariCommitParameters (s.device, s.light);

   ANARIArray1D lightArr = anariNewArray1D (s.device, &s.light, nullptr, nullptr, ANARI_LIGHT, 1);
   anariSetParameter (s.device, s.world, "light", ANARI_ARRAY1D, &lightArr);
   anariRelease (s.device, lightArr);
   anariCommitParameters (s.device, s.world);

   LOGI ("step 10: camera + renderer");
   s.camera = anariNewCamera (s.device, "perspective");
   float camPos[3] = { 0.0f, 0.0f, 3.0f };
   float camDir[3] = { 0.0f, 0.0f, -1.0f };
   float camUp[3]  = { 0.0f, 1.0f,  0.0f };
   float const aspect = float (w) / float (h);
   float const fovY   = 0.9f;
   anariSetParameter (s.device, s.camera, "position",  ANARI_FLOAT32_VEC3, camPos);
   anariSetParameter (s.device, s.camera, "direction", ANARI_FLOAT32_VEC3, camDir);
   anariSetParameter (s.device, s.camera, "up",        ANARI_FLOAT32_VEC3, camUp);
   anariSetParameter (s.device, s.camera, "aspect",    ANARI_FLOAT32, &aspect);
   anariSetParameter (s.device, s.camera, "fovy",      ANARI_FLOAT32, &fovY);
   anariCommitParameters (s.device, s.camera);

   s.renderer = anariNewRenderer (s.device, "default");
   float bg[4] = { 0.1f, 0.1f, 0.15f, 1.0f };
   anariSetParameter (s.device, s.renderer, "background", ANARI_FLOAT32_VEC4, bg);
   anariCommitParameters (s.device, s.renderer);

   LOGI ("step 11: frame + attach nativeSurface");
   s.frame = anariNewFrame (s.device);
   uint32_t size[2] = { w, h };
   anariSetParameter (s.device, s.frame, "size", ANARI_UINT32_VEC2, size);
   ANARIDataType colorType = ANARI_UFIXED8_RGBA_SRGB;
   anariSetParameter (s.device, s.frame, "channel.color", ANARI_DATA_TYPE, &colorType);
   anariSetParameter (s.device, s.frame, "world",         ANARI_WORLD,    &s.world);
   anariSetParameter (s.device, s.frame, "camera",        ANARI_CAMERA,   &s.camera);
   anariSetParameter (s.device, s.frame, "renderer",      ANARI_RENDERER, &s.renderer);
   anariSetParameter (s.device, s.frame, "nativeSurface", ANARI_OBJECT,   &s.nativeSurface);
   anariCommitParameters (s.device, s.frame);

   LOGI ("step 12: ready to render");
   s.valid = true;
   return true;
}

void RenderOneFrame (Scene& s, float t)
{
   // Orbit the camera slowly around the origin so the sphere's shading changes
   // and the screen is clearly alive.
   float const radius = 3.0f;
   float const speed  = 0.6f;
   float const x = radius * __builtin_sinf (t * speed);
   float const z = radius * __builtin_cosf (t * speed);
   float const camPos[3] = { x, 0.5f, z };
   float const camDir[3] = { -x, -0.5f, -z };  // looking at origin
   anariSetParameter (s.device, s.camera, "position",  ANARI_FLOAT32_VEC3, camPos);
   anariSetParameter (s.device, s.camera, "direction", ANARI_FLOAT32_VEC3, camDir);
   anariCommitParameters (s.device, s.camera);

   anariRenderFrame (s.device, s.frame);
   anariFrameReady  (s.device, s.frame, ANARI_WAIT);
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

struct AppState
{
   Scene scene;
   bool  initialized = false;
   float time = 0.0f;
};

void OnAppCmd (android_app* app, int32_t cmd)
{
   AppState& st = *static_cast<AppState*> (app->userData);
   if (cmd == APP_CMD_INIT_WINDOW && app->window != nullptr && !st.initialized)
   {
      int32_t w = ANativeWindow_getWidth (app->window);
      int32_t h = ANativeWindow_getHeight (app->window);
      LOGI ("APP_CMD_INIT_WINDOW: window = %p, %dx%d", app->window, w, h);
      st.initialized = InitScene (st.scene, app->window, uint32_t (w), uint32_t (h));
      LOGI ("onAppCmd: InitScene returned, st.initialized=%d", int (st.initialized));
   }
   else if (cmd == APP_CMD_TERM_WINDOW && st.initialized)
   {
      LOGI ("APP_CMD_TERM_WINDOW: tearing down");
      TeardownScene (st.scene);
      st.initialized = false;
   }
}

} // namespace

extern "C" void android_main (android_app* app)
{
   AppState st;
   app->userData = &st;
   app->onAppCmd = OnAppCmd;

   // Keep the screen on + keep our window visible; otherwise the default
   // NativeActivity window behaviour on some devices (Samsung S22 observed)
   // lets the display doze while we're mid-init, immediately stopping the
   // activity and invalidating the ANativeWindow before we can render a
   // frame. FLAG_KEEP_SCREEN_ON = 0x80; FLAG_TURN_SCREEN_ON = 0x200000.
   ANativeActivity_setWindowFlags (app->activity, 0x80 | 0x200000, 0);

   int64_t iter = 0;
   while (!app->destroyRequested)
   {
      if (iter < 5) LOGI ("main-loop iter=%lld, initialized=%d", (long long) iter, int (st.initialized));
      ++iter;
      for (;;) {
         android_poll_source* source = nullptr;
         int events = 0;
         int const timeoutMs = st.initialized ? 0 : -1;
         int const r = ALooper_pollOnce (timeoutMs, nullptr, &events, (void**) &source);
         if (r < 0) break;
         if (source) source->process (app, source);
         if (app->destroyRequested) break;
      }

      if (st.initialized && !app->destroyRequested)
      {
         static int64_t frameCount = 0;
         if (frameCount < 3)  LOGI ("-> frame %lld: before RenderOneFrame", (long long) frameCount);
         RenderOneFrame (st.scene, st.time);
         if (frameCount < 3)  LOGI ("<- frame %lld: after RenderOneFrame",  (long long) frameCount);
         st.time += 1.0f / 60.0f;
         if ((++frameCount % 60) == 0)
            LOGI ("rendered %lld frames (t = %.2f)", (long long) frameCount, st.time);
      }
   }

   if (st.initialized) TeardownScene (st.scene);
   LOGI ("android_main exit");
}
