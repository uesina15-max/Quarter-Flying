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
layout(location = 8) in float aRoughness;     // 머티리얼(R-4)
layout(location = 9) in float aMetallic;
layout(location = 10) in uint aEntityId;      // 지금은 안 씀
layout(location = 11) in uint aIsSelected;

out vec3 FragPos;
out vec3 Normal;
out vec3 InstanceColor;
out float Roughness;
out float Metallic;
out vec2 TexCoords;
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
    Roughness = aRoughness;
    Metallic = aMetallic;
    TexCoords = aTexCoords;
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
in float Roughness;
in float Metallic;
in vec2 TexCoords;
flat in uint IsSelected;

// 감쇠 없는 방향광 하나 - point light 배열(1/distance^2)과 달리 오브젝트가 광원 위치와
// 우연히 겹쳐도(거리 0) 나눗셈이 생기지 않는다.
uniform vec3 lightDir;   // 표면 -> 광원 방향(정규화됨)
uniform vec3 lightColor; // 빛 색 x 세기(SceneLighting.color)
uniform float ambientStrength;
uniform vec3 viewPos;

// InstancedBatchManager::RenderBatch가 배치마다 설정한다(텍스처 없는 배치는 uHasTexture = 0).
uniform int uHasTexture;
uniform sampler2D uTexture;

void main()
{
    vec3 norm = normalize(Normal);
    vec3 viewDir = normalize(viewPos - FragPos);

    // 텍스처 색 * 인스턴스 색(색이 흰색이면 텍스처 그대로). 텍스처는 sRGB로 저작되므로 아래 감마 보정과
    // 짝이 맞도록 선형 공간으로 옮겨서 조명한다.
    vec3 baseColor = InstanceColor;
    if (uHasTexture == 1)
    {
        baseColor *= pow(texture(uTexture, TexCoords).rgb, vec3(2.2));
    }

    vec3 ambient = ambientStrength * baseColor;

    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * baseColor * lightColor;

    // roughness·metallic(R-4). 기본값(0.6 / 0.0)에서 예전 고정값과 정확히 같도록 맞춘 매핑이다(지수 32, 세기 0.3,
    // 흰 반사광) - 머티리얼이 없는 옛 씬이 이전과 똑같이 보여야 한다. 거칠수록 하이라이트가 넓고 약해지고,
    // 금속일수록 반사광이 표면색을 띠고 확산광이 줄어든다.
    float shininess = 32.0 * exp2((0.6 - Roughness) * 8.0);
    float specStrength = 0.75 * (1.0 - Roughness);
    vec3 specColor = mix(vec3(1.0), baseColor, Metallic);
    diffuse *= (1.0 - 0.9 * Metallic);
    vec3 halfDir = normalize(lightDir + viewDir);
    float spec = pow(max(dot(norm, halfDir), 0.0), shininess);
    vec3 specular = specStrength * specColor * spec * lightColor;

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

    void SceneMeshRenderer::Render(InstancedBatchManager& batchManager, const Camera& camera, const SceneLighting& lighting)
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
        // 예전에는 여기서 방향 (0.4, 1, 0.35)과 주변광 0.25를 고정으로 썼다(씬 라이트와 무관). 지금은 RenderSystem이
        // DirectionalLightComponent에서 만든 값을 받는다. 라이트가 없으면 같은 고정값(SceneLighting::Default)이다.
        shader.setVec3("lightDir", lighting.toLight);
        shader.setVec3("lightColor", lighting.color);
        shader.setFloat("ambientStrength", lighting.ambient);

        batchManager.RenderBatches(PassType::ForwardOpaque, &shader);
    }
}
