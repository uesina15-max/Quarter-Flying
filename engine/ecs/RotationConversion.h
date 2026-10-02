#pragma once

#include "../core/Types.h"

namespace Engine
{
    // TransformComponent::rotation은 Quaternion으로 저장하지만, 직렬화(프리팹/PIE 스냅샷/scene.json)와
    // 에디터 편집, ECSRegistry::SetTransformRotation은 "오일러 각(도) XYZ" 3개 값으로 다룬다.
    // 두 표현 사이의 변환은 이 두 함수만 쓴다(glm의 pitch/yaw/roll = X/Y/Z 규약).
    Quaternion QuaternionFromEulerDegrees(const Vec3& eulerDegrees);
    Vec3 EulerDegreesFromQuaternion(const Quaternion& rotation);
}
