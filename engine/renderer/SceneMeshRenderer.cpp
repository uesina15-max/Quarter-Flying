#include "SceneMeshRenderer.h"
#include "InstancedBatchManager.h"
#include "Camera.h"
#include "../core/logging/Logger.h"
#include <glm/glm.hpp>
#include <GL/glew.h>

namespace Engine
{
    namespace
    {
        const char* kVertexSrc = R"GLSL(
#version 330 core
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aTexCoords;

// InstancedBatchManager::SetupInstanceVAO()가 채우는 인스턴스 attribute (divisor=1).
layout(location = 3) in mat4 aModel;          // 4개 vec4 attribute(3,4,5,6)로 mat4 구성
layout(location = 7) in vec3 aInstanceColor;
layout(location = 8) in float aRoughness;     // 지금은 안 씀(자리만 확보 - VAO stride와 맞춰야 함)
layout(location = 9) in float aMetallic;      // 지금은 안 씀
layout(location = 10) in uint aEntityId;      // 지금은 안 씀
layout(location = 11) in uint aIsSelected;

out vec3 FragPos;
out vec3 Normal;
out vec3 InstanceColor;
flat out uint IsSelected;

uniform mat4 view;
uniform mat4 projection;

void main()
{
    vec4 worldPos = aModel * vec4(aPos, 1.0);
    FragPos = worldPos.xyz;

    mat3 normalMatrix = transpose(inverse(mat3(aModel)));
    Normal = normalize(normalMatrix * aNormal);

    InstanceColor = aInstanceColor;
    IsSelected = aIsSelected;

    gl_Position = projection * view * worldPos;
}
)GLSL";

        const char* kFragmentSrc = R"GLSL(
#version 330 core
out vec4 FragColor;

in vec3 FragPos;
in vec3 Normal;
in vec3 InstanceColor;
flat in uint IsSelected;

// 감쇠 없는 방향광 하나 - point light 배열(1/distance^2)과 달리 오브젝트가 광원 위치와
// 우연히 겹쳐도(거리 0) 나눗셈이 생기지 않는다.
uniform vec3 lightDir;   // 표면 -> 광원 방향(정규화됨)
uniform vec3 viewPos;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    float ambientStrength = 0.25;
    vec3 ambient = ambientStrength * InstanceColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * InstanceColor;

    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), 32.0);
    vec3 specular = vec3(0.3) * spec;

    vec3 color = ambient + diffuse + specular;
    if (IsSelected == 1u)
    {
        color = mix(color, vec3(1.0, 0.85, 0.2), 0.4);
    }

    color = pow(color, vec3(1.0 / 2.2));
    FragColor = vec4(color, 1.0);
}
)GLSL";
    }

    bool SceneMeshRenderer::Initialize()
    {
        if (initialized)
        {
            return true;
        }

        if (!shader.loadFromSource(kVertexSrc, kFragmentSrc))
        {
            Logger::Log(LogLevel::Error, "SceneMeshRenderer::Initialize - Shader compile/link failed");
            return false;
        }

        initialized = true;
        Logger::Log(LogLevel::Info, "SceneMeshRenderer::Initialize - Ready");
        return true;
    }

    void SceneMeshRenderer::Shutdown()
    {
        // Shader 소멸자가 glDeleteProgram을 처리한다 - 여기선 상태 플래그만 내린다.
        initialized = false;
    }

    void SceneMeshRenderer::Render(InstancedBatchManager& batchManager, const Camera& camera)
    {
        if (!initialized)
        {
            return;
        }

        batchManager.UpdateDynamicBatches();

        shader.use();
        shader.setMat4("view", camera.getViewMatrix());
        shader.setMat4("projection", camera.getProjectionMatrix());
        shader.setVec3("viewPos", camera.getPosition());
        shader.setVec3("lightDir", glm::normalize(glm::vec3(0.4f, 1.0f, 0.35f)));

        batchManager.RenderBatches(PassType::ForwardOpaque, &shader);
    }
}
