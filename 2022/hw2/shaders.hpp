#pragma once
#include <string>
#include <cassert>

#include "stb_image.h"

const std::string vertex_shader_source = R"(
    #version 330 core

    uniform mat4 model;
    uniform mat4 view;
    uniform mat4 projection;

    layout (location = 0) in vec3 in_position;
    layout (location = 1) in vec3 in_normal;
    layout (location = 2) in vec2 in_texcoord;

    out vec3 position;
    out vec3 normal;
    out vec2 texcoord;

    void main() {
        gl_Position = projection * view * model * vec4(in_position, 1.0);

        position = (model * vec4(in_position, 1.0)).xyz;
        normal = normalize((model * vec4(in_normal, 0.0)).xyz);
        texcoord = vec2(in_texcoord.x, 1 - in_texcoord.y);
    }
)";

const std::string fragment_shader_source = R"(
    #version 330 core

    in vec3 position;
    in vec3 normal;
    in vec2 texcoord;

    // удаленный источник
    uniform vec3 sun_color;
    uniform vec3 camera_position;
    uniform vec3 sun_direction;

    // точечный источник
    uniform float glossiness;
    uniform float power;
    uniform vec3 point_light_attenuation;
    uniform vec3 point_light_color;
    uniform vec3 point_light_position;
    uniform mat4 transform;

    uniform samplerCube depthMap;
    uniform float far_plane;

    float ShadowCalculation() {
        vec3 fragToLight = position - point_light_position;
        //return texture(depthMap, fragToLight).r;;
        float closestDepth = texture(depthMap, fragToLight).r;
        closestDepth *= far_plane;
        float currentDepth = length(fragToLight);
//        //if (currentDepth < 100.f) {
//          //  return 0.0;
//        //}
        float bias = 1.5;
        float shadow = currentDepth -  bias > closestDepth ? 0.0 : 1.0;
        return shadow;
    }

    uniform sampler2D sampler;
    uniform sampler2D shadow_map;

    layout (location = 0) out vec4 out_color;

    vec3 diffuse(vec3 direction, vec3 albedo) {
      return albedo * max(0.0, dot(normal, direction));
    }

    vec3 specular(vec3 direction, vec3 albedo) {
        vec3 reflected_direction = 2.0 * normal *  dot(normal, direction) - direction;
        vec3 view_direction = normalize(camera_position - position);
        return glossiness * albedo * pow(max(0.0, dot(reflected_direction, view_direction)), power);
    }

    float C = 0.005;

    void main() {
        //  float zz = gl_FragCoord.z;
        //  out_color = vec4(zz, 0.0, 0.0, 0.0);
         // return;

          // SUN SHADOW
          vec4 shadow_pos = transform * vec4(position, 1.0);
          shadow_pos /= shadow_pos.w;
          shadow_pos = shadow_pos * 0.5 + vec4(0.5);

          bool in_shadow_texture = (shadow_pos.x > 0) && (shadow_pos.x < 1) &&
                    (shadow_pos.y > 0) && (shadow_pos.y < 1) &&
                    (shadow_pos.z > 0) && (shadow_pos.z < 1);



          vec2 sum = vec2(0.0, 0.0);
          int r = 3;
          float sum_weight = 0;

          for (int x = -r; x <= r; ++x) {
              for (int y = -r; y <= r; ++y) {
                  float expo = exp(-(x*x+y*y)/8.);
                  sum += expo * texture(shadow_map, shadow_pos.xy+vec2(x,y) / textureSize(shadow_map, 0).xy).rg;
                  sum_weight += expo;
              }
          }

          vec2 data = sum / sum_weight;

          float mu = data.r;
          float sigma = data.g - mu * mu;
          float z = shadow_pos.z - C;
          float factor = (z < mu) ? 1.0 :
                  sigma / (sigma + (z - mu) * (z - mu));
          float l = 0.125;
          if (factor < l) {
              factor = 0.0;;
          } else {
              factor = (factor - l) / (1 - l);
          }

          float shadow_factor = 1.0;
          if (in_shadow_texture)
              shadow_factor = factor;

          vec3 albedo = texture(sampler, texcoord).xyz;
          vec3 point_light_direction = point_light_position - position;
          float dist = length(point_light_direction);
          point_light_direction /= dist;

          float c0 = point_light_attenuation.x;
          float c1 = point_light_attenuation.y;
          float c2 = point_light_attenuation.z;
          float attenuation = 1 / (c0 + c1 * dist + c2 * dist * dist);
          float attenuation_limit = 1.5f;
          if (attenuation > attenuation_limit) attenuation = attenuation_limit;


          float ambient_light = 0.2;
          vec3 ambient = albedo * ambient_light;
          vec3 color = ambient;

          color += diffuse(sun_direction, albedo) * sun_color * shadow_factor;
          color += specular(sun_direction, albedo) * sun_color * shadow_factor;

          float point_shadow = ShadowCalculation();
          color += diffuse(point_light_direction, albedo) * point_light_color * attenuation * point_shadow;
          color += specular(point_light_direction, albedo) * point_light_color * attenuation * point_shadow;

          out_color = vec4(color, 1.0);
    }
)";

const std::string shadow_vertex_shader_source =
    R"(#version 330 core
uniform mat4 model;
uniform mat4 transform;
layout (location = 0) in vec3 in_position;
 layout (location = 2) in vec2 in_texcoord;

out vec2 texcoord;
void main()
{
    gl_Position = transform * model * vec4(in_position, 1.0);
    texcoord = vec2(in_texcoord.x, 1 - in_texcoord.y);
}
)";

const std::string shadow_fragment_shader_source =
    R"(#version 330 core
out vec4 out_color;
uniform sampler2D map_d;
in vec2 texcoord;
void main()
{
    if(texture2D(map_d, texcoord).x > 0.5) discard;
    float z = gl_FragCoord.z;
    float dF_dx = dFdx(z);
    float dF_dy = dFdy(z);
    float add = 0.25 * (dF_dx * dF_dx + dF_dy * dF_dy);
    out_color = vec4(z, z * z + add, 0.0, 0.0);
}
)";

const std::string point_shadow_vertex_shader_source =
    R"(
    #version 330 core
    layout (location = 0) in vec3 in_position;

    uniform mat4 model;

    void main()
    {
        gl_Position = model * vec4(in_position, 1.0);
    }

    )";

const std::string point_shadow_geometry_shader_source =
    R"(
    #version 330 core
    layout (triangles) in;
    layout (triangle_strip, max_vertices=18) out;

    out vec4 FragPos;
    uniform mat4 shadowMatrices[6];

    void main() {
        for (int face = 0; face < 6; ++face) {
            gl_Layer = face;
            for (int i = 0; i < 3; ++i) {
                FragPos = gl_in[i].gl_Position;
                gl_Position = shadowMatrices[face] * FragPos;
                EmitVertex();
            }
            EndPrimitive();
        }
    }

    )";

const std::string point_shadow_fragment_shader_source =
    R"(
    #version 330 core

    in vec4 FragPos;
    uniform vec3 lightPos;
    uniform float far_plane;

    void main() {
        float lightDistance = length(FragPos.xyz-lightPos);
        lightDistance /= far_plane;
        gl_FragDepth = lightDistance;
    }
    )";

const char debug_vertex_shader_source[] =
    R"(#version 330 core
vec2 vertices[6] = vec2[6](
    vec2(-1.0, -1.0),
    vec2( 1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0, -1.0),
    vec2( 1.0,  1.0),
    vec2(-1.0,  1.0)
);
out vec2 texcoord;
void main()
{
    vec2 position = vertices[gl_VertexID];
    gl_Position = vec4(position * 0.25 + vec2(-0.75, -0.75), 0.0, 1.0);
    texcoord = position * 0.5 + vec2(0.5);
}
)";

const char debug_fragment_shader_source[] =
    R"(#version 330 core
uniform samplerCube sampler_cube;
in vec2 texcoord;
layout (location = 0) out vec4 out_color;
void main()
{
    vec3 cubemap_pos = vec3(texcoord.x ,-10.0, texcoord.y);
    out_color = vec4(texture(sampler_cube, cubemap_pos).rgb, 1.0);
}
)";

namespace Logger {
template<typename T>
void log(T n) {
#ifdef debug
  std::cout << n << std::endl;
  std::cerr << n << std::endl;
#endif
}

template<typename T, typename ...Args>
void log(const T &n, const Args &... rest) {
#ifdef debug
  std::cout << n << " ";
  std::cerr << n << " ";
  log(rest...);
#endif
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

GLuint create_program(GLuint vertex_shader, GLuint fragment_shader,
                      std::optional<GLuint> geometry_shader = std::nullopt) {
  GLuint result = glCreateProgram();
  glAttachShader(result, vertex_shader);
  glAttachShader(result, fragment_shader);
  if (geometry_shader.has_value()) {
    glAttachShader(result, *geometry_shader);
  }

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

using TextureKeeper = std::map<std::string, GLuint>;

TextureKeeper loadTextures(const std::string &path) {
  TextureKeeper result;
  for (int i = 0; const auto &dirEntry : std::filesystem::directory_iterator(path)) {
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0 + i);
    assert(i < 90);

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);

    int width;
    int height;
    int comp;
    stbi_uc *pixels = stbi_load(dirEntry.path().c_str(), &width, &height, &comp, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    std::string current_path = dirEntry.path().string();
    std::string rel_path;

    for (int j = (int) current_path.size() - 1, cnt = 0; j >= 0 && cnt < 2; --j) {
      if (current_path[j] == '/') {
        ++cnt;
        if (cnt < 2) {
          rel_path += '\\';
        }
      } else {
        rel_path += current_path[j];
      }
    }
    std::reverse(rel_path.begin(), rel_path.end());
    result[rel_path] = i++;
  };

  return result;
}