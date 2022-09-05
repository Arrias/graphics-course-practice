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

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

GLuint create_shader(GLenum shader_type, const char *shader_source) {
    GLuint shader = glCreateShader(shader_type);

    if (!shader)
        throw std::runtime_error("Incorrect shader type!");
    glShaderSource(shader, 1, &shader_source, nullptr);
    glCompileShader(shader);

    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled != GL_TRUE) {
        GLsizei log_length = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_buf(log_length, '\0');
        glGetShaderInfoLog(shader, error_buf.size(), &log_length, const_cast<GLchar *>(error_buf.c_str()));
        throw std::runtime_error(error_buf);
    }
    return shader;
}

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader) {
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);

    glLinkProgram(program);
    GLint fragment_compiled;
    glGetProgramiv(program, GL_LINK_STATUS, &fragment_compiled);
    if (fragment_compiled != GL_TRUE) {
        GLsizei log_length = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
        std::string error_buf(log_length, '\0');
        glGetProgramInfoLog(program, error_buf.size(), &log_length, const_cast<GLchar *>(error_buf.c_str()));
        throw std::runtime_error(error_buf);
    }
    return program;
}

int main() try {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_Window *window = SDL_CreateWindow("Graphics course practice 1", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    glClearColor(0.8f, 0.8f, 1.f, 0.f);

    const std::string vertex_shader =
            R"(
            #version 330 core

            const vec2 VERTICES[3] = vec2[3] (
                vec2(0.0, 0.0),
                vec2(1.0, 0.0),
                vec2(0.0, 1.0)
            );

            out vec3 color;

            void main()
            {
                gl_Position = vec4(VERTICES[gl_VertexID], 0.0, 1.0);
                color = vec3(gl_Position.x, gl_Position.y, 0.0);
            }
    )";

    const std::string color_shader =
            R"(
            #version 330 core

            layout (location = 0) out vec4 out_color;
            in vec3 color;

            float len = 1.0 / 16.0;

            void main()
            {
                float x = floor(color.x / len);
                float y = floor(color.y / len);
                int col = (int(x)+int(y))%2;
                out_color = vec4(col, col, col, 1.0);
            }
    )";

    auto v_shader = create_shader(GL_VERTEX_SHADER, vertex_shader.c_str());
    auto c_shader = create_shader(GL_FRAGMENT_SHADER, color_shader.c_str());

    auto program = create_program(v_shader, c_shader);

    GLuint arr;
    glGenVertexArrays(1, &arr);

    bool running = true;
    while (running) {
        for (SDL_Event event; SDL_PollEvent(&event);)
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
            }

        if (!running)
            break;

        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(program);
        glBindVertexArray(arr);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
