#include <string_view>
#include <stdexcept>
#include <iostream>
#include <vector>
#include <functional>
#include <climits>
#include <chrono>
#include <random>
#include <cassert>

#include "Grid.hpp"
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
    for (int i = 0; i < 7; ++i) {
        auto center = get_rand_cord(20);
        auto coefficient_r = (float) getRnd(0, 1);
        auto coefficient_g = (float) getRnd(0, 1);
        if (equal(coefficient_g + coefficient_r, 0.0)) {
            coefficient_r++;
        }
        coefficient_r *= 10.f;
        coefficient_g *= 10.f;
        std::mt19937 rnd(std::chrono::steady_clock::now().time_since_epoch().count());
        auto new_circle = Circle(center, directions[rnd() % directions.size()], 20.f, coefficient_r, coefficient_g, 0.f);
        circles.push_back(new_circle);
    }

    int grid_n = 100;
    int grid_m = 100;
    Grid grid;
    grid.build(grid_n, grid_m, width, height);

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
        dst = std::min(dst, limits.back().first - 1000);

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
                    auto sym = event.key.keysym.sym;
                    if (sym != SDLK_DOWN && sym != SDLK_UP) break;
                    auto grid_delta = GridQuantityDelta;
                    if (sym == SDLK_DOWN) grid_delta *= -1;

                    grid_n += grid_delta;
                    grid_m += grid_delta;
                    grid.build(grid_n, grid_m, GridWidth, GridHeight);
            }
            if (event.type == SDL_QUIT) {
                running = false;
                break;
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
            circle.move(dt * 50.f, GridWidth, GridHeight);
        }

        grid.apply(std::function<Color(Vec2)>([&timer, &circles, &get_component](Vec2 pos) {
            float r_dst = 0, g_dst = 0, b_dst = 0, dst = 0;
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

