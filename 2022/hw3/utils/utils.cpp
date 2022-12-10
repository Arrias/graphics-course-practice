#include <stdexcept>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>
#include <complex>
#include "utils.h"
#include "stb_image.h"
#include "glm/ext/scalar_constants.hpp"

std::string to_string(std::string_view str) {
    return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
    throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
    throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> generate_sphere(float radius, int quality) {
    std::vector<vertex> vertices;

    for (int latitude = -quality; latitude <= quality; ++latitude) {
        for (int longitude = 0; longitude <= 4 * quality; ++longitude) {
            float lat = (latitude * glm::pi<float>()) / (2.f * quality);
            float lon = (longitude * glm::pi<float>()) / (2.f * quality);

            auto &vertex = vertices.emplace_back();
            vertex.normal = {std::cos(lat) * std::cos(lon), std::sin(lat), std::cos(lat) * std::sin(lon)};
            vertex.position = vertex.normal * radius;
            vertex.tangent = {-std::cos(lat) * std::sin(lon), 0.f, std::cos(lat) * std::cos(lon)};
            vertex.texcoords.x = (longitude * 1.f) / (4.f * quality);
            vertex.texcoords.y = (latitude * 1.f) / (2.f * quality) + 0.5f;
        }
    }

    std::vector<std::uint32_t> indices;

    for (int latitude = 0; latitude < 2 * quality; ++latitude) {
        for (int longitude = 0; longitude < 4 * quality; ++longitude) {
            std::uint32_t i0 = (latitude + 0) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i1 = (latitude + 1) * (4 * quality + 1) + (longitude + 0);
            std::uint32_t i2 = (latitude + 0) * (4 * quality + 1) + (longitude + 1);
            std::uint32_t i3 = (latitude + 1) * (4 * quality + 1) + (longitude + 1);

            indices.insert(indices.end(), {i0, i1, i2, i2, i1, i3});
        }
    }

    return {std::move(vertices), std::move(indices)};
}

std::pair<SDL_Window *, SDL_GLContext> CreateWindowContext() {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    SDL_Window *window = SDL_CreateWindow("Graphics course hw3",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          800, 600,
                                          SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

    if (!window)
        sdl2_fail("SDL_CreateWindow: ");

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context)
        sdl2_fail("SDL_GL_CreateContext: ");

    if (auto result = glewInit(); result != GLEW_NO_ERROR)
        glew_fail("glewInit: ", result);

    if (!GLEW_VERSION_3_3)
        throw std::runtime_error("OpenGL 3.3 is not supported");

    return {window, gl_context};
}

std::vector<obj_data::vertex> generate_floor(float radius, int quality) {
    glm::vec2 vec{0.f, radius};

    std::vector<obj_data::vertex> vertices;
    vertices.push_back(obj_data::vertex{
            .position = {0.f, 0.f, 0.f},
            .normal = {0.f, 1.f, 0.f},
            .texcoord = {0.5f, 0.5f}
    });

    float angle = 2 * glm::pi<float>() / quality;
    std::complex<float> cur = {0, radius};

    for (int i = 0; i <= quality; ++i) {
        vertices.push_back(obj_data::vertex{
                .position = {cur.real(), 0.f, cur.imag()},
                .normal = {0.f, 1.f, 0.f},
                .texcoord = {0.5f + cur.real() / 5, 0.5f + cur.imag() / 5}
        });
        cur *= std::polar(1.f, angle);
    }

    return vertices;
}

glm::mat4 GetSunShadowTransform(const std::vector<glm::vec3> &bounding_box, const glm::vec3 &C,
                                glm::vec3 light_direction) {
    auto light_z = -light_direction;
    auto light_x = glm::normalize(glm::cross(light_z, {0.f, 1.f, 0.f}));
    auto light_y = glm::cross(light_x, light_z);

    auto dz = 0.f;
    auto dx = 0.f;
    auto dy = 0.f;
    for (auto V: bounding_box) {
        auto vec = V - C;
        dz = std::max(dz, glm::dot(vec, light_z));
        dx = std::max(dx, glm::dot(vec, light_x));
        dy = std::max(dy, glm::dot(vec, light_y));
    }

    return glm::inverse(glm::mat4({
                                          {dx * light_x.x, dx * light_x.y, dx * light_x.z, 0.f},
                                          {dy * light_y.x, dy * light_y.y, dy * light_y.z, 0.f},
                                          {dz * light_z.x, dz * light_z.y, dz * light_z.z, 0.f},
                                          {C.x,            C.y,            C.z,            1.f}
                                  }));
}