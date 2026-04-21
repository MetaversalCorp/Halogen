// Copyright 2026 Metaversal Corporation
// SPDX-License-Identifier: MIT
//
// Minimal Android NativeActivity that exercises Halogen's HALOGEN_NATIVE_SURFACE
// extension: no SDL, no windowing-toolkit, no secondary graphics dependencies.
// Waits for the first APP_CMD_INIT_WINDOW, creates a Halogen ANARI device,
// forces the Vulkan backend, and builds a nativeSurface object from the
// NativeActivity's ANativeWindow*. Logs status at every step so crashes can
// be bisected from logcat alone.
//
// Useful as a reproducer for investigations into Halogen's Android Vulkan
// path without the rest of an application's memory/lib footprint.

#include <android/log.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <anari/anari.h>

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

void TryHalogen (ANativeWindow* window)
{
   LOGI ("step 1: setenv FILAMENT_BACKEND=vulkan");
   setenv ("FILAMENT_BACKEND", "vulkan", 1);

   LOGI ("step 2: anariLoadLibrary(\"halogen\")");
   ANARILibrary lib = anariLoadLibrary ("halogen", AnariStatus, nullptr);
   if (!lib) { LOGE ("  -> failed"); return; }

   LOGI ("step 3: anariNewDevice(\"default\")");
   ANARIDevice dev = anariNewDevice (lib, "default");
   if (!dev) { LOGE ("  -> failed"); anariUnloadLibrary (lib); return; }

   LOGI ("step 4: anariCommitParameters(device)");
   anariCommitParameters (dev, dev);

   const char* const* exts = anariGetDeviceExtensions (lib, "default");
   bool const hasNativeSurface = HasExtension (exts, "HALOGEN_NATIVE_SURFACE");
   LOGI ("step 5: HALOGEN_NATIVE_SURFACE advertised = %d", hasNativeSurface);
   if (!hasNativeSurface) { anariRelease (dev, dev); anariUnloadLibrary (lib); return; }

   LOGI ("step 6: anariNewObject(\"nativeSurface\") with ANativeWindow=%p", window);
   ANARIObject ns = anariNewObject (dev, "nativeSurface", "default");
   if (!ns) { LOGE ("  -> failed"); anariRelease (dev, dev); anariUnloadLibrary (lib); return; }

   void* windowVoid = window;
   anariSetParameter (dev, ns, "nativeWindow", ANARI_VOID_POINTER, &windowVoid);

   LOGI ("step 7: anariCommitParameters(nativeSurface)  <-- crash expected here");
   anariCommitParameters (dev, ns);

   LOGI ("step 8: survived commit; releasing");
   anariRelease (dev, ns);
   anariRelease (dev, dev);
   anariUnloadLibrary (lib);
   LOGI ("step 9: clean shutdown");
}

void OnAppCmd (android_app* app, int32_t cmd)
{
   if (cmd == APP_CMD_INIT_WINDOW && app->window != nullptr)
   {
      LOGI ("APP_CMD_INIT_WINDOW: window = %p, %dx%d",
         app->window, ANativeWindow_getWidth (app->window),
         ANativeWindow_getHeight (app->window));
      TryHalogen (app->window);
   }
}

} // namespace

extern "C" void android_main (android_app* app)
{
   app->onAppCmd = OnAppCmd;
   while (!app->destroyRequested)
   {
      android_poll_source* source = nullptr;
      int events = 0;
      if (ALooper_pollOnce (-1, nullptr, &events, (void**) &source) >= 0)
      {
         if (source) source->process (app, source);
      }
   }
}
