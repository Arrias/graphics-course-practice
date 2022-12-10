#pragma once

#include <array>
#include <vector>
#include <unordered_map>

#include "../../common/types.h"
#include "glm/vec3.hpp"
#include "tinyobj/tiny_obj_loader.hpp"

namespace rapiragl::components {

class ArraySceneLoader {
public:
    struct SceneParameters {
        const FilePath &obj_path;
        const FilePath &mtl_path;
    };

    struct Scene {
        struct vertex {
            std::array<float, 3> position;
            std::array<float, 3> normal;
            std::array<float, 2> texcoord;
        };

        struct Segment {
            size_t l, r;
        };

        std::vector<Scene::vertex> vertices;
        std::vector<float> glossiness;
        std::vector<float> power;

        std::vector<glm::vec3> bounding_box;
        glm::vec3 C;
        std::vector<size_t> alpha_ids;
    };

    static Scene LoadScene(SceneParameters parameters);

    static Scene ParseScene(const tinyobj::attrib_t &attrib,
                            const std::vector<tinyobj::shape_t> &shapes,
                            const std::vector<tinyobj::material_t> &materials, const FilePath &mtl_path);

};

}