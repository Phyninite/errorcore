#pragma once

#include <EGL/egl.h>
#include <GLES3/gl32.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void* client;
    void* surface_control;
    void* surface;
    EGLDisplay display;
    EGLSurface egl_surface;
    EGLContext context;
    int screen_width;
    int screen_height;
} exploit_client;

extern const char* vertex_shader_source;
extern const char* fragment_shader_source;

int initialize_renderer(exploit_client* renderer);
void render_frame(exploit_client* renderer);
void cleanup_renderer(exploit_client* renderer);

#ifdef __cplusplus
}
#endif
