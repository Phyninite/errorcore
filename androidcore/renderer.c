#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <android/native_window.h>
#include <android/binder_ibinder.h>
#include <gui/Surface.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/ISurfaceComposer.h>
#include <ui/DisplayInfo.h>
#include <pthread.h>
#include <unistd.h>
#include <vector>

const char* vertex_shader_source =
"#version 320 es\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texCoord;\n"
"out vec2 TexCoord;\n"
"void main() {\n"
"    gl_Position = vec4(position, 1.0);\n"
"    TexCoord = texCoord;\n"
"}\n";

const char* fragment_shader_source =
"#version 320 es\n"
"precision mediump float;\n"
"in vec2 TexCoord;\n"
"out vec4 FragColor;\n"
"void main() {\n"
"    FragColor = vec4(0.0, 0.0, 0.0, 0.0);\n"
"}\n";

typedef struct {
    android::sp<android::SurfaceComposerClient> client;
    android::sp<android::SurfaceControl> surface_control;
    android::sp<android::Surface> android_surface;
    EGLDisplay display;
    EGLSurface egl_surface;
    EGLContext context;
    int screen_width;
    int screen_height;
    pthread_t render_thread;
    volatile bool thread_active;
    GLuint shader_program;
    GLuint vao;
    GLuint vbo;
} renderer_data_t;

static GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) return 0;
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint create_shader_program(const char* vs_source, const char* fs_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vs_source);
    if (vertex_shader == 0) return 0;

    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fs_source);
    if (fragment_shader == 0) {
        glDeleteShader(vertex_shader);
        return 0;
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        glDeleteShader(vertex_shader);
        glDeleteShader(fragment_shader);
        return 0;
    }

    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);

    GLint linked;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);

    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);

    if (!linked) {
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

void* render_thread_func(void* arg) {
    renderer_data_t* renderer = (renderer_data_t*)arg;

    if (eglMakeCurrent(renderer->display, renderer->egl_surface, renderer->egl_surface, renderer->context) == EGL_FALSE) {
        renderer->thread_active = false;
        return NULL;
    }
    
    glUseProgram(renderer->shader_program);
    glBindVertexArray(renderer->vao);

    while (renderer->thread_active) {
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        if (eglSwapBuffers(renderer->display, renderer->egl_surface) == EGL_FALSE) {
            renderer->thread_active = false; 
        }
        usleep(16667);
    }

    glBindVertexArray(0);
    glUseProgram(0);
    eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    return NULL;
}

int initialize_renderer(renderer_data_t* renderer) {
    renderer->display = EGL_NO_DISPLAY;
    renderer->egl_surface = EGL_NO_SURFACE;
    renderer->context = EGL_NO_CONTEXT;
    renderer->shader_program = 0;
    renderer->vao = 0;
    renderer->vbo = 0;

    renderer->client = android::SurfaceComposerClient::create();
    if (renderer->client == nullptr) return 0;

    android::sp<android::IBinder> display_token = android::SurfaceComposerClient::getInternalDisplayToken();
    if (display_token == nullptr) {
        renderer->client.clear();
        return 0;
    }
    android::DisplayInfo display_info;
    if (android::SurfaceComposerClient::getDisplayInfo(display_token, &display_info) != android::OK) {
        renderer->client.clear();
        return 0;
    }
    renderer->screen_width = display_info.w;
    renderer->screen_height = display_info.h;

    renderer->surface_control = renderer->client->createSurface(
        android::String8("renderer_surface"),
        renderer->screen_width,
        renderer->screen_height,
        PIXEL_FORMAT_RGBA_8888,
        android::SurfaceControl::HIDDEN
    );

    if (renderer->surface_control == nullptr || !renderer->surface_control->isValid()) {
        renderer->client.clear();
        return 0;
    }
    
    android::SurfaceComposerClient::Transaction t;
    t.setLayer(renderer->surface_control, 0x7FFFFFFF);
    t.setPosition(renderer->surface_control, 0, 0);
    t.show(renderer->surface_control);
    t.apply();

    renderer->android_surface = renderer->surface_control->getSurface();
    if (renderer->android_surface == nullptr) {
        renderer->surface_control.clear();
        renderer->client.clear();
        return 0;
    }
    
    ANativeWindow* native_window = renderer->android_surface.get();

    renderer->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (renderer->display == EGL_NO_DISPLAY) goto error_cleanup;
    if (eglInitialize(renderer->display, NULL, NULL) == EGL_FALSE) goto error_cleanup;

    EGLConfig config;
    EGLint num_configs;
    const EGLint config_attribs[] = {
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0, EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };

    if (eglChooseConfig(renderer->display, config_attribs, &config, 1, &num_configs) == EGL_FALSE || num_configs == 0) {
        goto error_cleanup;
    }

    renderer->egl_surface = eglCreateWindowSurface(renderer->display, config, native_window, NULL);
    if (renderer->egl_surface == EGL_NO_SURFACE) goto error_cleanup;

    const EGLint context_attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    renderer->context = eglCreateContext(renderer->display, config, EGL_NO_CONTEXT, context_attribs);
    if (renderer->context == EGL_NO_CONTEXT) goto error_cleanup;

    if (eglMakeCurrent(renderer->display, renderer->egl_surface, renderer->egl_surface, renderer->context) == EGL_FALSE) {
        goto error_cleanup;
    }

    renderer->shader_program = create_shader_program(vertex_shader_source, fragment_shader_source);
    if (renderer->shader_program == 0) goto error_cleanup_gl;

    GLfloat quad_vertices[] = {
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 0.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
        -1.0f,  1.0f, 0.0f,   0.0f, 1.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 0.0f,
         1.0f,  1.0f, 0.0f,   1.0f, 1.0f
    };

    glGenVertexArrays(1, &renderer->vao);
    glGenBuffers(1, &renderer->vbo);
    glBindVertexArray(renderer->vao);
    glBindBuffer(GL_ARRAY_BUFFER, renderer->vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad_vertices), quad_vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (void*)(3 * sizeof(GLfloat)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    glViewport(0, 0, renderer->screen_width, renderer->screen_height);
    
    eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    renderer->thread_active = true;
    if (pthread_create(&renderer->render_thread, NULL, render_thread_func, renderer) != 0) {
        renderer->thread_active = false;
        goto error_cleanup_gl; // pthread_create failed, full GL cleanup needed
    }

    return 1;

error_cleanup_gl:
    if (eglGetCurrentContext() != renderer->context && renderer->context != EGL_NO_CONTEXT) {
         // Try to make context current for cleanup if not already.
         // This path is complex if current context is unknown or belongs elsewhere.
         // Assuming for this path, context was made current before failure or can be made current.
         if (eglMakeCurrent(renderer->display, renderer->egl_surface, renderer->egl_surface, renderer->context) == EGL_FALSE && renderer->context != EGL_NO_CONTEXT) {
            // Could not make context current, GL resources might leak if not cleaned by OS
         }
    }
    if (renderer->shader_program != 0) { glDeleteProgram(renderer->shader_program); renderer->shader_program = 0; }
    if (renderer->vao != 0) { glDeleteVertexArrays(1, &renderer->vao); renderer->vao = 0; }
    if (renderer->vbo != 0) { glDeleteBuffers(1, &renderer->vbo); renderer->vbo = 0; }
    // Fall through to EGL cleanup

error_cleanup:
    if (renderer->display != EGL_NO_DISPLAY) {
        eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (renderer->context != EGL_NO_CONTEXT) eglDestroyContext(renderer->display, renderer->context);
        if (renderer->egl_surface != EGL_NO_SURFACE) eglDestroySurface(renderer->display, renderer->egl_surface);
        eglTerminate(renderer->display);
    }
    renderer->display = EGL_NO_DISPLAY;
    renderer->context = EGL_NO_CONTEXT;
    renderer->egl_surface = EGL_NO_SURFACE;

    if (renderer->android_surface != nullptr) renderer->android_surface.clear();
    if (renderer->surface_control != nullptr) renderer->surface_control.clear();
    if (renderer->client != nullptr) renderer->client.clear();
    return 0;
}

void cleanup_renderer(renderer_data_t* renderer) {
    if (renderer->thread_active) {
        renderer->thread_active = false;
        pthread_join(renderer->render_thread, NULL);
    }

    if (renderer->display != EGL_NO_DISPLAY && renderer->context != EGL_NO_CONTEXT) {
        if (eglMakeCurrent(renderer->display, renderer->egl_surface, renderer->egl_surface, renderer->context) == EGL_TRUE) {
             if (renderer->shader_program != 0) glDeleteProgram(renderer->shader_program);
             if (renderer->vao != 0) glDeleteVertexArrays(1, &renderer->vao);
             if (renderer->vbo != 0) glDeleteBuffers(1, &renderer->vbo);
             eglMakeCurrent(renderer->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        }
    }
    
    if (renderer->display != EGL_NO_DISPLAY) {
        if (renderer->context != EGL_NO_CONTEXT) eglDestroyContext(renderer->display, renderer->context);
        if (renderer->egl_surface != EGL_NO_SURFACE) eglDestroySurface(renderer->display, renderer->egl_surface);
        eglTerminate(renderer->display);
    }
    
    renderer->display = EGL_NO_DISPLAY;
    renderer->context = EGL_NO_CONTEXT;
    renderer->egl_surface = EGL_NO_SURFACE;
    renderer->shader_program = 0;
    renderer->vao = 0;
    renderer->vbo = 0;

    if (renderer->android_surface != nullptr) renderer->android_surface.clear();
    if (renderer->surface_control != nullptr) renderer->surface_control.clear();
    if (renderer->client != nullptr) renderer->client.clear();
}
