#include "shader.h"

namespace rapiragl::components {

Shader::Shader(const ShaderPaths &shader_paths) : program(
        CreateProgram(shader_paths)) {
}

GLUid Shader::CreateShader(GLenum type, const std::string &src) {
    const char *source = src.data();
    GLuint result = glCreateShader(type);
    glShaderSource(result, 1, &source, nullptr);
    glCompileShader(result);
    GLint status;
    glGetShaderiv(result, GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE) {
        GLint info_log_length;
        glGetShaderiv(result, GL_INFO_LOG_LENGTH, &info_log_length);
        std::string info_log(info_log_length, '\0');
        glGetShaderInfoLog(result, (GLsizei) info_log.size(), nullptr, info_log.data());
        throw std::runtime_error("Shader compilation failed: " + info_log);
    }
    return GLUid{result};
}

GLUid Shader::CreateProgram(GLUid vertex_shader, GLUid fragment_shader,
                            std::optional<GLUid> geometry_shader) {
    GLuint result = glCreateProgram();
    glAttachShader(result, vertex_shader.GetUnderLying());
    glAttachShader(result, fragment_shader.GetUnderLying());
    if (geometry_shader.has_value()) {
        glAttachShader(result, geometry_shader->GetUnderLying());
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

    return GLUid{result};
}

GLUid Shader::CreateProgram(const ShaderPaths &shader_paths) {
    auto vertex_shader = CreateShader(GL_VERTEX_SHADER, FileReader::read(shader_paths.vertex));
    auto fragment_shader = CreateShader(GL_FRAGMENT_SHADER, FileReader::read(shader_paths.fragment));
    std::optional<GLUid> geometry_shader = std::nullopt;
    if (shader_paths.geometry.has_value()) {
        geometry_shader = CreateShader(GL_GEOMETRY_SHADER, FileReader::read(*shader_paths.geometry));
    }
    return CreateProgram(vertex_shader, fragment_shader, geometry_shader);
}

void Shader::Use() {
    glUseProgram(program.GetUnderLying());
}

void Shader::Set1f(const std::string &name, float value) {
    glUniform1f(GetLocation(name), value);
}

void Shader::SetMat4x4(const std::string &name, const glm::mat4x4 &m) {
    glUniformMatrix4fv(GetLocation(name), 1, GL_FALSE,
                       reinterpret_cast<const GLfloat *>(&m));
}

void Shader::SetVec3(const std::string &name, const glm::vec3 &v) {
    glUniform3fv(GetLocation(name), 1,
                 reinterpret_cast<const GLfloat *>(&v));
}

void Shader::Set1i(const std::string &name, int value) {
    glUniform1i(GetLocation(name), value);
}

GLuint Shader::GetLocation(const std::string &name) {
    if (location.find(name) != location.end()) {
        return location[name];
    }
    return location[name] = glGetUniformLocation(program.GetUnderLying(), name.data());
}


Shader Shader::GenShader(const FilePath &path, bool with_geom) {
    // TODO: arbitrary names
    auto vertex_shader_path = FilePath{path.GetUnderLying() + "/vertex.vert"};
    auto fragment_shader_path = FilePath{path.GetUnderLying() + "/fragment.frag"};
    std::optional<FilePath> geometry_shader_path = std::nullopt;

    if (with_geom) {
        geometry_shader_path = path.GetUnderLying() + "/geom.geom";
    }

    return Shader(Shader::ShaderPaths{
            .vertex = vertex_shader_path,
            .fragment = fragment_shader_path,
            .geometry = geometry_shader_path
    });
}

Shader Shader::GenShader(const Shader::ShaderPaths &shader_paths) {
    return Shader(shader_paths);
}

void Shader::Set(const std::string &name, float value) {
    Set1f(name, value);
}

void Shader::Set(const std::string &name, const glm::mat4x4 &m) {
    SetMat4x4(name, m);
}

void Shader::Set(const std::string &name, const glm::vec3 &m) {
    SetVec3(name, m);
}

void Shader::Set(const std::string &name, int value) {
    Set1i(name, value);
}

}
