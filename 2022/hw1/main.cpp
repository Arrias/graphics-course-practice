#include <iostream>

#include "Graph.hpp"
#include "util.hpp"
#include "shaders.hpp"
#include "constants.hpp"
#include "Vec2.h"
#include "Circle.hpp"
#include "PointsHolder.hpp"
#include "Timer.hpp"

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
    for (int i = 0; i < 1; ++i) {
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
        auto new_circle = Circle(alter, Vec2{0, 0}, (float) getRnd(60, 100), 1.f, 0.f, 0.f);
        circles.push_back(new_circle);
    }

    int grid_n = 50;
    int grid_m = 50;
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

    float C = 0.8;
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
        gen_view_matrix(view, (float) width, (float) height);
        auto dt = timer.tick();

        for (auto &circle: circles) {
            circle.move(dt * 100.f, GridWidth, GridHeight);
        }

        graph.apply(std::function<float(Vec2, Color &)>([&timer, &circles, &get_component](Vec2 pos, Color &color) {
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

