#ifdef WIN32
#include <SDL.h>
#undef main
#else
#include <SDL2/SDL.h>
#endif

#include <GL/glew.h>

#define LOG(x) std::cout << "[" << #x << "] = " << x << std::endl;

#include <string_view>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <random>
#include <map>
#include <cmath>

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/string_cast.hpp>

#include "gltf_loader.hpp"
#include "stb_image.h"
#include "aabb.hpp"
#include "frustum.hpp"
#include "intersect.hpp"

std::string to_string(std::string_view str) {
  return std::string(str.begin(), str.end());
}

void sdl2_fail(std::string_view message) {
  throw std::runtime_error(to_string(message) + SDL_GetError());
}

void glew_fail(std::string_view message, GLenum error) {
  throw std::runtime_error(to_string(message) + reinterpret_cast<const char *>(glewGetErrorString(error)));
}

const char vertex_shader_source[] =
    R"(#version 330 core

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec2 in_texcoord;
layout (location = 3) in vec3 shift;

out vec3 normal;
out vec2 texcoord;

void main()
{
    gl_Position = projection * view * model * vec4(in_position + shift, 1.0);
    normal = mat3(model) * in_normal;
    texcoord = in_texcoord;
}
)";

const char fragment_shader_source[] =
    R"(#version 330 core

uniform sampler2D albedo;

uniform vec3 light_direction;

layout (location = 0) out vec4 out_color;

in vec3 normal;
in vec2 texcoord;

void main()
{
    vec3 albedo_color = texture(albedo, texcoord).rgb;

    float ambient = 0.4;
    float diffuse = max(0.0, dot(normalize(normal), light_direction));

    out_color = vec4(albedo_color * (ambient + diffuse), 1.0);
}
)";

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

template<typename ... Shaders>
GLuint create_program(Shaders ... shaders) {
  GLuint result = glCreateProgram();
  (glAttachShader(result, shaders), ...);
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

int main() try {
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

  SDL_Window *window = SDL_CreateWindow("Graphics course practice 11",
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

  auto vertex_shader = create_shader(GL_VERTEX_SHADER, vertex_shader_source);
  auto fragment_shader = create_shader(GL_FRAGMENT_SHADER, fragment_shader_source);
  auto program = create_program(vertex_shader, fragment_shader);

  GLuint model_location = glGetUniformLocation(program, "model");
  GLuint view_location = glGetUniformLocation(program, "view");
  GLuint projection_location = glGetUniformLocation(program, "projection");
  GLuint albedo_location = glGetUniformLocation(program, "albedo");
  GLuint color_location = glGetUniformLocation(program, "color");
  GLuint use_texture_location = glGetUniformLocation(program, "use_texture");
  GLuint light_direction_location = glGetUniformLocation(program, "light_direction");
  GLuint bones_location = glGetUniformLocation(program, "bones");

  const std::string project_root = PROJECT_ROOT;
  const std::string model_path = project_root + "/bunny/bunny.gltf";

  auto const input_model = load_gltf(model_path);
  GLuint vbo;
  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, input_model.buffer.size(), input_model.buffer.data(), GL_STATIC_DRAW);

  std::vector<GLuint> vaos;
  for (int i = 0; i < input_model.meshes.size(); ++i) {
    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo);

    auto setup_attribute = [](int index, gltf_model::accessor const &accessor) {
      glEnableVertexAttribArray(index);
      glVertexAttribPointer(index,
                            accessor.size,
                            accessor.type,
                            GL_FALSE,
                            0,
                            reinterpret_cast<void *>(accessor.view.offset));
    };

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    setup_attribute(0, input_model.meshes[i].position);
    setup_attribute(1, input_model.meshes[i].normal);
    setup_attribute(2, input_model.meshes[i].texcoord);

    vaos.push_back(vao);
  }

  GLuint texture;
  {
    auto const &mesh = input_model.meshes[0];

    auto path = std::filesystem::path(model_path).parent_path() / *mesh.material.texture_path;

    int width, height, channels;
    auto data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    assert(data);

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);

    stbi_image_free(data);
  }

  auto last_frame_start = std::chrono::high_resolution_clock::now();
  float time = 0.f;
  std::map<SDL_Keycode, bool> button_down;
  glm::vec3 camera_position{0.f, 1.5f, 3.f};
  float camera_rotation = 0.f;
  bool paused = false;
  bool running = true;

  // TASK 1
  std::vector<std::uint32_t> qo_ids;
  std::vector<std::uint8_t> qo_free;

  auto GetQo = [&]() {
    for (int i = 0; i < qo_ids.size(); ++i) {
      if (qo_free[i]) {
        qo_free[i] = false;
        return qo_ids[i];
      }
    }
    GLuint co;
    glGenQueries(1, &co);
    qo_ids.push_back(co);
    qo_free.push_back(false);
    return co;
  };

  // TASK 3
  const int LOD_CNT = 6;
  std::vector<glm::vec3> shifts[LOD_CNT]{};

  GLuint shifts_vbo;
  glGenBuffers(1, &shifts_vbo);
  glBindBuffer(GL_ARRAY_BUFFER, shifts_vbo);
  //glBufferData(GL_ARRAY_BUFFER, shifts.size() * sizeof(glm::vec3), shifts.data(), GL_STATIC_DRAW);

  for (const auto &vao : vaos) {
    glBindVertexArray(vao);
    glEnableVertexAttribArray(3);
    glBindBuffer(GL_ARRAY_BUFFER, shifts_vbo);
    glVertexAttribPointer(3,
                          3,
                          GL_FLOAT,
                          GL_FALSE,
                          0,
                          nullptr);
    glVertexAttribDivisor(3, 1);
  }

  while (running) {
    for (SDL_Event event; SDL_PollEvent(&event);)
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

    if (!running)
      break;

    // TASK 1
    auto qo_id = GetQo();
    glBeginQuery(GL_TIME_ELAPSED, qo_id);

    auto now = std::chrono::high_resolution_clock::now();
    float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
    last_frame_start = now;

    if (!paused)
      time += dt;

    float camera_move_forward = 0.f;
    float camera_move_sideways = 0.f;

    if (button_down[SDLK_w])
      camera_move_forward -= 3.f * dt;
    if (button_down[SDLK_s])
      camera_move_forward += 3.f * dt;
    if (button_down[SDLK_a])
      camera_move_sideways -= 3.f * dt;
    if (button_down[SDLK_d])
      camera_move_sideways += 3.f * dt;

    if (button_down[SDLK_LEFT])
      camera_rotation -= 3.f * dt;
    if (button_down[SDLK_RIGHT])
      camera_rotation += 3.f * dt;

    if (button_down[SDLK_DOWN])
      camera_position.y -= 3.f * dt;
    if (button_down[SDLK_UP])
      camera_position.y += 3.f * dt;

    camera_position += camera_move_forward * glm::vec3(-std::sin(camera_rotation), 0.f, std::cos(camera_rotation));
    camera_position += camera_move_sideways * glm::vec3(std::cos(camera_rotation), 0.f, std::sin(camera_rotation));

    glClearColor(0.8f, 0.8f, 1.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    float near = 0.1f;
    float far = 100.f;

    glm::mat4 view(1.f);
    view = glm::rotate(view, camera_rotation, {0.f, 1.f, 0.f});
    view = glm::translate(view, -camera_position);
    glm::mat4 projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);
    glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();
    glm::vec3 light_direction = glm::normalize(glm::vec3(1.f, 2.f, 3.f));
    glm::mat4 model = glm::mat4(1.f);

    auto frustum_ = frustum(projection * view);

    glUseProgram(program);
    glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
    glUniformMatrix4fv(view_location, 1, GL_FALSE, reinterpret_cast<float *>(&view));
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, reinterpret_cast<float *>(&projection));
    glUniform3fv(light_direction_location, 1, reinterpret_cast<float *>(&light_direction));

    glBindTexture(GL_TEXTURE_2D, texture);

    {
      //auto const &mesh = input_model.meshes[0];
      //glBindVertexArray(vaos[0]);

      for (int i = 0; i < LOD_CNT; ++i)
        shifts[i].clear();
      // TASK 4
      for (int dx = -16; dx < 16; ++dx) {
        for (int dz = -16; dz < 16; ++dz) {
          auto shift = glm::vec3(dx, 0, dz);
          auto shift_min = input_model.meshes[0].min + shift;
          auto shift_max = input_model.meshes[0].max + shift;
          auto center = (shift_min + shift_max) / 2.f;
          auto length = glm::length(camera_position - center);
          auto C = 4.f;
          auto lod = std::min(LOD_CNT - 1, (int) floor(length / C));
          shifts[lod].emplace_back(shift);
        }
      }

      for (int lod = 0; lod < LOD_CNT; ++lod) {
        std::vector<glm::vec3> cur_shifts;
        const auto &mesh = input_model.meshes[lod];

        for (auto shift : shifts[lod]) {
          aabb aabb_ = aabb(mesh.min + shift, mesh.max + shift);
          if (intersect(aabb_, frustum_)) {
            cur_shifts.emplace_back(shift);
          }
        }

        glBindVertexArray(vaos[lod]);
        glBindBuffer(GL_ARRAY_BUFFER, shifts_vbo);
        glBufferData(GL_ARRAY_BUFFER, cur_shifts.size() * sizeof(glm::vec3), cur_shifts.data(), GL_STATIC_DRAW);
        LOG(cur_shifts.size());

        glDrawElementsInstanced(GL_TRIANGLES,
                                mesh.indices.count,
                                mesh.indices.type,
                                reinterpret_cast<void *>(mesh.indices.view.offset),
                                cur_shifts.size());

      }
      //LOG(shifts.size());

      // TASK 2
//      for (int dx = -16; dx <= 15; ++dx) {
//        for (int dz = -16; dz <= 15; ++dz) {
//          glm::mat4 model = glm::translate(glm::mat4(1.f), {dx, 0, dz});
//          glUniformMatrix4fv(model_location, 1, GL_FALSE, reinterpret_cast<float *>(&model));
//          glDrawElements(GL_TRIANGLES,
//                         mesh.indices.count,
//                         mesh.indices.type,
//                         reinterpret_cast<void *>(mesh.indices.view.offset));
//        }
//      }

// TASK 3
//      glDrawElementsInstanced(GL_TRIANGLES,
//                              mesh.indices.count,
//                              mesh.indices.type,
//                              reinterpret_cast<void *>(mesh.indices.view.offset),
//                              shifts.size());
    }

    glEndQuery(GL_TIME_ELAPSED);

    SDL_GL_SwapWindow(window);

    for (int i = 0; i < qo_ids.size(); ++i) {
      if (qo_free[i]) continue;
      int is_ready;
      glGetQueryObjectiv(qo_ids[i], GL_QUERY_RESULT_AVAILABLE, reinterpret_cast<GLint *>(&is_ready));
      if (is_ready) {
        int result;
        glGetQueryObjectiv(qo_ids[i], GL_QUERY_RESULT, &result);
        float render_time = (float) result / 100000.f;
        LOG(render_time);
        qo_free[i] = true;
      }
    }

  }

  LOG(qo_ids.size());
  SDL_GL_DeleteContext(gl_context);
  SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
  std::cerr << e.what() << std::endl;
  return EXIT_FAILURE;
}
