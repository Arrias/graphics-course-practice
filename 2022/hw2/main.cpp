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

#include "obj_parser.hpp"

const std::string log_path = "../log.txt";

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

int main(int argc, char **argv) try {
    std::freopen(log_path.data(), "w", stderr);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        sdl2_fail("SDL_Init: ");
    if (argc < 2) {
        throw std::runtime_error("Error: please, specify scene path");
    }
    const auto scene_path = std::string{argv[1]};
    Logger::log("[scene_path] =", scene_path);

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn;
    std::string err;

    bool ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, scene_path.data());

    Logger::log("[warn] =", warn);
    Logger::log("[err] =", err);


    return 0;
}
catch (std::exception const &e) {
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
}
