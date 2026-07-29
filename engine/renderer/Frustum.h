#pragma once

#include <glm/mat4x4.hpp>
#include <glm/vec3.hpp>
#include <array>

namespace Engine
{
    // ============================================================================
    // Frustum Class for View Frustum Culling
    // ============================================================================
    class Frustum
    {
    public:
        Frustum();
        ~Frustum() = default;

        // Update frustum planes from view-projection matrix
        void Update(const glm::mat4& viewProjection);

        // Test if a point is inside the frustum
        bool IsPointVisible(const glm::vec3& point) const;

        // Test if a sphere is inside the frustum
        bool IsSphereVisible(const glm::vec3& center, float radius) const;

        // Test if an AABB is inside the frustum
        bool IsAABBVisible(const glm::vec3& min, const glm::vec3& max) const;

    private:
        // Frustum planes: left, right, top, bottom, near, far
        // Each plane is defined as (normal.x, normal.y, normal.z, distance)
        std::array<glm::vec4, 6> planes;

        // Plane indices
        enum PlaneIndex
        {
            Left = 0,
            Right = 1,
            Top = 2,
            Bottom = 3,
            Near = 4,
            Far = 5
        };

        // Helper to normalize a plane
        void NormalizePlane(glm::vec4& plane);
    };
}
