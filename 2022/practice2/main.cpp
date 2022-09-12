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

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
        R"(#version 330 core

#define M_PI 3.1415926535897932384626433832795
//uniform float scale;
//uniform float angle;
uniform mat4 transform;
uniform mat4 view;

const vec3 COLORS[8] = vec3[8](
    vec3(0.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 0.0, 1.0),
    vec3(0.0, 1.0, 1.0),
    vec3(1.0, 1.0, 0.0),
    vec3(1.0, 0.0, 1.0),
    vec3(0.0, 1.0, 0.0)
);

out vec3 color;
float angle = M_PI / 3;

void main()
{
    // vec2 position = VERTICES[gl_VertexID];
    // mat2 rotation_matrix = mat2(cos(angle), sin(angle), -sin(angle), cos(angle));
    // position = rotation_matrix * position;
    // position *= scale;

    //gl_Position = view * transform * vec4(position, 0.0, 1.0);
    //gl_Position = vec4(position, 0.0, 1.0);

    float id = float(gl_VertexID);

    if (gl_VertexID == 0) {
        gl_Position = vec4(0.0, 0.0, 0.0, 1.0);
    } else {
        float id = gl_VertexID - 1;
        gl_Position = vec4(0.0, 1.0, 0.0, 1.0);

        mat4 calc_coords;

        float theta = angle * id;
        calc_coords[0] = vec4(cos(theta), sin(theta), 0.0, 0.0);
        calc_coords[1] = vec4(-sin(theta), cos(theta), 0.0, 0.0);
        calc_coords[2] = vec4(0.0, 0.0, 1.0, 0.0);
        calc_coords[3] = vec4(0.0, 0.0, 0.0, 1.0);

        gl_Position = calc_coords * gl_Position;
    }

    color = COLORS[gl_VertexID];
    gl_Position = view * transform * gl_Position;
}
)";

const char fragment_shader_source[] =
        R"(#version 330 core

in vec3 color;

layout (location = 0) out vec4 out_color;

void main()
{
    out_color = vec4(color, 1.0);
}
)";

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

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 2",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    SDL_GL_SetSwapInterval(0);

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    GLuint vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
    GLuint fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);

    GLuint program = create_program(vertex_shader, fragment_shader);

    GLuint vao;
    glGenVertexArrays(1, &vao);

    glUseProgram(program);
    GLint transformLocation = glGetUniformLocation(program, "transform");
    GLint viewLocation = glGetUniformLocation(program, "view");

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;
    float scale = 0.5;

    bool running = true;
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
            }

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = 0.0016f;//std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;

        float A = cos(time) * scale;
        float B = sin(time) * scale;
        float C = -sin(time) * scale;
        float D = cos(time) * scale;
        float x = 0.5;
        float y = 0.5;
        float aspect_ratio = float(width) / float(height);

        float transform[] = {
                A, B, 0, A * x + B * y,
                C, D, 0, C * x + D * y,
                0, 0, 1, 0,
                0, 0, 0, 1
        };

        float view[] = {
                1 / aspect_ratio, 0, 0, 0,
                0, 1, 0, 0,
                0, 0, 1, 0,
                0, 0, 0, 1
        };

        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);

        glUniformMatrix4fv(transformLocation, 1, GL_TRUE, transform);
        glUniformMatrix4fv(viewLocation, 1, GL_TRUE, view);

        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 8);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
