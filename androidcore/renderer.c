#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>

typedef struct {
    void* client;
    void* surface_control;
    void* surface;
    EGLDisplay display;
    EGLSurface egl_surface;
    EGLContext context;
    int screen_width;
    int screen_height;
} exploit_renderer;

int initialize_renderer(exploit_renderer* renderer) {
    renderer->client = new android::SurfaceComposerClient();
    
    android::DisplayInfo display_info;
    android::SurfaceComposerClient::getDisplayInfo(0, &display_info);
    renderer->screen_width = display_info.w;
    renderer->screen_height = display_info.h;

    android::sp<android::SurfaceControl> surface_control = 
        ((android::SurfaceComposerClient*)renderer->client)->createSurface(
            android::String8("exploit_overlay"),
            renderer->screen_width,
            renderer->screen_height,
            PIXEL_FORMAT_RGBA_8888,
            0
        );
    renderer->surface_control = surface_control.get();

    android::SurfaceComposerClient::Transaction{}
        .setLayer(surface_control, 0x7fffffff)
        .setPosition(surface_control, 0, 0)
        .setSize(surface_control, renderer->screen_width, renderer->screen_height)
        .show(surface_control)
        .apply();

    android::sp<android::Surface> surface = surface_control->getSurface();
    renderer->surface = surface.get();

    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(renderer->display, NULL, NULL);

    EGLConfig config;
    EGLint num_configs;
    EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_BLUE_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_RED_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };

    eglChooseConfig(renderer->display, config_attribs, &config, 1, &num_configs);

    ANativeWindow* native_window = (ANativeWindow*)renderer->surface;
    renderer->egl_surface = eglCreateWindowSurface(renderer->display, config, native_window, NULL);

    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };

    renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, context_attribs);
    eglMakeCurrent(renderer->display, renderer->egl_surface, renderer->egl_surface, renderer->context);

    glViewport(0, 0, renderer->screen_width, renderer->screen_height);

    return 1;
}

void render_frame(exploit_renderer* renderer) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    eglSwapBuffers(renderer->display, renderer->egl_surface);
}

void cleanup_renderer(exploit_renderer* renderer) {
    eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(renderer->display, renderer->context);
    eglDestroySurface(renderer->display, renderer->egl_surface);
    eglTerminate(renderer->display);
    
    if (renderer->surface_control) {
        ((android::sp<android::SurfaceControl>*)&renderer->surface_control)->clear();
    }
    if (renderer->client) {
        delete (android::SurfaceComposerClient*)renderer->client;
    }
}
