#pragma once

#include <unordered_map>
#include <functional>
#include <memory>

#include <GL/glew.h>

#include "../../common/types.h"

namespace rapiragl::components {

class TextureLoader {
public:
    struct TextureInfo {
        GLUid id;
        size_t texture_unit;
        int width, height, comp;
    };

    TextureInfo GetTexture(const FilePath &path, std::function<void(void)> settings = []() {});

    static int GetMaxTextureUnits();

    static TextureLoader &GetInstance();

    TextureLoader(TextureLoader const &) = delete;

    void operator=(TextureLoader const &) = delete;

private:
    std::unordered_map<std::string, TextureInfo> texture_keeper;

    TextureLoader();
};

}