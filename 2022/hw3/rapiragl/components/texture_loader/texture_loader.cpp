#include "texture_loader.h"
#include "stb_image.h"

namespace rapiragl::components {

TextureLoader::TextureInfo TextureLoader::GetTexture(const FilePath &path, std::function<void(void)> settings) {
    auto raw_path = path.GetUnderLying();
    if (texture_keeper.find(raw_path) != texture_keeper.end()) {
        return texture_keeper.at(raw_path);
    }

    size_t fresh_unit = texture_keeper.size();
    if (fresh_unit > GetMaxTextureUnits()) {
        throw std::runtime_error("Textures number great then max texture unit num");
    }
    GLuint texture;
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0 + fresh_unit);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
    settings();

    int width, height, comp;
    stbi_uc *pixels = stbi_load(raw_path.data(), &width, &height, &comp, 4);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    stbi_image_free(pixels);

    texture_keeper.insert({raw_path, TextureInfo{
            .id = GLUid{texture},
            .texture_unit = fresh_unit,
            .width = width,
            .height = height,
            .comp = comp
    }});

    return texture_keeper.at(raw_path);
}

int TextureLoader::GetMaxTextureUnits() {
    int texture_units;
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &texture_units);
    return texture_units;
}

TextureLoader &TextureLoader::GetInstance() {
    static TextureLoader instance;
    return instance;
}

TextureLoader::TextureLoader() = default;

}