#include "Shader.h"
#include <glad/glad.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

std::string Shader::readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "[Shader] No se pudo abrir: " << path << "\n";
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

unsigned int Shader::compile(unsigned int type, const std::string& source, const std::string& debugName) {
    unsigned int shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        int len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(shader, len, nullptr, log.data());
        std::cerr << "[Shader] Error compilando " << debugName << ":\n" << log.data() << "\n";
    }
    return shader;
}

Shader::Shader(const std::string& vertPath, const std::string& fragPath) {
    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);

    unsigned int vert = compile(GL_VERTEX_SHADER, vertSrc, vertPath);
    unsigned int frag = compile(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    programId = glCreateProgram();
    glAttachShader(programId, vert);
    glAttachShader(programId, frag);
    glLinkProgram(programId);

    int success = 0;
    glGetProgramiv(programId, GL_LINK_STATUS, &success);
    if (!success) {
        int len = 0;
        glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(programId, len, nullptr, log.data());
        std::cerr << "[Shader] Error linkeando programa:\n" << log.data() << "\n";
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

Shader::~Shader() {
    if (programId) glDeleteProgram(programId);
}

void Shader::use() const { glUseProgram(programId); }

void Shader::setMat4(const std::string& name, const glm::mat4& m) const {
    glUniformMatrix4fv(glGetUniformLocation(programId, name.c_str()), 1, GL_FALSE, &m[0][0]);
}

void Shader::setVec3(const std::string& name, const glm::vec3& v) const {
    glUniform3fv(glGetUniformLocation(programId, name.c_str()), 1, &v[0]);
}

void Shader::setFloat(const std::string& name, float v) const {
    glUniform1f(glGetUniformLocation(programId, name.c_str()), v);
}

void Shader::setInt(const std::string& name, int v) const {
    glUniform1i(glGetUniformLocation(programId, name.c_str()), v);
}
