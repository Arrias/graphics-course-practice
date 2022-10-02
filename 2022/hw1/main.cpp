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
#include <functional>
#include <climits>
#include <chrono>
#include <random>

const int baseWidth = 800;
const int baseHeight = 600;
const std::string program_name = "Crazy functions";
std::mt19937 rnd(std::chrono::steady_clock::now().time_since_epoch().count());

template<class T>
T sqr(T x) {
    return x * x;
}

struct Vec2 {
    float x{}, y{};

    float len() {
        return x * x + y * y;
    }
};

Vec2 operator+(Vec2 a, Vec2 b) {
    return Vec2{a.x + b.x, a.y + b.y};
}

Vec2 operator-(Vec2 a, Vec2 b) {
    return Vec2{a.x - b.x, a.y - b.y};
}

Vec2 operator*(float sc, Vec2 a) {
    return {a.x * sc, a.y * sc};
}

struct Color {
    std::uint8_t data[4]{};
};

const std::string vertex_shader_source =
        R"(#version 330 core
           uniform mat4 view;

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

void gen_view_matrix(float *m, float width, float height) {
    float view[16] = {
            2.f / (float) width, 0.f, 0.f, -1.f,
            0.f, -2.f / (float) height, 0.f, 1.f,
            0.f, 0.f, 1.f, 0.f,
            0.f, 0.f, 0.f, 1.f,
    };
    memcpy(m, view, sizeof(float) * 16);
}

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

struct Timer {
    std::chrono::time_point<std::chrono::high_resolution_clock> last_frame_start = std::chrono::high_resolution_clock::now();
    float time = 0.f;

    float tick() {
        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;
        time += dt;
        return dt;
    }
};

struct PointsHolder {
    std::vector<Vec2> points;
    std::vector<Color> colors;
    std::vector<uint32_t> ids;
    GLuint points_vbo{}, colors_vbo{}, vao{}, ebo{};

    PointsHolder() {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &points_vbo);
        glGenBuffers(1, &colors_vbo);
        glGenBuffers(1, &ebo);
        bindArgument<Vec2>(GL_ARRAY_BUFFER, points_vbo, vao, 0, 2, GL_FLOAT, GL_FALSE, (void *) nullptr);
        bindArgument<Color>(GL_ARRAY_BUFFER, colors_vbo, vao, 1, 4, GL_UNSIGNED_BYTE, GL_TRUE, (void *) nullptr);
    }

    void updColors() const {
        bindData(GL_ARRAY_BUFFER, colors_vbo, colors);
    }

    void updPoints() const {
        bindData(GL_ARRAY_BUFFER, points_vbo, points);
    }

    void updIndexes() const {
        bindData(GL_ELEMENT_ARRAY_BUFFER, ebo, ids);
    }

    size_t size() const {
        return ids.size();
    }
};

struct Grid : public PointsHolder {
    void build(int n, int m, int width, int height) {
        points.clear();
        ids.clear();

        float shift_x = (float) width / (float) (n - 1);
        float shift_y = (float) height / (float) (m - 1);

        auto get_id = [&n, &m](int i, int j) {
            return i * m + j;
        };

        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < m; ++j) {
                points.push_back({(float) i * shift_x, (float) j * shift_y});
                if (i + 1 < n && j + 1 < m) {
                    // triangulation
                    ids.push_back(get_id(i, j));
                    ids.push_back(get_id(i + 1, j));
                    ids.push_back(get_id(i, j + 1));

                    ids.push_back(get_id(i + 1, j));
                    ids.push_back(get_id(i, j + 1));
                    ids.push_back(get_id(i + 1, j + 1));
                }
            }
        }

        colors.resize(points.size());
        updPoints();
        updIndexes();
    }

    void apply(std::function<Color(Vec2)> &&point_to_color) {
        for (int i = 0; auto &color: colors) {
            color = point_to_color(points[i++]);
        }
        updColors();
    }
};

const uint32_t COLOR_BASE = 256;
const uint32_t COLORS_CNT = COLOR_BASE * COLOR_BASE * COLOR_BASE;

Color get_color_by_number(uint32_t num) {
    Color ret;
    for (int i = 0; i < 3; ++i) {
        ret.data[i] = num % COLOR_BASE;
        num /= COLOR_BASE;
    }
    return ret;
}

int getRnd(int l, int r) {
    return rnd() % (r - l + 1) + l;
}

const auto sq2 = (float) sqrt(2);
const std::vector<Vec2> directions = {
        //{0.f,  1.f},
        {sq2,  sq2},
        // {1.f,  0.f},
        {sq2,  -sq2},
        //  {0.f,  -1.f},
        {-sq2, -sq2},
        //  {-1.f, 0.f},
        {-sq2, sq2},
};

struct Circle {
    Vec2 c;
    float r;
    Vec2 dir;
    int potencial;

    bool is_lie(Vec2 pos) const {
        return sqr(pos.x - c.x) + sqr(pos.y - c.y) <= r * r;
    }

    void move(float time, float width, float height) {
        c = c + time * dir;
        potencial--;
        if (c.x <= 0 || c.y <= 0 || c.x >= width || c.y >= height) {
            c = c - 2 * time * dir;
            float xx = dir.x;
            float yy = dir.y;
            dir = {-yy, xx};
            potencial = getRnd(10, 100);
        } else if (potencial == 0) {
            dir = directions[getRnd(0, (int) directions.size() - 1)];
            potencial = getRnd(10, 100);
        }
    }
};

int main(int argc, char **argv) try {
    // init
    SDL_Window *window = nullptr;
    SDL_GLContext context = nullptr;
    SDL_init(window, context);

    // create shaders, shader program
    auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
    auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
    auto program = create_program(vertex_shader, fragment_shader);
    glUseProgram(program);

    // get uniforms
    GLuint view_location = glGetUniformLocation(program, "view");

    const int GRID_WIDTH = 300;
    const int GRID_HEIGHT = 300;

    // setup some parameters of painting
    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    glPointSize(5);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    auto get_rand_cord = [&GRID_HEIGHT, &GRID_WIDTH]() {
        uint32_t x = rnd() % (GRID_WIDTH - 20) + 10;
        uint32_t y = rnd() % (GRID_HEIGHT - 20) + 10;
        return Vec2{float(x), float(y)};
    };

    std::vector<Circle> circles;
    for (int i = 0; i < 6; ++i) {
        circles.push_back(
                Circle{get_rand_cord(), float(rnd() % 50 + 20),
                       directions[rnd() % directions.size()]});
    }

    int grid_n = 100;
    int grid_m = 100;
    Grid grid;
    grid.build(grid_n, grid_m, 300, 300);

    const float MAX_VALUE = 5.f;

    Timer timer;
    bool running = true;
    while (true) {
        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_DOWN) {
                        grid_n -= 5;
                        grid_m -= 5;
                        grid.build(grid_n, grid_m, 300, 300);
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        grid_n += 5;
                        grid_m += 5;
                        grid.build(grid_n, grid_m, 300, 300);
                    }
            }
            if (event.type == SDL_QUIT) {
                running = false;
                break;
            }
        }
        if (!running) {
            break;
        }

        float view[16]{};
        gen_view_matrix(view, (float) width, (float) height);
        auto dt = timer.tick() * 50;

        for (auto &circle: circles) {
            circle.move(dt, GRID_WIDTH, GRID_HEIGHT);
        }

        grid.apply(std::function<Color(Vec2)>([&timer, &circles, &MAX_VALUE](Vec2 pos) {
            float dst = 0;
            for (Circle &circle: circles) {
                dst += ((circle.r * circle.r) / (circle.c - pos).len());
            }
            dst = std::min(dst, MAX_VALUE);
            float len = MAX_VALUE / 255;
            Color ret;
            ret.data[0] = int(floor(dst / len));
            return ret;
        }));

        glClear(GL_COLOR_BUFFER_BIT);
        glUniformMatrix4fv((GLint) view_location, 1, GL_TRUE, view);
        glBindVertexArray(grid.vao);
        glDrawElements(GL_TRIANGLES, (GLsizei) grid.size(), GL_UNSIGNED_INT, (void *) nullptr);
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