
#include <iostream>
#include "array_scene_loader.h"
#include "tinyobj/tiny_obj_loader.hpp"
#include "../../components/texture_loader/texture_loader.h"
#include <iostream>

namespace rapiragl::components {


ArraySceneLoader::Scene ArraySceneLoader::LoadScene(ArraySceneLoader::SceneParameters parameters) {
    const auto scene_path = parameters.obj_path.GetUnderLying();
    const auto mtl_path = parameters.mtl_path.GetUnderLying();

    tinyobj::ObjReaderConfig reader_config;
    reader_config.mtl_search_path = mtl_path;
    tinyobj::ObjReader reader;

    if (!reader.ParseFromFile(scene_path, reader_config)) {
        if (!reader.Error().empty()) {
            throw std::runtime_error("TinyObjReader: " + reader.Error());
        }
        throw;
    }

    std::cout << reader.Warning() << std::endl;

    auto &attrib = reader.GetAttrib();
    auto &shapes = reader.GetShapes();
    auto &materials = reader.GetMaterials();
    return ParseScene(attrib, shapes, materials, parameters.mtl_path);
}

ArraySceneLoader::Scene
ArraySceneLoader::ParseScene(const tinyobj::attrib_t &attrib, const std::vector<tinyobj::shape_t> &shapes,
                             const std::vector<tinyobj::material_t> &materials, const FilePath &mtl_path) {
    auto &texture_loader =
            rapiragl::components::TextureLoader::GetInstance();


    std::vector<Scene::vertex> vertices;
    std::vector<Scene::Segment> segments;
    std::vector<size_t> texture_unit_ids;
    std::vector<float> glossiness;
    std::vector<float> power;
    std::vector<size_t> alpha_ids;

    float x[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};
    float y[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};
    float z[2] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::min()};

    auto process_path = [&](std::string path) {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    };

    for (const auto &shape: shapes) {
        size_t index_offset = 0;
        auto start_index = vertices.size();
        auto material_id = shape.mesh.material_ids[0];

        auto texname = process_path(materials[material_id].ambient_texname);
        std::cout << texname << std::endl;

        texture_unit_ids.push_back(
                texture_loader.GetTexture(
                        FilePath{mtl_path.GetUnderLying() + texname}).texture_unit);
//        glossiness.push_back(materials[material_id].specular[0]);
//        power.push_back(materials[material_id].shininess);
//        alpha_ids.push_back(texture_loader.GetTexture(
//                FilePath{root + "/" + clear_path(materials[material_id].alpha_texname)}).texture_unit);

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

                vertices.push_back(Scene::vertex{
                        .position = {vx, vy, vz},
                        .normal = {nx, ny, nz},
                        .texcoord = {tx, ty},
                });
            }

            index_offset += fv;
            shape.mesh.material_ids[f];
        }

        auto finish_index = vertices.size();
        segments.push_back(Scene::Segment{
                .l = start_index,
                .r = finish_index
        });
    }

    std::vector<glm::vec3> bounding_box;
    for (float i: x)
        for (float j: y)
            for (float k: z)
                bounding_box.emplace_back(i, j, k);

    glm::vec3 C{(x[0] + x[1]) / 2, (y[0] + y[1]) / 2, (z[0] + z[1]) / 2};

    return Scene{
            .vertices = vertices,
//            .segments = segments,
//            .texture_ids = texture_unit_ids,
            .glossiness = glossiness,
            .power = power,
            .bounding_box = bounding_box,
            .C = C,
            .alpha_ids = alpha_ids
    };
}

}