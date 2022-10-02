#pragma once

#include <string>
#include <string_view>
#include <stdexcept>
#include <vector>

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include "constants.hpp"
#include "util.hpp"

template<class T>
T sqr(T x) {
    return x * x;
}

bool equal(float a, float b);

bool lesser(float a, float b);

std::string to_string(std::string_view str);

void sdl2_fail(std::string_view message);

void glew_fail(std::string_view message, GLenum error);

GLuint create_shader(GLenum type, const char *source);

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

void SDL_init(SDL_Window *&window, SDL_GLContext &gl_context);

void gen_view_matrix(float *m, float width, float height);

template<class T>
void bindData(GLuint array_type, GLuint vbo, const std::vector<T> &vec) {
    glBindBuffer(array_type, vbo);
    glBufferData(array_type, sizeof(T) * vec.size(), vec.data(), GL_STATIC_DRAW);
}

template<class T>
void bindArgument(GLuint array_type, GLuint vbo, GLuint vao, size_t arg, GLint size, GLenum type, GLboolean norm, const GLvoid *pointer) {
    glBindVertexArray(vao);
    glBindBuffer(array_type, vbo);
    glEnableVertexAttribArray(arg);
    glVertexAttribPointer(arg, size, type, norm, sizeof(T), pointer);
}

int getRnd(int l, int r);