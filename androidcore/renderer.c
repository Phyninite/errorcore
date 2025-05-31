#include <android/log.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl32.h>
#include <stdlib.h>
#include <string.h>

#define LOG_TAG "drawing"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

typedef struct {
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
    EGLConfig config;
    GLuint program;
    GLuint vao;
    GLuint vbo;
    int width;
    int height;
} exploit_client;

const char* vertex_shader =
    "#version 320 es\n"
    "layout(location = 0) in vec2 position;\n"
    "void main() {\n"
    "    gl_Position = vec4(position, 0.0, 1.0);\n"
    "}\n";

const char* fragment_shader =
    "#version 320 es\n"
    "precision mediump float;\n"
    "out vec4 fragColor;\n"
    "void main() {\n"
    "    fragColor = vec4(1.0, 0.0, 0.0, 0.5);\n"
    "}\n";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, NULL);
    glCompileShader(shader);
    return shader;
}

GLuint create_program(void) {
    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, fragment_shader);
    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);
    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

exploit_client* init_renderer(void) {
    exploit_client* client = malloc(sizeof(exploit_client));
    memset(client, 0, sizeof(exploit_client));
    
    client->width = 1920;
    client->height = 1080;
    
    client->display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(client->display, NULL, NULL);
    
    EGLint attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    
    EGLint num_configs;
    eglChooseConfig(client->display, attribs, &client->config, 1, &num_configs);
    
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, client->width,
        EGL_HEIGHT, client->height,
        EGL_NONE
    };
    
    client->surface = eglCreatePbufferSurface(client->display, client->config, pbuffer_attribs);
    
    EGLint context_attribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    
    client->context = eglCreateContext(client->display, client->config, EGL_NO_CONTEXT, context_attribs);
    eglMakeCurrent(client->display, client->surface, client->surface, client->context);
    
    client->program = create_program();
    glGenVertexArrays(1, &client->vao);
    glGenBuffers(1, &client->vbo);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, client->width, client->height);
    
    LOGI("exploit_client overlay initialized %dx%d", client->width, client->height);
    
    return client;
}

void render_frame(exploit_client* client) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    glUseProgram(client->program);
    glBindVertexArray(client->vao);
    
    eglSwapBuffers(client->display, client->surface);
}

void cleanup_renderer(exploit_client* client) {
    if (client->vao) glDeleteVertexArrays(1, &client->vao);
    if (client->vbo) glDeleteBuffers(1, &client->vbo);
    if (client->program) glDeleteProgram(client->program);
    
    eglMakeCurrent(client->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (client->context != EGL_NO_CONTEXT) eglDestroyContext(client->display, client->context);
    if (client->surface != EGL_NO_SURFACE) eglDestroySurface(client->display, client->surface);
    if (client->display != EGL_NO_DISPLAY) eglTerminate(client->display);
    
    free(client);
}
