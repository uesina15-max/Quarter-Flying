#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
    Camera::Camera()
        : m_position(0.0f, 0.0f, 5.0f),
          m_target(0.0f, 0.0f, 0.0f),
          m_up(0.0f, 1.0f, 0.0f)
    {
    }

    glm::mat4 Camera::getViewMatrix() const
    {
        return glm::lookAt(m_position, m_target, m_up);
    }

    glm::mat4 Camera::getProjectionMatrix() const
    {
        return m_projection;
    }

    glm::vec3 Camera::getPosition() const
    {
        return m_position;
    }

    void Camera::setPosition(const glm::vec3& position)
    {
        m_position = position;
    }

    void Camera::lookAt(const glm::vec3& target)
    {
        m_target = target;
    }

    void Camera::setProjection(float fov, float aspect, float nearPlane, float farPlane)
    {
        m_projection = glm::perspective(fov, aspect, nearPlane, farPlane);
    }
}
