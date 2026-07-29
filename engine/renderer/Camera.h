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

        void setPosition(const glm::vec3& position);
        void lookAt(const glm::vec3& target);
        void setProjection(float fov, float aspect, float nearPlane, float farPlane);

    private:
        glm::vec3 m_position;
        glm::vec3 m_target;
        glm::vec3 m_up;
        glm::mat4 m_projection{1.0f};
    };
}
