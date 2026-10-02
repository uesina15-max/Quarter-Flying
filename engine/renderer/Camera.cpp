#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

namespace Engine
{
    Camera::Camera()
        : m_position(0.0f, 0.0f, 5.0f),
          m_target(0.0f, 0.0f, 0.0f),
          m_up(0.0f, 1.0f, 0.0f),
          m_fovRadians(glm::radians(60.0f)),
          m_aspect(16.0f / 9.0f),
          m_nearPlane(0.1f),
          m_farPlane(1000.0f)
    {
        rebuildProjection();
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

    void Camera::setProjection(float fovRadians, float aspect, float nearPlane, float farPlane)
    {
        m_fovRadians = fovRadians;
        m_aspect = aspect;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
        rebuildProjection();
    }

    void Camera::setAspect(float aspect)
    {
        m_aspect = aspect;
        rebuildProjection();
    }

    void Camera::setLens(float fovRadians, float nearPlane, float farPlane)
    {
        m_fovRadians = fovRadians;
        m_nearPlane = nearPlane;
        m_farPlane = farPlane;
        rebuildProjection();
    }

    void Camera::rebuildProjection()
    {
        m_projection = glm::perspective(m_fovRadians, m_aspect, m_nearPlane, m_farPlane);
    }
}
