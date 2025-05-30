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
    GLuint shader_program;
    GLuint vao, vbo;
    int screen_width, screen_height;

    const char* vertex_shader_source = R"(
#version 320 es
layout (location = 0) in vec2 a_pos;
void main() {
    gl_Position = vec4(a_pos, 0.0, 1.0);
}
)";

    const char* fragment_shader_source = R"(
#version 320 es
precision mediump float;
out vec4 frag_color;
void main() {
    frag_color = vec4(0.0, 0.6, 0.2, 0.8);
}
)";

    const float vertices[8] = {
        -0.3f, -0.2f,
         0.3f, -0.2f,
         0.3f,  0.2f,
        -0.3f,  0.2f
    };

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

        setup_shaders();
        setup_geometry();

        glViewport(0, 0, screen_width, screen_height);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        return true;
    }

    void render() {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(shader_program);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        
        eglSwapBuffers(display, egl_surface);
    }

    void cleanup() {
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        glDeleteProgram(shader_program);
        
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, context);
        eglDestroySurface(display, egl_surface);
        eglTerminate(display);
        
        surface_control.clear();
        client.clear();
    }

private:
    void setup_shaders() {
        GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex_shader, 1, &vertex_shader_source, nullptr);
        glCompileShader(vertex_shader);

        GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment_shader, 1, &fragment_shader_source, nullptr);
        glCompileShader(fragment_shader);

        shader_program = glCreateProgram();
        glAttachShader(shader_program, vertex_shader);
        glAttachShader(shader_program, fragment_shader);
        glLinkProgram(shader_program);

        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
    }

    void setup_geometry() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);

        glBindVertexArray(0);
    }
};
