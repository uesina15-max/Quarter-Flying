#pragma once

#include <glm/vec3.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Engine
{
    /// <summary>
    /// 위치/회전/스케일 TRS. Motion Mixer 착수 계약서 v3 §C1: mat4로 뭉개지 않고
    /// 항상 이 형태로 들고 다닌다 (블렌딩이 각 채널을 독립적으로 lerp/slerp 해야 하므로).
    ///
    /// 착수 계약서는 Phase 2/3에서 바인드 포즈(BoneLocalBind)와 애니메이션 포즈(BoneLocalPose)를
    /// 의미상 구분하지만, Phase 1(§F) 골격은 둘 다 이 하나의 Transform 타입으로 표현한다.
    /// 나중에 두 의미를 코드 레벨에서도 분리하고 싶어지면 여기 타입 별칭을 추가하면 된다.
    /// </summary>
    struct Transform
    {
        glm::vec3 position{ 0.0f, 0.0f, 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };  // identity (w, x, y, z)
        glm::vec3 scale{ 1.0f, 1.0f, 1.0f };

        Transform() = default;
        Transform(const glm::vec3& pos, const glm::quat& rot, const glm::vec3& scl)
            : position(pos), rotation(rot), scale(scl) {}
    };

    /// <summary>
    /// 부모의 월드 Transform과 자식의 (부모 기준) 로컬 Transform을 합성해 자식의 월드 Transform을 만든다.
    /// world_i = parentWorld_i * local_i (착수 계약서 §C1, §C12).
    /// 비균등 스케일의 shear는 고려하지 않는 근사(스켈레톤 계층에서 흔히 쓰는 단순화) — 스케일은
    /// 컴포넌트별 곱, 위치는 부모의 회전+스케일을 적용한 뒤 부모 위치를 더하는 방식으로 합성한다.
    /// </summary>
    inline Transform ComposeTransform(const Transform& parentWorld, const Transform& local)
    {
        Transform result;
        result.scale = parentWorld.scale * local.scale;
        result.rotation = glm::normalize(parentWorld.rotation * local.rotation);
        result.position = parentWorld.position +
            parentWorld.rotation * (parentWorld.scale * local.position);
        return result;
    }
}
