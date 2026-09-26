#pragma once

#include "Bone.h"
#include "Pose.h"
#include "SkeletonSignature.h"
#include <expected>
#include <string>
#include <unordered_map>
#include <vector>

namespace Engine
{
    enum class SkeletonError
    {
        DuplicateBoneName,
        InvalidParentIndex,
        CyclicHierarchy,
        NoRootBone,
        MultipleRootBones,
    };

    /// <summary>
    /// 스켈레톤 자산: 본 목록 + 이름 조회 + 구조 시그니처.
    ///
    /// 생성은 반드시 Skeleton::Create를 통해서만 한다 - 유효성 검증(이름 중복/부모 인덱스
    /// 유효성/루트 개수/사이클)을 통과하지 못하면 부분적으로 만들어진 객체를 반환하지
    /// 않는다 (착수 계약서 §C10: "partial object 반환 금지").
    /// </summary>
    class Skeleton
    {
    public:
        static std::expected<Skeleton, SkeletonError> Create(
            std::vector<Bone> bones,
            AxisSystem axis = AxisSystem::Y_UP_RH,
            LengthUnit unit = LengthUnit::Meter);

        const std::vector<Bone>& GetBones() const { return bones; }
        size_t GetBoneCount() const { return bones.size(); }

        // 이름으로 본 인덱스를 찾는다. 없으면 -1.
        int FindBoneIndex(const std::string& name) const;

        const SkeletonSignature& GetSignature() const { return signature; }

        // 바인드 포즈 기준 월드 트랜스폼 (GetBones()와 같은 순서/길이).
        std::vector<Transform> ComputeBindWorldTransforms() const;

        // 임의의 로컬 Pose(같은 길이/순서 가정)에 대한 월드 트랜스폼.
        // world_i = parentWorld_i * local_i (착수 계약서 §C1, §C12).
        std::vector<Transform> ComputeWorldTransforms(const Pose& localPose) const;

    private:
        Skeleton() = default;

        std::vector<Bone> bones;
        std::unordered_map<std::string, int> nameToIndex;
        SkeletonSignature signature;
    };
}
