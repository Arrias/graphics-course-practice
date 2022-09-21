#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <tuple>
#include <utility>

const char vertex_shader_source[] =
        R"(#version 330 core

uniform mat4 view;

layout (location = 0) in vec2 in_position;
layout (location = 1) in vec4 in_color;

out vec4 color;

void main()
{
    gl_Position = view * vec4(in_position, 0.5, 1.0);
    color = in_color;
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

in vec4 color;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = color;
}
)";

struct vec2 {
    float x;
    float y;
};

struct vertex {
    vec2 position;
    std::uint8_t color[4];
};

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum type, const char *source) {
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return result;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader);
    glAttachShader(result, fragment_shader);
    glLinkProgram(result);

    GLint status;
    glGetProgramiv(result, GL_LINK_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetProgramiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetProgramInfoLog(result, info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Program linkage failed: " + info_log);
    }

    return result;
}

vec2 bezier(std::vector<vertex> const &vertices, float t) {
    std::vector<vec2> points(vertices.size());

    for (std::size_t i = 0; i < vertices.size(); ++i)
        points[i] = vertices[i].position;

    // De Casteljau's algorithm
    for (std::size_t k = 0; k + 1 < vertices.size(); ++k) {
        for (std::size_t i = 0; i + k + 1 < vertices.size(); ++i) {
            points[i].x = points[i].x * (1.f - t) + points[i + 1].x * t;
            points[i].y = points[i].y * (1.f - t) + points[i + 1].y * t;
        }
    }
    return points[0];
}

// bond between vao and vbo
class PointHolder : public std::vector<vertex> {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(vertex), (void *) (sizeof(float) * 2));
    }

    // reinit vbo
    void bind() {
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertex) * size(), data(), GL_STATIC_DRAW);
    }
};

int main() try {

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);
        sdl2_fail("SDL_GL_CreateContext: ");



    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
    auto program = create_program(vertex_shader, fragment_shader);

    GLuint view_location = glGetUniformLocation(program, "view");

    //auto triangle = get_triangle_vertices(width, height);

    std::vector<vertex> curve_vertexes;
    std::vector<vertex> bezier_vertexes;

    GLuint vbo_s[2];
    GLuint vao_s[2];

    glGenVertexArrays(2, vao_s);
    glGenBuffers(2, vbo_s);
    glLineWidth(5.f);
    glPointSize(10);

    PointHolder curve_holder(vao_s[0], vbo_s[0]);
    PointHolder bezier_holder(vao_s[1], vbo_s[1]);

    int quality = 4;
    auto last_frame_start = std::chrono::high_resolution_clock::now();
    float time = 0.f;
    bool running = true;

    auto build_bezier = [&bezier_holder, &curve_holder, &quality]() {
        bezier_holder.clear();
        if (curve_holder.size() < 2) return;

        float step = 1.f / (((float) curve_holder.size() - 1) * (float) quality);
        std::cout << "step = " << step << std::endl;

        float t = 0;
        for (int i = 0; i < (curve_holder.size() - 1) * quality + 1; ++i) {
            bezier_holder.push_back({bezier(curve_holder, t), {255, 0, 0, 0}});
            t += step;
        }
    };

    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_WINDOWEVENT:
                    switch (event.window.event) {
                        case SDL_WINDOWEVENT_RESIZED:
                            width = event.window.data1;
                            height = event.window.data2;
                            glViewport(0, 0, width, height);
                            break;
                    }
                    break;
                case SDL_MOUSEBUTTONDOWN:
                    if (event.button.button == SDL_BUTTON_LEFT) {
                        int mouse_x = event.button.x;
                        int mouse_y = event.button.y;
                        curve_holder.push_back({(float) mouse_x, (float) mouse_y, {0, 0, 0, 1}});
                        curve_holder.bind();
                        build_bezier();
                        bezier_holder.bind();
                    } else if (event.button.button == SDL_BUTTON_RIGHT) {
                        int mouse_x = event.button.x;
                        int mouse_y = event.button.y;
                        if (!curve_holder.empty()) {
                            curve_holder.pop_back();
                            build_bezier();
                            bezier_holder.bind();
                        }
                    }
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_LEFT) {
                        quality = std::max(quality - 1, 1);
                        build_bezier();
                        bezier_holder.bind();
                    } else if (event.key.keysym.sym == SDLK_RIGHT) {
                        ++quality;
                        build_bezier();
                        bezier_holder.bind();
                    }
                    std::cout << "quality = " << quality << std::endl;
                    break;
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        glClear(GL_COLOR_BUFFER_BIT);

        float view[16] =
                {
                        2.f / (float) width, 0.f, 0.f, -1.f,
                        0.f, -2.f / (float) height, 0.f, 1.f,
                        0.f, 0.f, 1.f, 0.f,
                        0.f, 0.f, 0.f, 1.f,
                };

        glUseProgram(program);
        glUniformMatrix4fv(view_location, 1, GL_TRUE, view);

        glBindVertexArray(curve_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) curve_holder.size());
        glDrawArrays(GL_POINTS, 0, (GLsizei) curve_holder.size());

        glBindVertexArray(bezier_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) bezier_holder.size());

        glBindVertexArray(curve_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) curve_holder.size());
        glDrawArrays(GL_POINTS, 0, (GLsizei) curve_holder.size());

        glBindVertexArray(bezier_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) bezier_holder.size());

        glBindVertexArray(curve_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) curve_holder.size());
        glDrawArrays(GL_POINTS, 0, (GLsizei) curve_holder.size());

        glBindVertexArray(bezier_holder.getVao());
        glDrawArrays(GL_LINE_STRIP, 0, (GLsizei) bezier_holder.size());

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
