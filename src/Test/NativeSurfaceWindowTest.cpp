// Copyright 2026 Jonathan Hale
// SPDX-License-Identifier: MIT

#include <Corrade/TestSuite/Tester.h>
#include <Corrade/Utility/Debug.h>

#include <anari/anari.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#endif
#include <GLFW/glfw3native.h>

namespace {

struct DeviceFixture {
    ANARILibrary library = nullptr;
    ANARIDevice device = nullptr;

    DeviceFixture(const char *backend = nullptr) {
        auto statusFunc = [](const void *, ANARIDevice, ANARIObject,
            ANARIDataType, ANARIStatusSeverity severity, ANARIStatusCode,
            const char *message) {
            if (severity <= ANARI_SEVERITY_WARNING)
                Corrade::Utility::Debug{} << "ANARI:" << message;
        };
        library = anariLoadLibrary("halogen", statusFunc);
        if (library)
            device = anariNewDevice(library, "default");
        if (device) {
            if (backend)
                anariSetParameter(device, device,
                    "backend", ANARI_STRING, backend);
            anariCommitParameters(device, device);
        }
    }

    ~DeviceFixture() {
        if (device) anariRelease(device, device);
        if (library) anariUnloadLibrary(library);
    }
};

void *getNativeWindow(GLFWwindow *window) {
#ifdef _WIN32
    return static_cast<void *>(glfwGetWin32Window(window));
#elif defined(__APPLE__)
    return static_cast<void *>(glfwGetCocoaWindow(window));
#else
    return nullptr;
#endif
}

struct NativeSurfaceWindowTest : Corrade::TestSuite::Tester {
    explicit NativeSurfaceWindowTest();
    ~NativeSurfaceWindowTest();

    void renderToWindow();

private:
    bool mGlfwInitialized = false;
};

NativeSurfaceWindowTest::NativeSurfaceWindowTest() {
    mGlfwInitialized = glfwInit() == GLFW_TRUE;
    addTests({&NativeSurfaceWindowTest::renderToWindow});
}

NativeSurfaceWindowTest::~NativeSurfaceWindowTest() {
    if (mGlfwInitialized)
        glfwTerminate();
}

void NativeSurfaceWindowTest::renderToWindow() {
    CORRADE_VERIFY(mGlfwInitialized);

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    GLFWwindow *window = glfwCreateWindow(64, 64,
        "NativeSurfaceWindowTest", nullptr, nullptr);
    CORRADE_VERIFY(window);

    void *nativeHandle = getNativeWindow(window);
    CORRADE_VERIFY(nativeHandle);

    // Use OpenGL for windowed tests on Windows — Filament's Vulkan backend
    // fails to create a SwapChain from GLFW native windows on Intel
    // integrated GPUs (vkCreateSwapchainKHR returns VK_ERROR_DEVICE_LOST).
    // On macOS, use the default backend (Metal) since OpenGL is deprecated.
#ifdef __APPLE__
    DeviceFixture f;
#else
    DeviceFixture f("opengl");
#endif
    CORRADE_VERIFY(f.device);

    // Create and configure native surface
    ANARIObject ns = anariNewObject(f.device, "nativeSurface", "default");
    CORRADE_VERIFY(ns);
    // ANARI_VOID_POINTER takes the pointer value directly as the 5th arg —
    // NOT a pointer to it. anari_cpp_impl.hpp:530 dereferences one level for
    // this type, so &nativeHandle would store a dangling stack address.
    anariSetParameter(
        f.device, ns, "nativeWindow", ANARI_VOID_POINTER, nativeHandle);
    anariCommitParameters(f.device, ns);

    // Set up a minimal scene with a triangle
    const float vertices[] = {
        -0.5f, -0.5f, -2.0f,
         0.5f, -0.5f, -2.0f,
         0.0f,  0.5f, -2.0f};
    ANARIArray1D posArr = anariNewArray1D(
        f.device, vertices, nullptr, nullptr, ANARI_FLOAT32_VEC3, 3);

    ANARIGeometry geom = anariNewGeometry(f.device, "triangle");
    anariSetParameter(
        f.device, geom, "vertex.position", ANARI_ARRAY1D, &posArr);
    anariCommitParameters(f.device, geom);

    ANARIMaterial mat = anariNewMaterial(f.device, "matte");
    const float color[] = {0.8f, 0.2f, 0.1f};
    anariSetParameter(
        f.device, mat, "color", ANARI_FLOAT32_VEC3, color);
    anariCommitParameters(f.device, mat);

    ANARISurface surface = anariNewSurface(f.device);
    anariSetParameter(
        f.device, surface, "geometry", ANARI_GEOMETRY, &geom);
    anariSetParameter(
        f.device, surface, "material", ANARI_MATERIAL, &mat);
    anariCommitParameters(f.device, surface);

    ANARIWorld world = anariNewWorld(f.device);
    ANARIArray1D surfArr = anariNewArray1D(
        f.device, &surface, nullptr, nullptr, ANARI_SURFACE, 1);
    anariSetParameter(
        f.device, world, "surface", ANARI_ARRAY1D, &surfArr);
    anariCommitParameters(f.device, world);

    ANARICamera camera = anariNewCamera(f.device, "perspective");
    anariCommitParameters(f.device, camera);

    ANARIRenderer renderer = anariNewRenderer(f.device, "default");
    const float bg[] = {0.1f, 0.1f, 0.3f, 1.0f};
    anariSetParameter(
        f.device, renderer, "background", ANARI_FLOAT32_VEC4, bg);
    anariCommitParameters(f.device, renderer);

    ANARIFrame frame = anariNewFrame(f.device);
    const uint32_t imgSize[] = {64, 64};
    anariSetParameter(f.device, frame, "size", ANARI_UINT32_VEC2, imgSize);
    anariSetParameter(f.device, frame, "renderer", ANARI_RENDERER, &renderer);
    anariSetParameter(f.device, frame, "camera", ANARI_CAMERA, &camera);
    anariSetParameter(f.device, frame, "world", ANARI_WORLD, &world);
    anariSetParameter(f.device, frame, "nativeSurface", ANARI_OBJECT, &ns);
    anariCommitParameters(f.device, frame);

    // Render two frames to verify stability
    for (int i = 0; i < 2; ++i) {
        anariRenderFrame(f.device, frame);
        int ready = anariFrameReady(f.device, frame, ANARI_WAIT);
        CORRADE_VERIFY(ready);
        glfwPollEvents();
    }

    anariRelease(f.device, frame);
    anariRelease(f.device, renderer);
    anariRelease(f.device, camera);
    anariRelease(f.device, surfArr);
    anariRelease(f.device, world);
    anariRelease(f.device, surface);
    anariRelease(f.device, mat);
    anariRelease(f.device, geom);
    anariRelease(f.device, posArr);
    anariRelease(f.device, ns);

    glfwDestroyWindow(window);
}

}

CORRADE_TEST_MAIN(NativeSurfaceWindowTest)
