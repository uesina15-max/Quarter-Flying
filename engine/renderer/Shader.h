#pragma once
#include <string>
#include <unordered_map>
#include <glm/mat4x4.hpp>
#include <glm/mat3x3.hpp>
#include <glm/vec3.hpp>

namespace Engine
{
    class Shader
    {
    public:
        Shader() = default;
        ~Shader();

        bool load(const std::string& vertexPath, const std::string& fragmentPath);
        void use() const;

        void setMat4(const std::string& name, const glm::mat4& value);
        void setMat3(const std::string& name, const glm::mat3& value);
        void setVec3(const std::string& name, const glm::vec3& value);
        void setFloat(const std::string& name, float value);
        void setInt(const std::string& name, int value);

        unsigned int id() const { return m_program; }

    private:
        std::string readFile(const std::string& path);
        unsigned int compile(unsigned int type, const std::string& source);
        int getUniformLocation(const std::string& name);

    private:
        unsigned int m_program = 0;
        std::unordered_map<std::string, int> m_uniformCache;
    };
}
