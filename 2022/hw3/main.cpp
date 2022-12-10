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

#include <rapiragl/rapiragl.h>
#include <chrono>
#include <iostream>

using Shader = rapiragl::components::Shader;
using TextureLoader = rapiragl::components::TextureLoader;
using ArraySceneLoader = rapiragl::components::ArraySceneLoader;

struct BoundingBox {
    std::vector<glm::vec3> vertices;
    glm::vec3 C;
};

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

int main() try {
    auto [window, gl_context] = CreateWindowContext();

    int width, height;
    SDL_GetWindowSize(window, &width, &height);
    glClearColor(1.0, 0.0, 0.f, 0.f);

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

    auto &TextureLoader_ = TextureLoader::GetInstance();
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
    const int sun_texture_unit = 170; // переименовать, непонятное название
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

    GLuint debug_vao;
    glGenVertexArrays(1, &debug_vao);

    auto last_frame_start = std::chrono::high_resolution_clock::now();

    float time = 0.f;

    std::map<SDL_Keycode, bool> button_down;

    float view_elevation = glm::radians(30.f);
    float view_azimuth = 0.f;
    float camera_distance = 2.f;
    float ambient_light = 0.f;
    float env_lightness = 1.f;

    bool running = true;
    bool paused = false;

    auto cow_model = glm::translate(glm::scale(glm::mat4(1.f), {0.5, 0.5, 0.5}), {0.f, 0.5f, 0.f});

    auto draw_scene = [&](
            float far,
            glm::mat4x4 model,
            glm::mat4x4 view,
            glm::mat4x4 projection,
            glm::mat4x4 shadow_transform,
            glm::vec3 light_direction,
            glm::vec3 camera_position
    ) {
        glBindVertexArray(cow_vao);
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);

        main_shader.Use();
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
        main_shader.Set("ambient_light", ambient_light);
        main_shader.Set("transform", shadow_transform);
        main_shader.Set("shadow_map", (int) sun_texture_unit);

        glDrawElements(GL_TRIANGLES, cow.indices.size(), GL_UNSIGNED_INT, (void *) nullptr);

        // *** Рисуем пол
        main_shader.Set("sampler", (int) snow_texture.texture_unit);
        main_shader.Set("model", glm::mat4(1.f));
        glBindVertexArray(floor_vao);
        glDrawArrays(GL_TRIANGLE_FAN, 0, floor.size());

        // *** Рисуем ball
        {   // *** включаем блендинг
            glEnable(GL_BLEND);
            glBlendEquation(GL_FUNC_ADD);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

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

    while (running) {
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

        if (!running)
            break;

        auto now = std::chrono::high_resolution_clock::now();
        float dt = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_frame_start).count();
        last_frame_start = now;

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

        float near = 0.1f;
        float far = 100.f;
        float top = near;
        float right = (top * width) / height;

        glm::mat4 model = glm::mat4(1.f);
        glm::mat4 view(1.f);
        view = glm::translate(view, {0.f, 0.f, -camera_distance});
        view = glm::rotate(view, view_elevation, {1.f, 0.f, 0.f});
        view = glm::rotate(view, view_azimuth, {0.f, 1.f, 0.f});
        glm::mat4 projection = glm::mat4(1.f);
        projection = glm::perspective(glm::pi<float>() / 2.f, (1.f * width) / height, near, far);
        glm::vec3 light_direction = glm::normalize(glm::vec3(std::sin(time * 0.5f) * 3, 2.f, std::cos(time * 0.5f) * 3 ));
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
            shadow_shader.Set("transform", transform);

            shadow_shader.Set("model", cow_model);
            glBindVertexArray(cow_vao);
            glDrawElements(GL_TRIANGLES, cow.indices.size(), GL_UNSIGNED_INT, (void *) nullptr);

            shadow_shader.Set("model", model);
            glBindVertexArray(floor_vao);
            glDrawArrays(GL_TRIANGLE_FAN, 0, floor.size());

            glDisable(GL_CULL_FACE);

            glBindTexture(GL_TEXTURE_2D, shadow_map);
            glGenerateMipmap(GL_TEXTURE_2D);
        }

        glViewport(0, 0, width, height);
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
        env_shader.Set("lightness", env_lightness);
        glBindVertexArray(env_vao);
        glDrawArrays(GL_TRIANGLES, 0, 6);
//
//        /// *** Рисуем сцену
        draw_scene(far, model, view, projection, transform, light_direction, camera_position);
//
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
