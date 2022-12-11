#pragma once

#include <optional>
#include <unordered_map>

#include <rapiragl/common/types.h>
#include <rapiragl/components/file_reader/file_reader.h>
#include <GL/glew.h>

#define GLM_FORCE_SWIZZLE
#define GLM_ENABLE_EXPERIMENTAL

#include "glm/vec3.hpp"
#include "glm/mat4x4.hpp"
#include "glm/ext/matrix_transform.hpp"
#include "glm/ext/matrix_clip_space.hpp"
#include "glm/ext/scalar_constants.hpp"
#include "glm/gtx/string_cast.hpp"

namespace rapiragl::components {
class Shader {
public:
    struct ShaderPaths {
        const FilePath &vertex;
        const FilePath &fragment;
        const std::optional<FilePath> &geometry;
    };

    explicit Shader(const ShaderPaths &shader_paths);

    void Use();

    void Set(const std::string &name, float value);

    void Set(const std::string &name, const glm::mat4x4 &m);

    void Set(const std::string &name, const glm::vec3 &m);

    void Set(const std::string &name, int value);

    void Set(const std::string &name, const glm::vec4 &v);

    void Set(const std::string &name, int cnt, const glm::mat4x3 *m);

    void Set1f(const std::string &name, float value);

    void SetMat4x4(const std::string &name, const glm::mat4x4 &m);

    void SetVec3(const std::string &name, const glm::vec3 &v);

    void SetVec4(const std::string &name, const glm::vec4 &v);

    void Set1i(const std::string &name, int value);

    static Shader GenShader(const FilePath &path, bool with_geom = false);

    static Shader GenShader(const ShaderPaths &shader_paths);

private:
    std::unordered_map<std::string, GLuint> location;
    GLUid program;

    inline GLuint GetLocation(const std::string &name);

    static GLUid CreateShader(GLenum type, const std::string &src);

    static GLUid CreateProgram(GLUid vertex_shader, GLUid fragment_shader,
                               std::optional<GLUid> geometry_shader = std::nullopt);

    static GLUid CreateProgram(const ShaderPaths &shader_paths);
};
}