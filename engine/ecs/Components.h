#pragma once

#include "../core/Types.h"

namespace Engine
{
    // ========================================
    // Example Components
    // ========================================
    
    // 예제 컴포넌트들
    // 실제 게임에서는 프로젝트별로 정의
    
    // Transform Component
    struct TransformComponent
    {
        Vec3 position;
        Quaternion rotation;
        Vec3 scale;

        TransformComponent()
            : position(0.0f, 0.0f, 0.0f)
            , rotation(0.0f, 0.0f, 0.0f, 1.0f)
            , scale(1.0f, 1.0f, 1.0f)
        {}
    };

    // Renderable Component
    struct RenderableComponent
    {
        uint32_t meshHandle;
        uint32_t materialHandle;
        bool castShadows;

        RenderableComponent()
            : meshHandle(0)
            , materialHandle(0)
            , castShadows(true)
        {}
    };

    // Camera Component
    struct CameraComponent
    {
        float fov;
        float nearPlane;
        float farPlane;
        bool isMainCamera;

        // Cached matrices (flat array to avoid glm dependency in headers)
        float viewMatrix[16];
        float projMatrix[16];

        CameraComponent()
            : fov(45.0f)
            , nearPlane(0.1f)
            , farPlane(100.0f)
            , isMainCamera(false)
        {
            for(int i=0; i<16; ++i) {
                viewMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
                projMatrix[i] = (i % 5 == 0) ? 1.0f : 0.0f;
            }
        }
    };

    // AI State Enum
    enum class AIState {
        Idle,
        Patrol,
        Chase,
        Attack,
        Return
    };

    // AI Component
    struct AIComponent
    {
        bool enabled;
        float sight_range;
        float attack_range;
        std::string idle_action;
        std::string attack_action;
        std::string hit_action;
        AIState default_state;

        AIComponent()
            : enabled(true)
            , sight_range(10.0f)
            , attack_range(2.0f)
            , idle_action("Idle.action")
            , attack_action("Attack.action")
            , hit_action("Hit.action")
            , default_state(AIState::Patrol)
        {}
    };

} // namespace Engine
