#include "SkeletonSignature.h"
#include <algorithm>

namespace Engine
{
    namespace
    {
        // FNV-1a 64-bit. 결정적이고 빠르며 이 용도(구조적 fast-reject)에는 암호학적 강도가
        // 필요 없다 - 외부 의존성 추가 없이 자체 구현.
        uint64_t Fnv1a64Append(const std::string& data, uint64_t hash)
        {
            constexpr uint64_t kPrime = 1099511628211ull;
            for (unsigned char c : data)
            {
                hash ^= static_cast<uint64_t>(c);
                hash *= kPrime;
            }
            return hash;
        }
    }

    SkeletonSignature ComputeSkeletonSignature(const std::vector<Bone>& bones, AxisSystem axis, LengthUnit unit)
    {
        SkeletonSignature sig;
        sig.formatVersion = 1;
        sig.axis = axis;
        sig.unit = unit;

        sig.canonicalPairs.reserve(bones.size());
        for (const Bone& bone : bones)
        {
            BonePair pair;
            pair.boneName = bone.name;
            pair.parentName = (bone.parentIndex >= 0 && bone.parentIndex < static_cast<int>(bones.size()))
                ? bones[bone.parentIndex].name
                : kSkeletonRootSentinel;
            sig.canonicalPairs.push_back(std::move(pair));
        }

        std::sort(sig.canonicalPairs.begin(), sig.canonicalPairs.end());

        // 의도적으로 axis/unit은 hash에서 제외 (헤더 주석 참고).
        constexpr uint64_t kFnvOffsetBasis = 14695981039346656037ull;
        uint64_t hash = kFnvOffsetBasis;
        hash = Fnv1a64Append("v" + std::to_string(sig.formatVersion), hash);
        for (const BonePair& pair : sig.canonicalPairs)
        {
            hash = Fnv1a64Append(pair.boneName, hash);
            hash = Fnv1a64Append("|", hash);
            hash = Fnv1a64Append(pair.parentName, hash);
            hash = Fnv1a64Append(";", hash);
        }
        sig.hash = hash;

        return sig;
    }

    CompatibilityResult IsCompatible(const SkeletonSignature& a, const SkeletonSignature& b)
    {
        if (a.hash != b.hash)
        {
            return CompatibilityResult::RejectFast;
        }
        if (a.canonicalPairs != b.canonicalPairs)
        {
            return CompatibilityResult::RejectStructural;
        }
        if (a.axis != b.axis || a.unit != b.unit)
        {
            return CompatibilityResult::RejectMeta;
        }
        return CompatibilityResult::Compatible;
    }
}
