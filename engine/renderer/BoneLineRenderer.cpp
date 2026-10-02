#include "BoneLineRenderer.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>

namespace Engine
{
    namespace
    {
        const char* kBoneLineVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;

uniform mat4 uViewProjection;

void main()
{
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
}
)GLSL";

        const char* kBoneLineFragmentShader = R"GLSL(
#version 330 core
uniform vec3 uColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(uColor, 1.0);
}
)GLSL";
    }

    BoneLineRenderer::~BoneLineRenderer()
    {
        Shutdown();
    }

    unsigned int BoneLineRenderer::CompileShaderStage(unsigned int type, const char* source)
    {
        unsigned int shader = glCreateShader(type);
        glShaderSource(shader, 1, &source, nullptr);
        glCompileShader(shader);

        int success = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char log[1024] = {};
            glGetShaderInfoLog(shader, sizeof(log), nullptr, log);
            Logger::Log(LogLevel::Error, "BoneLineRenderer - Shader compile failed: {}", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    bool BoneLineRenderer::Initialize()
    {
        if (initialized)
        {
            return true;
        }

        unsigned int vs = CompileShaderStage(GL_VERTEX_SHADER, kBoneLineVertexShader);
        unsigned int fs = CompileShaderStage(GL_FRAGMENT_SHADER, kBoneLineFragmentShader);
        if (vs == 0 || fs == 0)
        {
            if (vs) glDeleteShader(vs);
            if (fs) glDeleteShader(fs);
            return false;
        }

        shaderProgram = glCreateProgram();
        glAttachShader(shaderProgram, vs);
        glAttachShader(shaderProgram, fs);
        glLinkProgram(shaderProgram);

        int linked = 0;
        glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linked);
        glDeleteShader(vs);
        glDeleteShader(fs);

        if (!linked)
        {
            char log[1024] = {};
            glGetProgramInfoLog(shaderProgram, sizeof(log), nullptr, log);
            Logger::Log(LogLevel::Error, "BoneLineRenderer::Initialize - Shader link failed: {}", log);
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            return false;
        }

        uniformLocViewProjection = glGetUniformLocation(shaderProgram, "uViewProjection");
        uniformLocColor = glGetUniformLocation(shaderProgram, "uColor");

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        if (vao == 0 || vbo == 0)
        {
            Logger::Log(LogLevel::Error, "BoneLineRenderer::Initialize - Failed to generate VAO/VBO");
            Shutdown();
            return false;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), reinterpret_cast<void*>(0));
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        initialized = true;
        Logger::Log(LogLevel::Info, "BoneLineRenderer::Initialize - Ready");
        return true;
    }

    void BoneLineRenderer::Shutdown()
    {
        if (vbo)
        {
            glDeleteBuffers(1, &vbo);
            vbo = 0;
        }
        if (vao)
        {
            glDeleteVertexArrays(1, &vao);
            vao = 0;
        }
        if (shaderProgram)
        {
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
        }
        initialized = false;
    }

    void BoneLineRenderer::Render(const glm::mat4& viewProjection, const std::vector<std::pair<glm::vec3, glm::vec3>>& lines,
                                  const glm::vec3& color)
    {
        if (!initialized || lines.empty())
        {
            return;
        }

        std::vector<glm::vec3> vertices;
        vertices.reserve(lines.size() * 2);
        for (const auto& line : lines)
        {
            vertices.push_back(line.first);
            vertices.push_back(line.second);
        }

        GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        if (!depthWasEnabled)
        {
            glEnable(GL_DEPTH_TEST);
        }

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uniformLocViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));
        glUniform3f(uniformLocColor, color.x, color.y, color.z);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        // 포즈가 매 프레임 바뀌므로(정적 그리드와 다름) 매번 다시 채운다. 본 개수가
        // 수백 단위를 넘기 전까지는 이 방식으로 충분하다.
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(glm::vec3)),
                     vertices.data(), GL_DYNAMIC_DRAW);

        glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertices.size()));
        glBindVertexArray(0);
        glUseProgram(0);

        if (!depthWasEnabled)
        {
            glDisable(GL_DEPTH_TEST);
        }
    }
}
