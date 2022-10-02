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
#include <cassert>

const int GridHeight = 300;
const int GridWidth = 300;
const int WindowWidth = 800;
const int WindowHeight = 600;

const float eps = 0.05;
const std::string program_name = "Crazy functions";
std::mt19937 rnd(std::chrono::steady_clock::now().time_since_epoch().count());

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

template<class T>
T sqr(T x);

bool equal(float a, float b);

bool lesser(float a, float b);

struct Vec2 {
    float x{}, y{};

    Vec2(float x, float y) : x(x), y(y) {}

    float square_len() const {
        return x * x + y * y;
    }

    void move90() {
        std::swap(x, y);
        x *= -1.f;
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

std::string to_string(std::string_view str);

void sdl2_fail(std::string_view message);

void glew_fail(std::string_view message, GLenum error);

GLuint create_shader(GLenum type, const char *source);

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader);

void SDL_init(SDL_Window *&window, SDL_GLContext &gl_context);

void gen_view_matrix(float *m, float width, float height);

template<class T>
void bindData(GLuint array_type, GLuint vbo, const std::vector<T> &vec);

template<class T>
void bindArgument(GLuint array_type, GLuint vbo, GLuint vao, size_t arg, GLint size, GLenum type, GLboolean norm, const GLvoid *pointer);

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
                points.emplace_back((float) i * shift_x, (float) j * shift_y);
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

int getRnd(int l, int r) {
    return (int) (rnd() % (r - l + 1) + l);
}

float floatRnd(int l, int r) {
    return (float) getRnd(l, r);
}

const auto sq2 = (float) sqrt(2);
const std::vector<Vec2> directions = {
        {0.f,  1.f},
        {sq2,  sq2},
        {1.f,  0.f},
        {sq2,  -sq2},
        {0.f,  -1.f},
        {-sq2, -sq2},
        {-1.f, 0.f},
        {-sq2, sq2},
};

struct Circle {
    Vec2 center, direction;
    float radius;
    float r, g, b;

    Circle(Vec2 center, Vec2 direction, float radius, float r, float g, float b) :
            center(center), direction(direction), radius(radius), r(r), g(g), b(b) {}

    float f(Vec2 pos) const {
        return sqr(radius) / (pos - center).square_len();
    }

    void move(float time, float width, float height) {
        center = center + time * direction;
        if (center.x <= 0 || center.y <= 0 || center.x >= width || center.y >= height) {
            center = center - 2 * time * direction;
            direction.move90();
            direction.move90();
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

    // setup some parameters of painting
    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    glPointSize(5);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    auto get_rand_cord = [ ](int delta) {
        uint32_t x = getRnd(delta, GridWidth - delta);
        uint32_t y = getRnd(delta, GridHeight - delta);
        return Vec2(float(x), float(y));
    };

    std::vector<Circle> circles;
//    circles.push_back(Circle({GridWidth / 2, GridHeight / 2}, directions[0], 20.f, 5, 0, 0));
//    circles.push_back(Circle({GridWidth / 2, GridHeight / 2}, directions[4], 20.f, 0, 5, 0));

    for (int i = 0; i < 10; ++i) {
        auto center = get_rand_cord(20);
        auto new_circle = Circle(center, directions[rnd() % directions.size()], 20.f, floatRnd(0, 1) * 5.f, floatRnd(0, 1) * 5.f,
                                 floatRnd(0, 1) * 5.f);
        circles.push_back(new_circle);

//        if (getRnd(0, 1)) {
//            auto new_circle = Circle(center, directions[rnd() % directions.size()], 20.f, 1, 0, 0);
//            circles.push_back(new_circle);
//        } else {
//            auto new_circle = Circle(center, directions[rnd() % directions.size()], 20.f, 1, 1, 0);
//            circles.push_back(new_circle);
//        }
    }

    int grid_n = 100;
    int grid_m = 100;
    Grid grid;
    grid.build(grid_n, grid_m, GridWidth, GridHeight);

    const std::vector<std::pair<float, unsigned>> limits = {
            {0.0,  0u},
            {1.f,  120u},
            {1.5,  170u},
            {2.f,  190u},
            {2.5,  220u},
            {3.f,  240u},
            {1e9f, 255u},
    };

    auto get_component = [&](float dst) {
        if (lesser(dst, 0.5f)) return 1u;
        dst = std::min(dst, limits.back().first - 3 * eps);

        for (int i = 1; i < limits.size(); ++i) {
            auto [cur_lim, cur_col_lim] = limits[i];
            if (lesser(dst, cur_lim)) {
                auto [prv_lim, prv_col_lim] = limits[i - 1];
                float frac = (dst - prv_lim) / (cur_lim - prv_lim);
                return (unsigned) ((float) prv_col_lim + (float(cur_col_lim - prv_col_lim)) * frac);
            }
        }

        assert(false);
    };

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
                        grid.build(grid_n, grid_m, GridWidth, GridHeight);
                    } else if (event.key.keysym.sym == SDLK_UP) {
                        grid_n += 5;
                        grid_m += 5;
                        grid.build(grid_n, grid_m, GridWidth, GridHeight);
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
        auto dt = timer.tick();

        for (auto &circle: circles) {
            circle.move(dt * 50.f, GridWidth, GridHeight);
        }

        grid.apply(std::function<Color(Vec2)>([&timer, &circles, &get_component](Vec2 pos) {
            float r_dst = 0, g_dst = 0, b_dst = 0;
            float dst = 0;
            for (Circle &circle: circles) {
                float add = circle.f(pos);
                r_dst += circle.r * add;
                g_dst += circle.g * add;
                b_dst += circle.b * add;
                dst += add;
            }
            Color ret;
            if (lesser(dst, 1.0)) return ret;
            ret.data[0] = get_component(r_dst);
            ret.data[1] = get_component(g_dst);
            ret.data[2] = get_component(b_dst);
            return ret;
        }));

        glClear(GL_COLOR_BUFFER_BIT);

        // set uniform
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
                              WindowWidth, WindowHeight,
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

template<class T>
T sqr(T x) {
    return x * x;
}

bool equal(float a, float b) {
    return fabs(a - b) < eps;
}

bool lesser(float a, float b) {
    return a + eps < b;
}