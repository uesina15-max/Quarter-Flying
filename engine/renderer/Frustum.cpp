#include "Frustum.h"
#include <glm/gtc/matrix_access.hpp>

namespace Engine
{
    Frustum::Frustum()
    {
        // Initialize planes to zero
        planes.fill(glm::vec4(0.0f));
    }

    void Frustum::Update(const glm::mat4& viewProjection)
    {
        // Extract frustum planes from the view-projection matrix
        // Based on the Gribb-Hartmann method

        // Left plane
        planes[Left] = glm::vec4(
            viewProjection[0][3] + viewProjection[0][0],
            viewProjection[1][3] + viewProjection[1][0],
            viewProjection[2][3] + viewProjection[2][0],
            viewProjection[3][3] + viewProjection[3][0]
        );
        NormalizePlane(planes[Left]);

        // Right plane
        planes[Right] = glm::vec4(
            viewProjection[0][3] - viewProjection[0][0],
            viewProjection[1][3] - viewProjection[1][0],
            viewProjection[2][3] - viewProjection[2][0],
            viewProjection[3][3] - viewProjection[3][0]
        );
        NormalizePlane(planes[Right]);

        // Top plane
        planes[Top] = glm::vec4(
            viewProjection[0][3] - viewProjection[0][1],
            viewProjection[1][3] - viewProjection[1][1],
            viewProjection[2][3] - viewProjection[2][1],
            viewProjection[3][3] - viewProjection[3][1]
        );
        NormalizePlane(planes[Top]);

        // Bottom plane
        planes[Bottom] = glm::vec4(
            viewProjection[0][3] + viewProjection[0][1],
            viewProjection[1][3] + viewProjection[1][1],
            viewProjection[2][3] + viewProjection[2][1],
            viewProjection[3][3] + viewProjection[3][1]
        );
        NormalizePlane(planes[Bottom]);

        // Near plane
        planes[Near] = glm::vec4(
            viewProjection[0][3] + viewProjection[0][2],
            viewProjection[1][3] + viewProjection[1][2],
            viewProjection[2][3] + viewProjection[2][2],
            viewProjection[3][3] + viewProjection[3][2]
        );
        NormalizePlane(planes[Near]);

        // Far plane
        planes[Far] = glm::vec4(
            viewProjection[0][3] - viewProjection[0][2],
            viewProjection[1][3] - viewProjection[1][2],
            viewProjection[2][3] - viewProjection[2][2],
            viewProjection[3][3] - viewProjection[3][2]
        );
        NormalizePlane(planes[Far]);
    }

    bool Frustum::IsPointVisible(const glm::vec3& point) const
    {
        for (const auto& plane : planes)
        {
            // Calculate distance from point to plane
            float distance = plane.x * point.x + plane.y * point.y + plane.z * point.z + plane.w;
            
            // If point is behind any plane, it's outside the frustum
            if (distance < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::IsSphereVisible(const glm::vec3& center, float radius) const
    {
        for (const auto& plane : planes)
        {
            // Calculate distance from sphere center to plane
            float distance = plane.x * center.x + plane.y * center.y + plane.z * center.z + plane.w;
            
            // If sphere is behind any plane by more than its radius, it's outside
            if (distance < -radius)
            {
                return false;
            }
        }
        return true;
    }

    bool Frustum::IsAABBVisible(const glm::vec3& min, const glm::vec3& max) const
    {
        // Test all 8 corners of the AABB against all 6 planes
        glm::vec3 corners[8] = {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, max.y, max.z)
        };

        for (const auto& plane : planes)
        {
            bool allOutside = true;
            
            for (int i = 0; i < 8; ++i)
            {
                float distance = plane.x * corners[i].x + plane.y * corners[i].y + 
                                 plane.z * corners[i].z + plane.w;
                
                // If at least one corner is in front of this plane, continue checking
                if (distance >= 0.0f)
                {
                    allOutside = false;
                    break;
                }
            }
            
            // If all corners are behind this plane, the AABB is outside the frustum
            if (allOutside)
            {
                return false;
            }
        }
        
        return true;
    }

    void Frustum::NormalizePlane(glm::vec4& plane)
    {
        float length = glm::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
        
        if (length > 0.0f)
        {
            plane /= length;
        }
    }
}
