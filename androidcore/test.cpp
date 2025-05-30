#include "renderer.hpp"
#include <unistd.h>

const float vertices[] = {
    -0.9f, -0.2f, 0.0f, 0.0f, 0.0f,
    -0.5f, -0.2f, 0.0f, 1.0f, 0.0f,
    -0.5f,  0.2f, 0.0f, 1.0f, 1.0f,
    -0.9f,  0.2f, 0.0f, 0.0f, 1.0f
};

const unsigned int indices[] = {
    0, 1, 2,
    2, 3, 0
};

const char* button_vertex_shader = 
"#version 320 es\n"
"layout (location = 0) in vec3 position;\n"
"layout (location = 1) in vec2 texCoord;\n"
"out vec2 TexCoord;\n"
"void main() {\n"
"    gl_Position = vec4(position, 1.0);\n"
"    TexCoord = texCoord;\n"
"}\n";

const char* button_fragment_shader = 
"#version 320 es\n"
"precision mediump float;\n"
"in vec2 TexCoord;\n"
"out vec4 FragColor;\n"
"void main() {\n"
"    FragColor = vec4(0.0, 0.6, 0.2, 1.0);\n"
"}\n";

GLuint compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    return shader;
}

GLuint create_program(const char* vertex_src, const char* fragment_src) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_src);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_src);
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

int main() {
    exploit_client renderer;
    
    if (!initialize_renderer(&renderer)) {
        return -1;
    }
    
    GLuint program = create_program(button_vertex_shader, button_fragment_shader);
    
    GLuint VAO, VBO, EBO;
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);
    
    glBindVertexArray(VAO);
    
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    while (true) {
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        
        glUseProgram(program);
        glBindVertexArray(VAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        
        eglSwapBuffers(renderer.display, renderer.egl_surface);
        usleep(16667);
    }
    
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteProgram(program);
    
    cleanup_renderer(&renderer);
    
    return 0;
}
