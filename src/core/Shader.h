#pragma once
#include <string>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertPath, const std::string& fragPath);
    ~Shader();

    void use() const;

    void setMat4(const std::string& name, const glm::mat4& m) const;
    void setVec3(const std::string& name, const glm::vec3& v) const;
    void setFloat(const std::string& name, float v) const;
    void setInt(const std::string& name, int v) const;

    unsigned int id() const { return programId; }

private:
    unsigned int programId = 0;

    static std::string readFile(const std::string& path);
    static unsigned int compile(unsigned int type, const std::string& source, const std::string& debugName);
};
