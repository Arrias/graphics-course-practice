#ifdef WIN32
#include <SDL.h>
#undef main
#else

#include <SDL2/SDL.h>

#endif

#include <GL/glew.h>

#define TINYOBJLOADER_IMPLEMENTATION
#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include "stb_image.h"
#include "utils/utils.h"
#include "obj_parser.h"
#include "gltf_loader.hpp"

#include <rapiragl/rapiragl.h>
#include <iostream>
#include <random>

using Shader = rapiragl::components::Shader;
using TextureLoader = rapiragl::components::TextureLoader;

const auto wolf_len = 0.2f;

int main() try {
    // *** init window
    auto [window, gl_context] = CreateWindowContext();

    // *** init loader
    auto &TextureLoader_ = TextureLoader::GetInstance();

    // *** init state
    int width_, height_;
    SDL_GetWindowSize(window, &width_, &height_);
    auto State = PState(width_, height_);
    std::string root = "/home/rapiralove/studying/graphics-course-practice/2022/hw3";

    // *** создаем шейдеры сцены
    auto sphere_shader = Shader::GenShader(
            Shader::ShaderPaths{
                    .vertex = FilePath{root + "/shaders/sphere_vertex_shader_source.vert"},
                    .fragment = FilePath{root + "/shaders/sphere_fragment_shader_source.frag"},
                    .geometry = std::nullopt
            });

    GLuint env_vao;
    glGenVertexArrays(1, &env_vao);
    GLuint sphere_vao, sphere_vbo, sphere_ebo;
    int sphere_index_count;
    std::tie(sphere_vao, sphere_vbo, sphere_ebo, sphere_index_count) = GenSphereBuffers();

    // env_map program
    auto env_shader = Shader::GenShader(
            Shader::ShaderPaths{
                    .vertex = FilePath{root + "/shaders/env_vertex_shader_source.vert"},
                    .fragment = FilePath{root + "/shaders/env_fragment_shader_source.frag"},
                    .geometry = std::nullopt
            }
    );

    auto SetTextureSettings = [&]() {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    };

    auto normal_texture = TextureLoader_.GetTexture(FilePath{root + "/textures/brick_normal.jpg"}, SetTextureSettings);
    auto environment_texture = TextureLoader_.GetTexture(FilePath{root + "/textures/environment_map.jpg"},
                                                         SetTextureSettings);
    auto snow_texture = TextureLoader_.GetTexture(FilePath{root + "/textures/snow.jpeg"});

    /// *** Грузим корову TODO: сделать нормально
    auto cow = parse_obj(root + "/cow/cow.obj");

    GLuint cow_vao, cow_vbo, cow_ebo;
    glGenVertexArrays(1, &cow_vao);
    glGenBuffers(1, &cow_vbo);
    glGenBuffers(1, &cow_ebo);

    glBindVertexArray(cow_vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cow_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint32_t) * cow.indices.size(), cow.indices.data(), GL_STATIC_DRAW);

    glBindBuffer(GL_ARRAY_BUFFER, cow_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(obj_data::vertex) * cow.vertices.size(), cow.vertices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (sizeof(float) * 6));

    auto cow_texture = TextureLoader_.GetTexture(
            FilePath{root + "/cow/cow.png"}, []() {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
            });

    auto snowflake_texture = TextureLoader_.GetTexture(
            FilePath{root + "/textures/snowflake2.png"}, []() {
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                                GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                                GL_LINEAR);
            });

    /// *** Шейдер для рисования сцены
    auto main_shader = Shader::GenShader(Shader::ShaderPaths{
            .vertex = FilePath{root + "/shaders/main_vertex_shader_source.vert"},
            .fragment = FilePath{root + "/shaders/main_fragment_shader_source.frag"},
            .geometry = std::nullopt
    });

    /// *** Всё для пола
    auto floor = generate_floor(1.f, 100);

    GLuint floor_vao, floor_vbo;
    glGenVertexArrays(1, &floor_vao);
    glGenBuffers(1, &floor_vbo);
    glBindVertexArray(floor_vao);
    glBindBuffer(GL_ARRAY_BUFFER, floor_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(obj_data::vertex) * floor.size(), floor.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) nullptr);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (sizeof(float) * 3));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(obj_data::vertex), (void *) (sizeof(float) * 6));

    /// *** Fbo для тени от солнца
    const int sun_texture_unit = 100; // переименовать, непонятное название
    assert(sun_texture_unit < TextureLoader_.GetMaxTextureUnits());
    GLsizei shadow_map_resolution = 1024;
    GLuint shadow_map;
    glGenTextures(1, &shadow_map);
    glActiveTexture(GL_TEXTURE0 + sun_texture_unit);
    glBindTexture(GL_TEXTURE_2D, shadow_map);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RG32F, shadow_map_resolution, shadow_map_resolution, 0, GL_RGBA, GL_FLOAT,
                 nullptr);

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

    /// *** шейдер для тени
    auto shadow_shader = Shader::GenShader(Shader::ShaderPaths{
            .vertex = FilePath{root + "/shaders/shadow_vertex_shader_source.vert"},
            .fragment = FilePath{root + "/shaders/shadow_fragment_shader_source.frag"},
            .geometry = std::nullopt
    });

    /// *** считаем bbox
    auto bbox = CalcBoundingBox(std::vector<std::vector<obj_data::vertex>>{cow.vertices, floor});

    /// *** Дебажный шейдер
    auto debug_shader = Shader::GenShader(Shader::ShaderPaths{
            .vertex = FilePath{root + "/shaders/debug_vertex_shader_source.vert"},
            .fragment = FilePath{root + "/shaders/debug_fragment_shader_source.frag"},
            .geometry = std::nullopt
    });

    /// *** Туман
    auto fog_shader = Shader::GenShader(Shader::ShaderPaths{
            .vertex = FilePath{root + "/shaders/fog_vertex_shader_source.vert"},
            .fragment = FilePath{root + "/shaders/fog_fragment_shader_source.frag"},
            .geometry = std::nullopt
    });

    GLuint fog_vao, fog_vbo, fog_ebo;
    int cube_size;
    std::tie(fog_vao, fog_vbo, fog_ebo, cube_size) = GenFogBuffers();
    auto fog_texture = Load3dTexture(root + "/cloud.data");

    /////////////////////////////
    GLuint debug_vao;
    glGenVertexArrays(1, &debug_vao);

    auto const wolf_model = load_gltf(root + "/wolf/Wolf-Blender-2.82a.gltf");
    GLuint wolf_vbo;
    glGenBuffers(1, &wolf_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, wolf_vbo);
    glBufferData(GL_ARRAY_BUFFER, wolf_model.buffer.size(), wolf_model.buffer.data(), GL_STATIC_DRAW);

    struct mesh {
        GLuint vao;
        gltf_model::accessor indices;
        gltf_model::material material;
    };

    auto setup_attribute = [](int index, gltf_model::accessor const &accessor, bool integer = false) {
        glEnableVertexAttribArray(index);
        if (integer)
            glVertexAttribIPointer(index, accessor.size, accessor.type, 0,
                                   reinterpret_cast<void *>(accessor.view.offset));
        else
            glVertexAttribPointer(index,
                                  accessor.size,
                                  accessor.type,
                                  GL_FALSE,
                                  0,
                                  reinterpret_cast<void *>(accessor.view.offset));
    };

    std::vector<mesh> meshes;
    for (auto const &mesh: wolf_model.meshes) {
        auto &result = meshes.emplace_back();
        glGenVertexArrays(1, &result.vao);
        glBindVertexArray(result.vao);

        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wolf_vbo);
        result.indices = mesh.indices;

        setup_attribute(0, mesh.position);
        setup_attribute(1, mesh.normal);
        setup_attribute(2, mesh.texcoord);
        setup_attribute(3, mesh.joints, true);
        setup_attribute(4, mesh.weights);

        result.material = mesh.material;
    }

    for (const auto &mesh: meshes) {
        if (!mesh.material.texture_path) continue;

        auto path = FilePath{root + "/wolf/" + *mesh.material.texture_path};
        TextureLoader_.GetTexture(
                path, []() {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
                });
    }

    const auto &run_animation = wolf_model.animations.at("01_Run");

    /// *** Снежинки
    auto snowflake_shader = Shader::GenShader(Shader::ShaderPaths{
            .vertex = FilePath{root + "/shaders/snowflake_vertex_shader_source.vert"},
            .fragment = FilePath{root + "/shaders/snowflake_fragment_shader_source.frag"},
            .geometry = FilePath{root + "/shaders/snowflake_geometry_shader_source.geom"}
    });

    GLuint snowflake_vao, snowflake_vbo;
    std::tie(snowflake_vao, snowflake_vbo) = GenSnowflakeBuffers();

    /// ***********************************  END OF PREDGEN  *************************************************
    auto cow_model = glm::translate(glm::scale(glm::mat4(1.f), {0.4, 0.4, 0.4}), {0.f, 0.5f, 0.f});

    auto get_wolf_model_mat = [&](float time) {
        auto wolf_model_mat = glm::scale(glm::mat4(1.f), {0.7, 0.7, 0.7});
        wolf_model_mat = glm::translate(wolf_model_mat, {0.f, 0.f, wolf_len / 2.f});
        wolf_model_mat = glm::rotate(wolf_model_mat, time, {0.f, 1.f, 0.f});
        wolf_model_mat = glm::translate(glm::mat4(1.f), {-cos(time) * 0.7, 0.f, sin(time) * 0.7}) * wolf_model_mat;
        return wolf_model_mat;
    };

    const glm::vec3 cloud_bbox_min{-2.f, -1.f, -1.f};
    const glm::vec3 cloud_bbox_max{2.f, 1.f, 1.f};

    auto draw_scene = [&](
            float far,
            glm::mat4x4 model,
            glm::mat4x4 view,
            glm::mat4x4 projection,
            glm::mat4x4 shadow_transform,
            glm::vec3 light_direction,
            glm::vec3 camera_position,
            const std::vector<glm::mat4x3> &bones
    ) {
        /// *** Рисуем корову
        glBindVertexArray(cow_vao);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        main_shader.Use();
        main_shader.Set("is_wolf", (int) 0);
        main_shader.Set("far_plane", far);
        main_shader.Set("model", cow_model);
        main_shader.Set("view", view);
        main_shader.Set("projection", projection);
        main_shader.Set("sun_color", {.7f, .7f, .7f});
        main_shader.Set("sun_direction", light_direction);
        main_shader.Set("camera_position", camera_position);
        main_shader.Set("sampler", (int) cow_texture.texture_unit);
        main_shader.Set("power", 0.5f);
        main_shader.Set("glossiness", 0.3f);
        main_shader.Set("ambient_light", State.ambient_light);
        main_shader.Set("transform", shadow_transform);
        main_shader.Set("shadow_map", (int) sun_texture_unit);

        glDrawElements(GL_TRIANGLES, cow.indices.size(), GL_UNSIGNED_INT, (void *) nullptr);

        // *** Рисуем пол
        main_shader.Set("sampler", (int) snow_texture.texture_unit);
        main_shader.Set("model", glm::mat4(1.f));
        glBindVertexArray(floor_vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, floor.size());

        // WOLF
        main_shader.Set("is_wolf", (int) 1);
        main_shader.Set("model", get_wolf_model_mat(State.time));
        main_shader.Set("bones", bones.size(), bones.data());

        auto draw_meshes = [&](bool transparent) {
            for (auto const &mesh: meshes) {
                if (mesh.material.transparent != transparent)
                    continue;

                if (mesh.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                if (transparent)
                    glEnable(GL_BLEND);
                else
                    glDisable(GL_BLEND);

                if (mesh.material.texture_path) {
                    auto path = FilePath{root + "/wolf/" + *mesh.material.texture_path};
                    main_shader.Set("albedo", (int) TextureLoader_.GetTexture(path).texture_unit);
                    main_shader.Set("use_texture", (int) 1);
                } else if (mesh.material.color) {
                    main_shader.Set("use_texture", (int) 0);
                    main_shader.Set("color", *mesh.material.color);
                } else
                    continue;

                glBindVertexArray(mesh.vao);
                glDrawElements(GL_TRIANGLES,
                               mesh.indices.count,
                               mesh.indices.type,
                               reinterpret_cast<void *>(mesh.indices.view.offset));
            }
        };

        draw_meshes(false);
        glDepthMask(GL_FALSE);
        draw_meshes(true);
        glDepthMask(GL_TRUE);

        /// *** Рисуем снежинки
        glPointSize(5.f);
        //glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        //glDisable(GL_DEPTH_TEST);
        glBindVertexArray(snowflake_vao);
        glBindBuffer(GL_ARRAY_BUFFER, snowflake_vbo);
        glBufferData(GL_ARRAY_BUFFER, State.particles.size() * sizeof(particle), State.particles.data(),
                     GL_STATIC_DRAW);
        snowflake_shader.Use();
        snowflake_shader.Set("col", 1);
        snowflake_shader.Set("model", glm::mat4(1.f));
        snowflake_shader.Set("view", view);
        snowflake_shader.Set("projection", projection);
        snowflake_shader.Set("camera_position", camera_position);
        snowflake_shader.Set("sampler", (int) snowflake_texture.texture_unit);
        snowflake_shader.Set("snow", (int) snow_texture.texture_unit);
        snowflake_shader.Set("shadow_map", (int) sun_texture_unit);
        snowflake_shader.Set("transform", shadow_transform);
        glDrawArrays(GL_POINTS, 0, 4 * State.particles.size());

        // *** Рисуем туман
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_FRONT);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        fog_shader.Use();
        fog_shader.Set("view", view);
        fog_shader.Set("projection", projection);
        fog_shader.Set("bbox_min", cloud_bbox_min);
        fog_shader.Set("bbox_max", cloud_bbox_max);
        fog_shader.Set("camera_position", camera_position);
        fog_shader.Set("light_direction", light_direction);
        fog_shader.Set("model", glm::mat4(1.f));
        fog_shader.Set("shadow_map", sun_texture_unit);
        fog_shader.Set("transform", shadow_transform);

        glBindVertexArray(sphere_vao);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, nullptr);

        // *** Рисуем ball
        glEnable(GL_BLEND);
        glBlendEquation(GL_FUNC_ADD);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glEnable(GL_DEPTH_TEST);
        sphere_shader.Use();
        sphere_shader.Set("model", model);
        sphere_shader.Set("view", view);
        sphere_shader.Set("projection", projection);
        sphere_shader.Set("light_direction", light_direction);
        sphere_shader.Set("camera_position", camera_position);
        sphere_shader.Set("albedo_texture", (int) snow_texture.texture_unit);
        sphere_shader.Set("normal_texture", (int) normal_texture.texture_unit);
        sphere_shader.Set("environment_texture", (int) environment_texture.texture_unit);
        glBindVertexArray(sphere_vao);
        glDrawElements(GL_TRIANGLES, sphere_index_count, GL_UNSIGNED_INT, nullptr);

        glDisable(GL_BLEND);
    };

    auto GetBones = [&]() {
        std::vector<glm::mat4x3> bones(wolf_model.bones.size());
        float frame_run = std::fmod(State.time, run_animation.max_time);

        for (int i = 0; i < wolf_model.bones.size(); ++i) {
            auto cur_translation = glm::translate(glm::mat4(1.f), run_animation.bones[i].translation(frame_run));
            auto cur_scale = glm::scale(glm::mat4(1.f), run_animation.bones[i].scale(frame_run));
            auto cur_rotation = glm::toMat4(run_animation.bones[i].rotation(frame_run));
            auto cur_transform = cur_translation * cur_rotation * cur_scale;
            if (wolf_model.bones[i].parent != -1) {
                cur_transform = bones[wolf_model.bones[i].parent] * cur_transform;
            }
            bones[i] = cur_transform;
        }
        for (int i = 0; i < wolf_model.bones.size(); ++i) {
            bones[i] = bones[i] * wolf_model.bones[i].inverse_bind_matrix;
        }
        return bones;
    };

    while (State.running) {
        if (!State.tick()) break;

        auto bones = GetBones();
        float near = 0.1f;
        float far = 100.f;
        float top = near;

        glm::mat4 model = glm::mat4(1.f);
        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -State.camera_distance});
        view = glm::rotate(view, State.view_elevation, {1.f, 0.f, 0.f});
        view = glm::rotate(view, State.view_azimuth, {0.f, 1.f, 0.f});
        glm::mat4 projection = glm::mat4(1.f);
        projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * State.width) / State.height, near, far);
        glm::vec3 light_direction = glm::normalize(
                glm::vec3(std::sin(State.time * 0.5f) * 3, 2.f, std::cos(State.time * 0.5f) * 3));
        glm::vec3 camera_position = (glm::inverse(view) * glm::vec4(0.f, 0.f, 0.f, 1.f)).xyz();

        /// *** в начале нарисуем все в shadow_map
        glm::mat4x4 transform; // ай-ай-ай, как плохо...
        {
            glBindFramebuffer(GL_FRAMEBUFFER, shadow_fbo);
            glViewport(0, 0, shadow_map_resolution, shadow_map_resolution);
            glClearColor(1.f, 1.f, 0.f, 0.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LEQUAL);
            glEnable(GL_CULL_FACE);
            glCullFace(GL_BACK);

            transform = GetSunShadowTransform(bbox.vertices, bbox.C, light_direction);
            shadow_shader.Use();
            shadow_shader.Set("is_wolf", 0);
            shadow_shader.Set("transform", transform);
            shadow_shader.Set("model", cow_model);
            glBindVertexArray(cow_vao);
            glDrawElements(GL_TRIANGLES, cow.indices.size(), GL_UNSIGNED_INT, (void *) nullptr);

            shadow_shader.Set("model", model);
            glBindVertexArray(floor_vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, floor.size());

            shadow_shader.Set("is_wolf", 1);
            shadow_shader.Set("bones", bones.size(), bones.data());
            shadow_shader.Set("model", get_wolf_model_mat(State.time));

            for (auto const &mesh: meshes) {
                glBindVertexArray(mesh.vao);

                if (mesh.material.two_sided)
                    glDisable(GL_CULL_FACE);
                else
                    glEnable(GL_CULL_FACE);

                glDrawElements(GL_TRIANGLES,
                               mesh.indices.count,
                               mesh.indices.type,
                               reinterpret_cast<void *>(mesh.indices.view.offset));
            }

            glDisable(GL_CULL_FACE);
            glBindTexture(GL_TEXTURE_2D, shadow_map);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glViewport(0, 0, State.width, State.height);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glClearColor(1.0, 0.0, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // *** Рисуем env
        glDisable(GL_DEPTH_TEST);
        env_shader.Use();
        auto view_projection_inverse = glm::inverse(projection * view);
        env_shader.Set("view_projection_inverse", view_projection_inverse);
        env_shader.Set("env", (int) environment_texture.texture_unit);
        env_shader.Set("camera_position", camera_position);
        env_shader.Set("lightness", State.env_lightness);
        glBindVertexArray(env_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        /// *** Рисуем сцену
        draw_scene(far, model, view, projection, transform, light_direction, camera_position, bones);

        /// *** Рисуем дебажный прямоугольник
        debug_shader.Use();
        debug_shader.Set("sampler", sun_texture_unit);
        glBindVertexArray(debug_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
