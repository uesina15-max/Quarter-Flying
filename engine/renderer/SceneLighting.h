#pragma once

#include <glm/glm.hpp>

namespace Engine
{
    // ========================================
    // 씬 조명 (docs/USABILITY_IMPROVEMENT_PLAN.md R-1)
    // ========================================
    //
    // RenderSystem(ecs)이 DirectionalLightComponent를 골라 이 구조체로 만들고, SceneMeshRenderer(renderer)가
    // 셰이더 유니폼으로 쓴다. 렌더러는 ECS 컴포넌트를 모른다 - 경계에서 오가는 것은 이 순수 데이터뿐이다.
    // 그래서 "조명이 이상하다"면 이 값부터 본다: 값이 맞는데 화면이 틀리면 셰이더(renderer), 값이 틀리면
    // 컴포넌트·선택 규칙(ecs).
    struct SceneLighting
    {
        glm::vec3 toLight;      // 표면 -> 광원 방향(정규화됨). 셰이더의 lightDir
        glm::vec3 color;        // 빛 색 x 세기
        float ambient;          // 주변광 비율

        // 라이트가 없을 때의 값. DirectionalLightComponent가 생기기 전의 고정값과 같다 - 라이트가 없는 옛 씬이
        // 이전과 똑같이 보여야 한다.
        static SceneLighting Default()
        {
            return { glm::normalize(glm::vec3(0.4f, 1.0f, 0.35f)), glm::vec3(1.0f), 0.25f };
        }
    };
}
