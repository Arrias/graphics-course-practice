#include <iostream>

#include "Graph.hpp"
#include "util.hpp"
#include "shaders.hpp"
#include "constants.hpp"
#include "Vec2.h"
#include "Circle.hpp"
#include "PointsHolder.hpp"
#include "Timer.hpp"

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

    // setup some parameters of painting
    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    int width, height;
    SDL_GetWindowSize(window, &width, &height);

    auto get_rand_cord = [ ](int delta) {
        uint32_t x = getRnd(delta, GridWidth - delta);
        uint32_t y = getRnd(delta, GridHeight - delta);
        return Vec2(float(x), float(y));
    };

    std::vector<Circle> circles;
    for (int i = 0; i < 7; ++i) {
        auto center = get_rand_cord(20);
        auto direction = directions[getRnd(0, (int) directions.size() - 1)];
        auto coefficient_r = (float) getRnd(0, 1);
        auto coefficient_g = (float) getRnd(0, 1);
        if (equal(coefficient_g + coefficient_r, 0.0)) {
            coefficient_r++;
        }
        coefficient_r *= 10.f;
        coefficient_g *= 10.f;
        std::mt19937 rnd(std::chrono::steady_clock::now().time_since_epoch().count());

        auto alter = Vec2{float(width) / 2, float(height) / 2};
        auto new_circle = Circle(center, direction, float(getRnd(60, 100)), 1.f, 0.f, 0.f);
        circles.push_back(new_circle);
    }

    int grid_n = 4 + GridQuantityDelta;
    int grid_m = 4 + GridQuantityDelta;
    Graph graph(grid_n, grid_m, width, height);

    const std::vector<std::pair<float, unsigned>> limits = {
            {0.0,  0u},
            {1.5,  170u},
            {2.f,  190u},
            {2.5,  220u},
            {3.f,  240u},
            {1e9f, 255u},
    };

    auto get_component = [&](float dst) {
        dst = std::min(dst, limits.back().first - 1000);

        for (int i = 1; i < limits.size(); ++i) {
            auto [cur_lim, cur_col_lim] = limits[i];
            if (lesser(dst, cur_lim)) {
                auto [prv_lim, prv_col_lim] = limits[i - 1];
                float frac = (dst - prv_lim) / (cur_lim - prv_lim);
                return (unsigned) ((float) prv_col_lim + (float(cur_col_lim - prv_col_lim)) * frac);
            }
        }

        throw std::runtime_error("get_component didn't return any");
    };

    float C = 1.f;
    std::vector<float> consts_for_lines = {C};

    Timer timer;
    bool running = true;
    while (true) {
        for (SDL_Event event; SDL_PollEvent(&event);) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    auto sym = event.key.keysym.sym;

                    if (sym == SDLK_1) {
                        consts_for_lines.push_back(C);
                    }
                    if (sym == SDLK_RIGHT || sym == SDLK_LEFT) {
                        if (sym == SDLK_RIGHT) C += 0.1;
                        else C -= 0.1;
                        std::cout << "C = " << C << std::endl;
                        break;
                    }

                    if (sym != SDLK_DOWN && sym != SDLK_UP) break;
                    auto grid_delta = GridQuantityDelta;
                    if (sym == SDLK_DOWN) grid_delta *= -1;

                    grid_n += grid_delta;
                    grid_m += grid_delta;
                    graph.buildGrid(grid_n, grid_m, GridWidth, GridHeight);
            }
        }
        if (!running) {
            break;
        }

        SDL_GetWindowSize(window, &width, &height);
        float view[16]{};
        gen_view_matrix(view, (float) GridWidth, (float) GridHeight);
        auto dt = timer.tick();

        for (auto &circle: circles) {
            circle.move(dt * 100.f, GridWidth, GridHeight);
        }

        graph.apply(std::function<float(Vec2, Color & )>([&timer, &circles, &get_component](Vec2 pos, Color &color) {
            float r_dst = 0, g_dst = 0, b_dst = 0, dst = 0;
            for (Circle &circle: circles) {
                float add = circle.f(pos);
                r_dst += circle.r * add;
                g_dst += circle.g * add;
                b_dst += circle.b * add;
                dst += add;
            }
            color = Color();
            if (!lesser(dst, 1.0)) {
                color.data[0] = get_component(r_dst);
                color.data[1] = get_component(g_dst);
                color.data[2] = get_component(b_dst);
            }
            return dst;
        }));

        for (auto line: consts_for_lines) {
            graph.addLine(line);
        }

        glClear(GL_COLOR_BUFFER_BIT);

        // set uniform
        glUseProgram(program);
        glUniformMatrix4fv((GLint) view_location, 1, GL_TRUE, view);

        graph.draw();
        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}

