#pragma once
#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>

namespace Engine
{
    class Camera
    {
    public:
        Camera();

        glm::mat4 getViewMatrix() const;
        glm::mat4 getProjectionMatrix() const;
        glm::vec3 getPosition() const;
        glm::vec3 getTarget() const { return m_target; }

        void setPosition(const glm::vec3& position);
        void lookAt(const glm::vec3& target);

        // 투영은 두 소유자가 나눠 갱신한다:
        //  - 종횡비(aspect): 뷰포트 크기를 아는 Engine(HandleWindowResize)이 setAspect로.
        //  - 렌즈(fov/near/far): 활성 CameraComponent를 아는 CameraSystem이 setLens로.
        // 예전에는 setProjection 하나뿐이라, 리사이즈가 매번 투영 전체를 60도 고정값으로 덮어써서
        // CameraComponent.fov를 반영할 자리가 없었다(docs/INGAME_CAMERA_PLAN.md C2).
        void setProjection(float fovRadians, float aspect, float nearPlane, float farPlane);
        void setAspect(float aspect);
        void setLens(float fovRadians, float nearPlane, float farPlane);

        float getFovRadians() const { return m_fovRadians; }
        float getAspect() const { return m_aspect; }
        float getNearPlane() const { return m_nearPlane; }
        float getFarPlane() const { return m_farPlane; }

    private:
        void rebuildProjection();

        glm::vec3 m_position;
        glm::vec3 m_target;
        glm::vec3 m_up;

        float m_fovRadians;
        float m_aspect;
        float m_nearPlane;
        float m_farPlane;
        glm::mat4 m_projection{1.0f};
    };
}
