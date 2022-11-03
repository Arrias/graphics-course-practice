#ifdef WIN32
#include <SDL.h>
#undef main
#else

#define TINYOBJLOADER_IMPLEMENTATION

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>
#include "tiny_obj_loader.hpp"

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

const std::string log_path = "../log.txt";
const std::string mtl_path = "../";

struct vec3 {
  tinyobj::real_t x, y, z;
};

struct obj_data {
  struct vertex {
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 2> texcoord;
    std::array<float, 3> color;
  };

  std::vector<vertex> vertices;
};

namespace Logger {
template<typename T>
void log(T n) {
  std::cout << n << std::endl;
  std::cerr << n << std::endl;
}

template<typename T, typename ...Args>
void log(const T &n, const Args &... rest) {
  std::cout << n << " ";
  std::cerr << n << " ";
  log(rest...);
}
};

std::string to_string(std::string_view str) {
  return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
  throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
  throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const std::string vertex_shader_source = R"(
    #version 330 core

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    layout (location = 0) in vec3 in_position;
    layout (location = 1) in vec3 in_normal;
    layout (location = 2) in vec2 in_texcoord;
    layout (location = 3) in vec3 in_color;

    out vec3 position;
    out vec3 normal;
    out vec3 color;

    void main() {
        gl_Position = projection * view * model * vec4(in_position, 1.0);

        color = in_color;
        position = (model * vec4(in_position, 1.0)).xyz;
        normal = normalize((model * vec4(in_normal, 0.0)).xyz);
    }
)";

const std::string fragment_shader_source = R"(
    #version 330 core

    in vec3 position;
    in vec3 normal;
    in vec3 color;

    uniform vec3 albedo;
    uniform vec3 sun_color;
    uniform vec3 camera_position;
    uniform vec3 sun_direction;

    layout (location = 0) out vec4 out_color;

    vec3 diffuse(vec3 direction) {
      return albedo * max(0.0, dot(normal, direction));
    }

    vec3 specular(vec3 direction) {
        float power = 64.0;
        vec3 reflected_direction = 2.0 * normal * dot(normal, direction) - direction;
        vec3 view_direction = normalize(camera_position - position);
        return albedo * pow(max(0.0, dot(reflected_direction, view_direction)), power);
    }

    vec3 phong(vec3 direction) {
      return diffuse(direction) + specular(direction);
    }

    void main() {
           float ambient_light = 0.2;
           vec3 color = albedo * ambient_light + sun_color * phong(sun_direction);
           out_color = vec4(color, 1.0);
    }
)";

template<class T>
void bindData(GLuint array_type, GLuint vbo, GLuint vao, const std::vector<T> &vec) {
  glBindVertexArray(vao);
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

obj_data parse_scene(const tinyobj::attrib_t &attrib,
                     const std::vector<tinyobj::shape_t> &shapes,
                     const std::vector<tinyobj::material_t> &materials) {
  std::vector<obj_data::vertex> vertices;
  for (const auto &shape : shapes) {
    size_t index_offset = 0;
    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      auto fv = size_t(shape.mesh.num_face_vertices[f]);

      for (size_t v = 0; v < fv; ++v) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
        tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
        tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

        assert(idx.normal_index >= 0);
        tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
        tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
        tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

        assert(idx.texcoord_index >= 0);
        tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
        tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

        // Optional: vertex colors
        tinyobj::real_t red = attrib.colors[3 * size_t(idx.vertex_index) + 0];
        tinyobj::real_t green = attrib.colors[3 * size_t(idx.vertex_index) + 1];
        tinyobj::real_t blue = attrib.colors[3 * size_t(idx.vertex_index) + 2];

        vertices.push_back(obj_data::vertex{
            .position = {vx, vy, vz},
            .normal = {nx, ny, nz},
            .texcoord = {tx, ty},
            .color = {red, green, blue}
        });
      }
      index_offset += fv;
      shape.mesh.material_ids[f];
    }
  }

  return obj_data{
      .vertices = vertices
  };
}

int main(int argc, char **argv) try {
  std::freopen(log_path.data(), "w", stderr);
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    sdl2_fail("SDL_Init: ");

  // Рутина по созданию окна
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

  SDL_Window *window = SDL_CreateWindow("Graphics course hw 2",
                                        SDL_WINDOWPOS_CENTERED,
                                        SDL_WINDOWPOS_CENTERED,
                                        800, 600,
                                        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED);

  if (!window)
    sdl2_fail("SDL_CreateWindow: ");

  int width, height;
  SDL_GetWindowSize(window, &width, &height);

  SDL_GLContext gl_context = SDL_GL_CreateContext(window);
  if (!gl_context)
    sdl2_fail("SDL_GL_CreateContext: ");

  if (auto result = glewInit(); result != GLEW_NO_ERROR)
    glew_fail("glewInit: ", result);

  if (!GLEW_VERSION_3_3)
    throw std::runtime_error("OpenGL 3.3 is not supported");

  // Создаем шейдеры
  auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
  auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
  auto program = create_program(vertex_shader, fragment_shader);

  GLuint model_location = glGetUniformLocation(program, "model");
  GLuint view_location = glGetUniformLocation(program, "view");
  GLuint projection_location = glGetUniformLocation(program, "projection");
  GLuint albedo_location = glGetUniformLocation(program, "albedo");
  GLuint sun_direction_location = glGetUniformLocation(program, "sun_direction");
  GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
  GLuint camera_position_location = glGetUniformLocation(program, "camera_position");

  glUseProgram(program);

  if (argc < 2) {
    throw std::runtime_error("Error: please, specify scene path");
  }
  const auto scene_path = std::string{argv[1]};
  Logger::log("[scene_path] =", scene_path);

  tinyobj::ObjReaderConfig reader_config;
  reader_config.mtl_search_path = mtl_path;
  tinyobj::ObjReader reader;

  if (!reader.ParseFromFile(scene_path, reader_config)) {
    if (!reader.Error().empty()) {
      throw std::runtime_error("TinyObjReader: " + reader.Error());
    }
    throw;
  }
  Logger::log(reader.Warning());
  auto &attrib = reader.GetAttrib();
  auto &shapes = reader.GetShapes();
  auto &materials = reader.GetMaterials();

  obj_data scene = parse_scene(attrib, shapes, materials);

  // Привязываем сцену к vbo, vao, ebo
  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 0, 3, GL_FLOAT, GL_FALSE, (void *) nullptr);
  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 1, 3, GL_FLOAT, GL_FALSE, (void *) 12);
  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 2, 2, GL_FLOAT, GL_FALSE, (void *) 24);
  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 3, 3, GL_FLOAT, GL_FALSE, (void *) 32);

  bindData(GL_ARRAY_BUFFER, vbo, vao, scene.vertices);

  float view_elevation = glm::radians(30.f);
  float view_azimuth = 0.f;
  float camera_distance = 900.f;
  float y = 0;
  float time = 0.f;

  std::map<SDL_Keycode, bool> button_down;
  auto last_frame_start = std::chrono::high_resolution_clock::now();

  bool running = true;
  bool paused = false;

  auto update_view_parameters = [&button_down, &camera_distance, &view_azimuth, &view_elevation, &y](float dt) {
    if (button_down[SDLK_UP])
      camera_distance -= 400.f * dt;
    if (button_down[SDLK_DOWN])
      camera_distance += 400.f * dt;

    if (button_down[SDLK_LEFT])
      view_azimuth -= 1.f * dt, y += 5.0;
    if (button_down[SDLK_RIGHT])
      view_azimuth += 1.f * dt, y -= 5.0;

    if (button_down[SDLK_g])
      view_elevation += glm::radians(.15f);
    if (button_down[SDLK_h])
      view_elevation -= glm::radians(.15f);
  };

  while (true) {
    for (SDL_Event event; SDL_PollEvent(&event);) {
      switch (event.type) {
        case SDL_QUIT:running = false;
          break;
        case SDL_WINDOWEVENT:
          switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED:width = event.window.data1;
              height = event.window.data2;
              glViewport(0, 0, width, height);
              break;
          }
          break;
        case SDL_KEYDOWN:button_down[event.key.keysym.sym] = true;

          if (event.key.keysym.sym == SDLK_SPACE)
            paused = !paused;

          break;
        case SDL_KEYUP:button_down[event.key.keysym.sym] = false;
          break;
      }
    }
    if (!running) break;

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
    last_frame_start = now;
    if (!paused)
      time += dt;
    update_view_parameters(dt);

    glm::mat4 model(1.f);
    glUseProgram(program);
    glClearColor(0.8, 0.8, 0.9, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    glViewport(0, 0, width, height);

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    float near = 0.01f;
    float far = 5000.f;

    glm::mat4 view(1.f);

//    view = glm::rotate(view, view_azimuth, {0.f, 1.f, 1.f});
    view = glm::translate(view, {y, 0.f, -camera_distance});
    // view = glm::rotate(view, view_elevation, {1.f, 0.f, 0.f});
    glm::mat4 projection = glm::mat4(1.f);
    projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);
    glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 2.f, std::cos(time * 0.5f)));
    glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

    glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
    glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
    glUniform3f(albedo_location, .8f, .7f, .6f);
    glUniform3f(sun_color_location, 1.f, 1.f, 1.f);
    glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));
    glUniform3fv(camera_position_location, 1, (float *) (&camera_position));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, scene.vertices.size());

    SDL_GL_SwapWindow(window);
  }

  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
  return 0;
}
catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
