#include <EGL/egl.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <ui/DisplayInfo.h>

using namespace android;

class exploit_renderer {
private:
    sp<SurfaceComposerClient> client;
    sp<SurfaceControl> surface_control;
    sp<Surface> surface;
    EGLDisplay display;
    EGLSurface egl_surface;
    EGLContext context;
    int screen_width, screen_height;

public:
    bool initialize() {
        client = new SurfaceComposerClient();
        
        DisplayInfo display_info;
        SurfaceComposerClient::getDisplayInfo(0, &display_info);
        screen_width = display_info.w;
        screen_height = display_info.h;

        surface_control = client->createSurface(
            String8("exploit_overlay"),
            screen_width,
            screen_height,
            PIXEL_FORMAT_RGBA_8888,
            0
        );

        SurfaceComposerClient::Transaction{}
            .setLayer(surface_control, 0x7fffffff)
            .setPosition(surface_control, 0, 0)
            .setSize(surface_control, screen_width, screen_height)
            .show(surface_control)
            .apply();

        surface = surface_control->getSurface();

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        eglInitialize(display, nullptr, nullptr);

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

        eglChooseConfig(display, config_attribs, &config, 1, &num_configs);

        ANativeWindow* native_window = surface.get();
        egl_surface = eglCreateWindowSurface(display, config, native_window, nullptr);

        EGLint context_attribs[] = {
            EGL_CONTEXT_CLIENT_VERSION, 3,
            EGL_NONE
        };

        context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        eglMakeCurrent(display, egl_surface, egl_surface, context);

        glViewport(0, 0, screen_width, screen_height);

        return true;
    }

    void render() {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        eglSwapBuffers(display, egl_surface);
    }

    void cleanup() {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, egl_surface);
        eglTerminate(display);
        
        surface_control.clear();
        client.clear();
    }
};
