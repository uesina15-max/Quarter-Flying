#include "RotationConversion.h"
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    Quaternion QuaternionFromEulerDegrees(const Vec3& eulerDegrees)
    {
        glm::quat q(glm::radians(glm::vec3(eulerDegrees.x, eulerDegrees.y, eulerDegrees.z)));
        return Quaternion(q.x, q.y, q.z, q.w);
    }

    Vec3 EulerDegreesFromQuaternion(const Quaternion& rotation)
    {
        glm::quat q(rotation.w, rotation.x, rotation.y, rotation.z);  // glm 생성자는 (w, x, y, z)
        glm::vec3 e = glm::degrees(glm::eulerAngles(glm::normalize(q)));
        return Vec3(e.x, e.y, e.z);
    }
}
