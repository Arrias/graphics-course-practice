#include <stdexcept>
#include <SDL2/SDL_error.h>
#include <SDL2/SDL_video.h>
#include <SDL2/SDL.h>
#include <complex>
#include <random>
#include <fstream>
#include "utils.h"
#include "stb_image.h"
#include "glm/ext/scalar_constants.hpp"

namespace {

std::default_random_engine rng;

float get_rnd(float l, float r) {
    return std::uniform_real_distribution{l, r}(rng);
};

const float eps = 0.01;

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


BoundingBox CalcBoundingBox(const std::vector<std::vector<obj_data::vertex>> &dats) {
    const auto INF = std::numeric_limits<float>::max();

    float x[] = {INF, -INF};
    float y[] = {INF, -INF};
    float z[] = {INF, -INF};

    for (const auto &dat: dats) {
        for (const auto &ver: dat) {
            x[0] = std::min(x[0], ver.position[0]);
            y[0] = std::min(y[0], ver.position[1]);
            z[0] = std::min(z[0], ver.position[2]);

            x[1] = std::max(x[1], ver.position[0]);
            y[1] = std::max(y[1], ver.position[1]);
            z[1] = std::max(z[1], ver.position[2]);
        }
    }

    auto C = glm::vec3{(x[0] + x[1]) / 2.f, (y[0] + y[1]) / 2.f, (z[0] + z[1]) / 2.f};
    std::vector<glm::vec3> vertices;
    for (int i = 0; i < 2; ++i) {
        for (int j = 0; j < 2; ++j) {
            for (int k = 0; k < 2; ++k) {
                vertices.emplace_back(x[i], y[j], z[k]);
            }
        }
    }
    return BoundingBox{
            .vertices = vertices,
            .C = C
    };
}

bool PState::tick() {
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

            case SDL_KEYDOWN:
                button_down[event.key.keysym.sym] = true;
                if (event.key.keysym.sym == SDLK_SPACE)
                    paused = !paused;
                break;
            case SDL_KEYUP:
                button_down[event.key.keysym.sym] = false;
                break;
        }

    if (!running) return false;

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame).count();

    update_particles(dt);
    last_frame = now;

    if (!paused) time += dt;

    if (button_down[SDLK_UP])
        camera_distance -= 4.f * dt;
    if (button_down[SDLK_DOWN])
        camera_distance += 4.f * dt;

    if (button_down[SDLK_LEFT])
        view_azimuth -= 2.f * dt;
    if (button_down[SDLK_RIGHT])
        view_azimuth += 2.f * dt;

    if (button_down[SDLK_t])
        ambient_light += 0.001f;
    if (button_down[SDLK_y])
        ambient_light -= 0.001f;

    if (button_down[SDLK_u])
        env_lightness += 0.001f;
    if (button_down[SDLK_i])
        env_lightness -= 0.001f;

    return true;
}

PState::PState(int width, int height) : width(width), height(height) {}

void PState::update_particles(float dt) {
    if (paused) return;
    if (particles.size() < 400) {
        particles.push_back(new_particle());
    }

    float A = 0.5;
    float C = 2;
    float D = 0.2;
    float maxY = 0.3f;
    for (auto &p: particles) {
        p.speed.y -= dt * A;
        p.position += p.speed * dt;
        p.speed *= exp(-C * dt);
        p.size *= exp(-D * dt);
        p.angle += p.angular_speed * dt;

        if (p.position.y < eps) {
            p = new_particle();
        }
    }
}

particle PState::new_particle() {
    particle p{};
    p.position.x = get_rnd(-0.8f, 0.8f);
    p.position.z = get_rnd(-sqrt(1 - p.position.x * p.position.x), sqrt(1 - p.position.x * p.position.x));
    p.position.y = sqrt(1 - p.position.x * p.position.x - p.position.z * p.position.z) - eps;
    p.size = get_rnd(0.01, 0.02);
    p.speed = glm::vec3(get_rnd(0.01, 0.02), -get_rnd(0.1, 0.2), get_rnd(0.01, 0.02));
    p.angle = 0;
    p.angular_speed = get_rnd(0.1, 0.9);
    return p;
}

std::tuple<GLuint, GLuint, GLuint, int> GenSphereBuffers() {
    GLuint sphere_vao, sphere_vbo, sphere_ebo;
    glGenVertexArrays(1, &sphere_vao);
    glBindVertexArray(sphere_vao);
    glGenBuffers(1, &sphere_vbo);
    glGenBuffers(1, &sphere_ebo);
    GLuint sphere_index_count;
    {
        auto [vertices, indices] = generate_sphere(1.f, 16);

        glBindBuffer(GL_ARRAY_BUFFER, sphere_vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(vertices[0]), vertices.data(), GL_STATIC_DRAW);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphere_ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(indices[0]), indices.data(), GL_STATIC_DRAW);

        sphere_index_count = indices.size();
    }
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, tangent));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, normal));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(vertex), (void *) offsetof(vertex, texcoords));

    return std::tuple(sphere_vao, sphere_vbo, sphere_ebo, sphere_index_count);
}

std::tuple<GLuint, GLuint> GenSnowflakeBuffers() {
    GLuint vao, vbo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) (0));

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) 12);

    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(particle), (void *) 28);

    return {vao, vbo};
}

static glm::vec3 cube_vertices[]
        {
                {0.f, 0.f, 0.f},
                {1.f, 0.f, 0.f},
                {0.f, 1.f, 0.f},
                {1.f, 1.f, 0.f},
                {0.f, 0.f, 1.f},
                {1.f, 0.f, 1.f},
                {0.f, 1.f, 1.f},
                {1.f, 1.f, 1.f},
        };

static std::uint32_t cube_indices[]
        {
                // -Z
                0, 2, 1,
                1, 2, 3,
                // +Z
                4, 5, 6,
                6, 5, 7,
                // -Y
                0, 1, 4,
                4, 1, 5,
                // +Y
                2, 6, 3,
                3, 6, 7,
                // -X
                0, 4, 2,
                2, 4, 6,
                // +X
                1, 3, 5,
                5, 3, 7,
        };


std::tuple<GLuint, GLuint, GLuint, int> GenFogBuffers() {
    GLuint vao, vbo, ebo;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);

    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cube_indices), cube_indices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    return {vao, vbo, ebo, std::size(cube_indices)};
}

GLuint Load3dTexture(const std::string &path) {
    int width = 128;
    int height = 64;
    int depth = 64;

    std::vector<char> pixels(width * height * depth);

    GLuint texture;
    glActiveTexture(GL_TEXTURE0);
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_3D, texture);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    std::ifstream input(path, std::ios::binary);
    input.read(pixels.data(), pixels.size());

    glTexImage3D(GL_TEXTURE_3D, 0, GL_R8, width, height, depth, 0, GL_RED, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_3D);
    return texture;
}
