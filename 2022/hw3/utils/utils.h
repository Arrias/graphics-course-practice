#pragma once

#include <GL/glew.h>
#include <string>
#include <vector>
#include <map>
#include "glm/vec3.hpp"
#include "glm/vec2.hpp"
#include "obj_parser.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/matrix.hpp"
#include "glm/trigonometric.hpp"

struct vertex {
    glm::vec3 position;
    glm::vec3 tangent;
    glm::vec3 normal;
    glm::vec2 texcoords;
};

std::pair<std::vector<vertex>, std::vector<std::uint32_t>> generate_sphere(float radius, int quality);

std::vector<obj_data::vertex> generate_floor(float radius, int quality);

std::string to_string(std::string_view str);

void sdl2_fail(std::string_view message);

void glew_fail(std::string_view message, GLenum error);

std::pair<SDL_Window *, SDL_GLContext> CreateWindowContext();

template<class T>
void bindData(GLuint array_type, GLuint vbo, GLuint vao, const std::vector<T> &vec) {
    glBindVertexArray(vao);
    glBindBuffer(array_type, vbo);
    glBufferData(array_type, sizeof(T) * vec.size(), vec.data(), GL_STATIC_DRAW);
}

template<class T>
void bindArgument(GLuint array_type,
                  GLuint vbo,
                  GLuint vao,
                  size_t arg,
                  GLint size,
                  GLenum type,
                  GLboolean norm,
                  const GLvoid *pointer) {
    glBindVertexArray(vao);
    glBindBuffer(array_type, vbo);
    glEnableVertexAttribArray(arg);
    glVertexAttribPointer(arg, size, type, norm, sizeof(T), pointer);
}

glm::mat4 GetSunShadowTransform(const std::vector<glm::vec3> &bounding_box, const glm::vec3 &C,
                                glm::vec3 light_direction);

struct particle {
    glm::vec3 position;
    float size;
    glm::vec3 speed;
    float angle;
    float angular_speed;
};

struct PState {
    int width, height;

    PState(int width, int height);

    bool running = true;
    bool paused = false;

    std::chrono::time_point<std::chrono::high_resolution_clock> last_frame = std::chrono::high_resolution_clock::now();

    float view_elevation = glm::radians(30.f);
    float view_azimuth = 0.f;
    float camera_distance = 2.f;
    float ambient_light = 0.2f;
    float env_lightness = 1.f;
    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    std::vector<particle> particles;

    bool tick();

private:
    void update_particles(float dt);

    static particle new_particle();
};

struct BoundingBox {
    std::vector<glm::vec3> vertices;
    glm::vec3 C;
};

BoundingBox CalcBoundingBox(const std::vector<std::vector<obj_data::vertex>> &dats);

std::tuple<GLuint, GLuint, GLuint, int> GenSphereBuffers();

std::tuple<GLuint, GLuint> GenSnowflakeBuffers();

GLuint Load3dTexture(const std::string &path);

std::tuple<GLuint, GLuint, GLuint, int> GenFogBuffers();