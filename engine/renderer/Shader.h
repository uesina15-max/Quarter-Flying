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

        // load()와 달리 파일 경로를 거치지 않고 GLSL 소스 문자열을 바로 컴파일한다 -
        // 상대경로가 프로세스 CWD 기준이라 에디터(engine/editor/에서 실행)에서 항상
        // 깨지는 문제(DebugGridRenderer/BoneLineRenderer가 겪었던 것과 동일)를 원천적으로
        // 피하기 위한 경로. 내부 컴파일/링크 로직은 load()와 동일하게 공유한다.
        bool loadFromSource(const std::string& vertexSrc, const std::string& fragmentSrc);

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
