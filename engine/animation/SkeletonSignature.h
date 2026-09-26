#pragma once

#include "Bone.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Engine
{
    enum class AxisSystem : uint8_t
    {
        Y_UP_RH,   // 착수 계약서 v3 MVP 범위에서 지원하는 유일한 axis 체계
    };

    enum class LengthUnit : uint8_t
    {
        Meter,
        Centimeter,
    };

    inline constexpr const char* kSkeletonRootSentinel = "<ROOT>";

    /// <summary>
    /// 정렬된 (본 이름, 부모 이름) 페어 하나. 루트 본의 parentName은 kSkeletonRootSentinel.
    /// </summary>
    struct BonePair
    {
        std::string boneName;
        std::string parentName;

        bool operator==(const BonePair&) const = default;
        bool operator<(const BonePair& other) const
        {
            if (boneName != other.boneName) return boneName < other.boneName;
            return parentName < other.parentName;
        }
    };

    /// <summary>
    /// 스켈레톤의 "구조"만을 요약하는 시그니처. 착수 계약서 §C3:
    ///   - 전위순회 기반 인덱스 의존 시그니처는 폐기.
    ///   - (본 이름, 부모 이름) 페어를 사전순 정렬해 직렬화 -> 형제 순서/배열 인덱스 배치 무관.
    ///
    /// hash는 formatVersion + canonicalPairs(구조)에서만 유도한다 - axis/unit은 일부러
    /// 뺐다. 그래야 "구조는 같은데 axis/unit만 다른" 케이스에서 구조 재검증까지는
    /// 통과하고 별도의 메타 검사(IsCompatible의 RejectMeta 단계)에서 걸러진다.
    /// axis/unit까지 hash에 섞으면 그 케이스가 이미 hash 불일치(RejectFast)로
    /// 처리되어 버려서, 메타 검사 단계가 사실상 죽은 코드가 된다.
    /// </summary>
    struct SkeletonSignature
    {
        uint32_t formatVersion = 1;
        AxisSystem axis = AxisSystem::Y_UP_RH;
        LengthUnit unit = LengthUnit::Meter;
        std::vector<BonePair> canonicalPairs;  // 정렬됨
        uint64_t hash = 0;                     // formatVersion+canonicalPairs의 FNV-1a 64 해시

        bool operator==(const SkeletonSignature&) const = default;
    };

    enum class CompatibilityResult
    {
        Compatible,
        RejectFast,        // hash 불일치로 조기 거부 (구조가 다름)
        RejectStructural,  // hash는 같지만 canonicalPairs 재검증에서 불일치 (해시 충돌 방어)
        RejectMeta,         // 구조는 같지만 axis 또는 unit이 다름
    };

    /// <summary>
    /// 본 목록으로부터 SkeletonSignature를 계산한다. 유효성 검증(중복 이름/cycle 등)은
    /// Skeleton::Create가 담당하므로 여기서는 이미 유효한 본 목록을 가정한다.
    /// </summary>
    SkeletonSignature ComputeSkeletonSignature(
        const std::vector<Bone>& bones,
        AxisSystem axis = AxisSystem::Y_UP_RH,
        LengthUnit unit = LengthUnit::Meter);

    /// <summary>
    /// 착수 계약서 §C3의 3단 판정 경로: hash로 fast reject -> canonical pair 재검증
    /// (hash만으로 "같다"고 성급히 판단하지 않기 위함) -> axis/unit 메타 검사.
    /// </summary>
    CompatibilityResult IsCompatible(const SkeletonSignature& a, const SkeletonSignature& b);
}
