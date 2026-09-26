#include "DebugGridRenderer.h"
#include "../core/logging/Logger.h"
#include <GL/glew.h>
#include <glm/gtc/type_ptr.hpp>
#include <vector>

namespace Engine
{
    namespace
    {
        const char* kGridVertexShader = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPosition;
layout(location = 1) in vec3 aColor;

uniform mat4 uViewProjection;

out vec3 vColor;

void main()
{
    gl_Position = uViewProjection * vec4(aPosition, 1.0);
    vColor = aColor;
}
)GLSL";

        const char* kGridFragmentShader = R"GLSL(
#version 330 core
in vec3 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vec4(vColor, 1.0);
}
)GLSL";

        struct GridVertex
        {
            float x, y, z;
            float r, g, b;
        };
    }

    DebugGridRenderer::~DebugGridRenderer()
    {
        Shutdown();
    }

    unsigned int DebugGridRenderer::CompileShaderStage(unsigned int type, const char* source)
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
            Logger::Log(LogLevel::Error, "DebugGridRenderer - Shader compile failed: {}", log);
            glDeleteShader(shader);
            return 0;
        }
        return shader;
    }

    bool DebugGridRenderer::Initialize(float halfSize, float spacing)
    {
        if (initialized)
        {
            return true;
        }

        if (spacing <= 0.0f || halfSize <= 0.0f)
        {
            Logger::Log(LogLevel::Error, "DebugGridRenderer::Initialize - Invalid halfSize/spacing");
            return false;
        }

        // --- 셰이더 컴파일 ---
        unsigned int vs = CompileShaderStage(GL_VERTEX_SHADER, kGridVertexShader);
        unsigned int fs = CompileShaderStage(GL_FRAGMENT_SHADER, kGridFragmentShader);
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
            Logger::Log(LogLevel::Error, "DebugGridRenderer::Initialize - Shader link failed: {}", log);
            glDeleteProgram(shaderProgram);
            shaderProgram = 0;
            return false;
        }

        uniformLocViewProjection = glGetUniformLocation(shaderProgram, "uViewProjection");

        // --- 그리드 라인 정점 생성 (XZ 평면, Y=0에 고정) ---
        std::vector<GridVertex> vertices;
        const float minorColor[3] = { 0.35f, 0.35f, 0.38f };
        const float xAxisColor[3] = { 0.75f, 0.28f, 0.28f };  // X축 강조 (빨강 계열)
        const float zAxisColor[3] = { 0.28f, 0.42f, 0.78f };  // Z축 강조 (파랑 계열)

        int lineCount = static_cast<int>(halfSize / spacing);
        vertices.reserve(static_cast<size_t>(lineCount) * 2 * 4 + 4);

        for (int i = -lineCount; i <= lineCount; ++i)
        {
            float t = static_cast<float>(i) * spacing;

            // Z축과 평행한 선 (x = t 고정, z는 -halfSize..halfSize)
            const float* colorAlongZ = (i == 0) ? zAxisColor : minorColor;
            vertices.push_back({ t, 0.0f, -halfSize, colorAlongZ[0], colorAlongZ[1], colorAlongZ[2] });
            vertices.push_back({ t, 0.0f,  halfSize, colorAlongZ[0], colorAlongZ[1], colorAlongZ[2] });

            // X축과 평행한 선 (z = t 고정, x는 -halfSize..halfSize)
            const float* colorAlongX = (i == 0) ? xAxisColor : minorColor;
            vertices.push_back({ -halfSize, 0.0f, t, colorAlongX[0], colorAlongX[1], colorAlongX[2] });
            vertices.push_back({ halfSize, 0.0f, t, colorAlongX[0], colorAlongX[1], colorAlongX[2] });
        }

        vertexCount = static_cast<int>(vertices.size());

        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);

        if (vao == 0 || vbo == 0)
        {
            Logger::Log(LogLevel::Error, "DebugGridRenderer::Initialize - Failed to generate VAO/VBO");
            Shutdown();
            return false;
        }

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(vertices.size() * sizeof(GridVertex)),
                     vertices.data(), GL_STATIC_DRAW);

        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), reinterpret_cast<void*>(0));

        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(GridVertex), reinterpret_cast<void*>(3 * sizeof(float)));

        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        initialized = true;
        Logger::Log(LogLevel::Info, "DebugGridRenderer::Initialize - Grid ready ({} vertices)", vertexCount);
        return true;
    }

    void DebugGridRenderer::Shutdown()
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

    void DebugGridRenderer::Render(const glm::mat4& viewProjection)
    {
        if (!initialized)
        {
            return;
        }

        GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        if (!depthWasEnabled)
        {
            glEnable(GL_DEPTH_TEST);
        }

        glUseProgram(shaderProgram);
        glUniformMatrix4fv(uniformLocViewProjection, 1, GL_FALSE, glm::value_ptr(viewProjection));

        glBindVertexArray(vao);
        glDrawArrays(GL_LINES, 0, vertexCount);
        glBindVertexArray(0);
        glUseProgram(0);

        if (!depthWasEnabled)
        {
            glDisable(GL_DEPTH_TEST);
        }
    }
}
