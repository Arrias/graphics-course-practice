#ifdef WIN32
#include <SDL.h>
#undef main
#else

#define TINYOBJLOADER_IMPLEMENTATION
#define debug

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#include <filesystem>
#include <string_view>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <vector>
#include <map>
#include <cmath>
#include <fstream>
#include <sstream>

#include "shaders.hpp"
#include "tiny_obj_loader.hpp"
#include "stb_image.h"

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/string_cast.hpp>

const std::string log_path = "../log.txt";

const auto pi = (float) acos(-1);
const auto eps = (float) 0.01;

struct Segment {
  size_t l, r;
};

struct obj_data {
  struct vertex {
    std::array<float, 3> position;
    std::array<float, 3> normal;
    std::array<float, 2> texcoord;
    size_t texture_number;
  };

  std::vector<vertex> vertices;
  std::vector<Segment> segments; // for every shape [l..r) in vertices
  std::vector<size_t> texture_ids;

  std::vector<float> glossiness;
  std::vector<float> power;

  std::vector<glm::vec3> bounding_box;
  glm::vec3 C; // центр bounding_box
  std::vector<size_t> alpha_ids;
};

obj_data parse_scene(const tinyobj::attrib_t &attrib,
                     const std::vector<tinyobj::shape_t> &shapes,
                     const std::vector<tinyobj::material_t> &materials,
                     TextureKeeper &texture_keeper) {
  std::vector<obj_data::vertex> vertices;
  std::vector<Segment> segments;
  std::vector<size_t> texture_unit_ids;
  std::vector<float> glossiness;
  std::vector<float> power;
  std::vector<size_t> alpha_ids;

  float x[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};
  float y[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};
  float z[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};

  for (const auto &shape : shapes) {
    size_t index_offset = 0;
    auto start_index = vertices.size();
    auto material_id = shape.mesh.material_ids[0];

    texture_unit_ids.push_back(texture_keeper[materials[material_id].ambient_texname]);
    glossiness.push_back(materials[material_id].specular[0]);
    power.push_back(materials[material_id].shininess);
    alpha_ids.push_back(texture_keeper[materials[material_id].alpha_texname]);

    for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); ++f) {
      auto fv = size_t(shape.mesh.num_face_vertices[f]);

      for (size_t v = 0; v < fv; ++v) {
        tinyobj::index_t idx = shape.mesh.indices[index_offset + v];

        tinyobj::real_t vx = attrib.vertices[3 * size_t(idx.vertex_index) + 0];
        tinyobj::real_t vy = attrib.vertices[3 * size_t(idx.vertex_index) + 1];
        tinyobj::real_t vz = attrib.vertices[3 * size_t(idx.vertex_index) + 2];

        x[0] = std::min(x[0], vx);
        x[1] = std::max(x[1], vx);

        y[0] = std::min(y[0], vy);
        y[1] = std::max(y[1], vy);

        z[0] = std::min(z[0], vz);
        z[1] = std::max(z[1], vz);

        assert(idx.normal_index >= 0);
        tinyobj::real_t nx = attrib.normals[3 * size_t(idx.normal_index) + 0];
        tinyobj::real_t ny = attrib.normals[3 * size_t(idx.normal_index) + 1];
        tinyobj::real_t nz = attrib.normals[3 * size_t(idx.normal_index) + 2];

        assert(idx.texcoord_index >= 0);
        tinyobj::real_t tx = attrib.texcoords[2 * size_t(idx.texcoord_index) + 0];
        tinyobj::real_t ty = attrib.texcoords[2 * size_t(idx.texcoord_index) + 1];

        vertices.push_back(obj_data::vertex{
            .position = {vx, vy, vz},
            .normal = {nx, ny, nz},
            .texcoord = {tx, ty},
        });
      }

      index_offset += fv;
      shape.mesh.material_ids[f];
    }

    auto finish_index = vertices.size();
    segments.push_back(Segment{
        .l = start_index,
        .r = finish_index
    });
  }

  std::vector<glm::vec3> bounding_box;
  for (float i : x)
    for (float j : y)
      for (float k : z)
        bounding_box.emplace_back(i, j, k);

  glm::vec3 C{(x[0] + x[1]) / 2, (y[0] + y[1]) / 2, (z[0] + z[1]) / 2};

  return obj_data{
      .vertices = vertices,
      .segments = segments,
      .texture_ids = texture_unit_ids,
      .glossiness = glossiness,
      .power = power,
      .bounding_box = bounding_box,
      .C = C,
      .alpha_ids = alpha_ids
  };
}

struct Player {
  glm::vec3 coords{0.f, 0.f, -900.f};
  float alpha{}, gamma{};

  void update(std::map<SDL_Keycode, bool> &button_down, float dt) {
    float d_x = 500.f * sin(alpha) * sin(gamma);
    float d_z = 500.f * cos(alpha) * sin(gamma);
    float d_y = 500.f * cos(gamma);

    if (button_down[SDLK_UP]) {
      coords.x += d_x * dt;
      coords.y += d_y * dt;
      coords.z += d_z * dt;
    }
    if (button_down[SDLK_DOWN]) {
      coords.x -= d_x * dt;
      coords.y -= d_y * dt;
      coords.z -= d_z * dt;
    }

    if (button_down[SDLK_LEFT])
      alpha += 1.5f * dt;
    if (button_down[SDLK_RIGHT])
      alpha -= 1.5f * dt;

    if (button_down[SDLK_g])
      gamma = std::max(gamma - 1.5f * dt, eps);
    if (button_down[SDLK_h])
      gamma = std::min(gamma + 1.5f * dt, pi - eps);
  }

};

int main(int argc, char **argv) try {
  std::freopen(log_path.data(), "w", stderr);
  if (SDL_Init(SDL_INIT_VIDEO) != 0)
    sdl2_fail("SDL_Init: ");

  // *** Рутина по созданию окна
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

  int texture_units;
  glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &texture_units);
  Logger::log("Count texture units =", texture_units);

  GLint maxAtt = 0;
  glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxAtt);
  Logger::log("Max color attachments =", maxAtt);

  // *** Создаем шейдеры сцены
  auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source.data());
  auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source.data());
  auto program = create_program(vertex_shader, fragment_shader);

  /// *** Создаем шейдеры для тени от точечного источника
  auto point_shadow_vertex_shader = create_shader(GL_VERTEX_SHADER, point_shadow_vertex_shader_source.data());
  auto point_shadow_geometry_shader = create_shader(GL_GEOMETRY_SHADER, point_shadow_geometry_shader_source.data());
  auto point_shadow_fragment_shader = create_shader(GL_FRAGMENT_SHADER, point_shadow_fragment_shader_source.data());
  auto point_shadow_program =
      create_program(point_shadow_vertex_shader, point_shadow_fragment_shader, point_shadow_geometry_shader);

  /// *** Создаем шейдеры для тени от солнца
  auto shadow_vertex_shader = create_shader(GL_VERTEX_SHADER, shadow_vertex_shader_source.data());
  auto shadow_fragment_shader = create_shader(GL_FRAGMENT_SHADER, shadow_fragment_shader_source.data());
  auto shadow_program = create_program(shadow_vertex_shader, shadow_fragment_shader);

  GLuint model_location = glGetUniformLocation(program, "model");
  GLuint view_location = glGetUniformLocation(program, "view");
  GLuint projection_location = glGetUniformLocation(program, "projection");
  GLuint sampler_location = glGetUniformLocation(program, "sampler");
  GLuint sun_direction_location = glGetUniformLocation(program, "sun_direction");
  GLuint sun_color_location = glGetUniformLocation(program, "sun_color");
  GLuint camera_position_location = glGetUniformLocation(program, "camera_position");
  GLuint power_location = glGetUniformLocation(program, "power");
  GLuint glossiness_location = glGetUniformLocation(program, "glossiness");
  GLuint point_light_position_location = glGetUniformLocation(program, "point_light_position");
  GLuint point_light_color_location = glGetUniformLocation(program, "point_light_color");
  GLuint point_light_attenuation_location = glGetUniformLocation(program, "point_light_attenuation");
  GLuint shadow_map_location = glGetUniformLocation(program, "shadow_map");
  GLuint transform_location = glGetUniformLocation(program, "transform");

  glUseProgram(program);

  if (argc < 2) {
    throw std::runtime_error("Error: please, specify scene path");
  }

  // *** Загрузка текстур и сцены
  const auto scene_path = std::string{argv[1]};
  std::string texture_path = scene_path;
  while (!texture_path.empty() && texture_path.back() != '/') texture_path.pop_back();
  std::string mtl_path = texture_path;
  texture_path += "textures";

  Logger::log("[scene_path] =", scene_path);
  Logger::log("[texture_path] =", texture_path);
  Logger::log("[mtl_path] =", mtl_path);

  auto textures = loadTextures(texture_path);
  uint32_t max_not_free_unit = 0;
  for (const auto &[i, j] : textures) {
    max_not_free_unit = std::max(max_not_free_unit, j);
  }
  Logger::log("[max_not_free_unit] =", max_not_free_unit);

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

  obj_data scene = parse_scene(attrib, shapes, materials, textures);

  // *** Привязываем сцену к vbo, vao, ebo
  GLuint vao, vbo;
  glGenVertexArrays(1, &vao);
  glGenBuffers(1, &vbo);

  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 0, 3, GL_FLOAT, GL_FALSE, (void *) nullptr); // точка
  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER, vbo, vao, 1, 3, GL_FLOAT, GL_FALSE, (void *) 12); // нормаль
  bindArgument<obj_data::vertex>(GL_ARRAY_BUFFER,
                                 vbo,
                                 vao,
                                 2,
                                 2,
                                 GL_FLOAT,
                                 GL_FALSE,
                                 (void *) 24); // текстурные координаты
  bindData(GL_ARRAY_BUFFER, vbo, vao, scene.vertices);

  // *** Тень от солнца
  GLuint shadow_model_location = glGetUniformLocation(shadow_program, "model");
  GLuint shadow_transform_location = glGetUniformLocation(shadow_program, "transform");
  GLuint shadow_map_d_location = glGetUniformLocation(shadow_program, "map_d");

  const int sun_texture_unit = 90; // переименовать, непонятное название
  GLsizei shadow_map_resolution = 1024;
  GLuint shadow_map;
  glGenTextures(1, &shadow_map);
  glActiveTexture(GL_TEXTURE0 + sun_texture_unit);
  glBindTexture(GL_TEXTURE_2D, shadow_map);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadow_map_resolution, shadow_map_resolution, 0, GL_RGBA, GL_FLOAT, nullptr);

  GLuint rbo;
  glGenRenderbuffers(1, &rbo);
  glBindRenderbuffer(GL_RENDERBUFFER, rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, shadow_map_resolution, shadow_map_resolution);

  GLuint shadow_fbo;
  glGenFramebuffers(1, &shadow_fbo);
  glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
  glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, shadow_map, 0);
  glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    throw std::runtime_error("Incomplete framebuffer!");

  // *** Дебажный прямоугольник
  auto debug_vertex_shader = create_shader(GL_VERTEX_SHADER, debug_vertex_shader_source);
  auto debug_fragment_shader = create_shader(GL_FRAGMENT_SHADER, debug_fragment_shader_source);
  auto debug_program = create_program(debug_vertex_shader, debug_fragment_shader);
  GLuint debug_sampler_cube_location = glGetUniformLocation(debug_program, "sampler_cube");
  GLuint debug_vao;
  glGenVertexArrays(1, &debug_vao);


  /// *** Тень от подвижной точки
  const int point_shadow_texture_unit = 95;
  GLuint point_shadow_model_location = glGetUniformLocation(point_shadow_program, "model");
  GLuint point_shadow_shadow_matrices_location = glGetUniformLocation(point_shadow_program, "shadowMatrices");
  GLuint point_shadow_light_pos_location = glGetUniformLocation(point_shadow_program, "lightPos");
  GLuint point_shadow_far_plane_location = glGetUniformLocation(point_shadow_program, "far_plane");

  GLsizei point_shadow_resolution = 512;

  float time = 0.f;
  std::map<SDL_Keycode, bool> button_down;
  auto last_frame_start = std::chrono::high_resolution_clock::now();
  Player player;

  bool running = true;
  bool paused = false;

  auto poll_events = [&running, &height, &width, &paused, &button_down]() {
    for (SDL_Event event; SDL_PollEvent(&event);) {
      switch (event.type) {
        case SDL_QUIT:running = false;
          break;
        case SDL_WINDOWEVENT:
          switch (event.window.event) {
            case SDL_WINDOWEVENT_RESIZED: width = event.window.data1;
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
  };

  std::vector<float> writedCube(point_shadow_resolution * point_shadow_resolution);
  int iter = 0;

  while (true) {
    ++iter;
    poll_events();
    if (!running) break;

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
    last_frame_start = now;
    if (!paused)
      time += dt;
    player.update(button_down, dt);

    glm::mat4 model(1.f);
    float near = 0.01f;
    float far = 5000.f;
    auto cords = player.coords;
    auto view = glm::lookAt(
        cords,
        cords + glm::vec3(sin(player.alpha) * sin(player.gamma),
                          cos(player.gamma),
                          cos(player.alpha) * sin(player.gamma)),
        glm::vec3(0, 1, 0)
    );
    glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);
    glm::vec3 sun_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f), 2.f, std::cos(time * 0.5f)));
    glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();
    glm::vec3 light_direction = sun_direction;

    auto light_z = -light_direction;
    auto light_x = glm::normalize(glm::cross(light_z, {0.f, 1.f, 0.f}));
    auto light_y = glm::cross(light_x, light_z);

    float dz = 0;
    float dx = 0;
    float dy = 0;
    auto C = scene.C;
    for (auto V : scene.bounding_box) {
      auto vec = V - C;
      dz = std::max(dz, glm::dot(vec, light_z));
      dx = std::max(dx, glm::dot(vec, light_x));
      dy = std::max(dy, glm::dot(vec, light_y));
    }

    glm::mat4 transform = glm::inverse(glm::mat4({
                                                     {dx * light_x.x, dx * light_x.y, dx * light_x.z, 0.f},
                                                     {dy * light_y.x, dy * light_y.y, dy * light_y.z, 0.f},
                                                     {dz * light_z.x, dz * light_z.y, dz * light_z.z, 0.f},
                                                     {C.x, C.y, C.z, 1.f}
                                                 }));

    auto point_light_position = glm::vec3(std::sin(time * 0.5f) * 1000, 100.f, std::cos(time * 0.5f) * 400);

    // Рисуем сцену в shadow_map солнца
    glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
    glClearColor(1.f, 1.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUseProgram(shadow_program);
    glUniformMatrix4fv(shadow_model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
    glUniformMatrix4fv(shadow_transform_location, 1, GL_FALSE, reinterpret_cast<float *>(&transform));
    glBindVertexArray(vao);

    for (int j = 0; const auto &i : scene.segments) {
      glUniform1i(shadow_map_d_location, scene.alpha_ids[j]);
      glDrawArrays(GL_TRIANGLES, i.l, i.r - i.l);
      j++;
    }
    glDrawArrays(GL_TRIANGLES, 0, scene.vertices.size());

    glBindTexture(GL_TEXTURE_2D, shadow_map);
    glGenerateMipmap(GL_TEXTURE_2D);

    // Рисуем сцену на экран
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glUseProgram(program);
    glClearColor(0.8, 0.8, 0.9, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glViewport(0, 0, width, height);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glUniform1i(shadow_map_location, sun_texture_unit);
    glUniformMatrix4fv(transform_location, 1, GL_FALSE, reinterpret_cast<float *>(&transform));
    glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
    glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
    glUniform3f(sun_color_location, .7f, .7f, .7f);
    glUniform3fv(sun_direction_location, 1, reinterpret_cast<float *>(&sun_direction));
    glUniform3fv(camera_position_location, 1, (float *) (&camera_position));
    glUniform3f(point_light_position_location, point_light_position.x, point_light_position.y, point_light_position.z);
    glUniform3f(point_light_color_location, 0.0f, 0.9f, 0.0f);
    glUniform3f(point_light_attenuation_location, 0.001f, 0.0001f, 0.0001f);
    glBindVertexArray(vao);

    for (int j = 0; const auto &i : scene.segments) {
      glUniform1i(sampler_location, scene.texture_ids[j]);
      glUniform1f(power_location, scene.power[j]);
      glUniform1f(glossiness_location, scene.glossiness[j]);
      glDrawArrays(GL_TRIANGLES, i.l, i.r - i.l);
      j++;
    }

//    glUseProgram(debug_program);
//    glUniform1i(debug_sampler_cube_location, point_shadow_texture_unit);
//    glBindVertexArray(debug_vao);
//    glDrawArrays(GL_TRIANGLES, 0, 6);
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
