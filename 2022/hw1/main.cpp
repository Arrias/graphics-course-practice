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
#include <vector>

const int baseWidth = 800;
const int baseHeight = 600;
const std::string program_name = "Crazy functions";

struct vec2 {
    float x, y;
};

struct color {
    std::uint8_t data[4];
};

const std::string vertex_shader_source =
        R"(#version 330 core
           uniform mat4 transform;

           layout (location = 0) in vec2 in_position;
           layout (location = 1) in vec4 in_color;

           out vec4 color;
           void main()
           {
                gl_Position = view * vec4(in_position, 0.0, 1.0);
                color = in_color;
           }
)";

const std::string fragment_shader_source =
        R"(#version 330 core
            in vec4 color;
            layout (location = 0) out vec4 out_color;
            void main()
            {
                out_color = color;
            }
)";

std::string to_string(std::string_view str);

void sdl2_fail(std::string_view message);

void glew_fail(std::string_view message, GLenum error);

GLuint create_shader(GLenum type, const char *source);

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

void SDL_init(SDL_Window *&window, SDL_GLContext &gl_context);

int main(int argc, char **argv) try {
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_init(window, context);

    GLuint vao, grid_vbo;

    bool running = true;
    while (true) {
        for (SDL_Event event; SDL_PollEvent(&event);) {
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
        if (!running) {
            break;
        }

        glClear(GL_COLOR_BUFFER_BIT);
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

void SDL_init(SDL_Window *&window, SDL_GLContext &gl_context) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    window = SDL_CreateWindow(program_name.data(),
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              baseWidth, baseHeight,
                              SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");
}

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}