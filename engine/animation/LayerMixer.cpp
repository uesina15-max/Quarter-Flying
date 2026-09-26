#include "LayerMixer.h"
#include "AnimationSampler.h"
#include "SkeletonSignature.h"
#include <algorithm>
#include <array>

namespace Engine
{
    namespace
    {
        bool StartsWith(const std::string& name, const std::string& prefix)
        {
            return name.size() >= prefix.size() && name.compare(0, prefix.size(), prefix) == 0;
        }

        // §C8: "Spine 및 그 자손 Arm*, Hand*, Head* 등"
        constexpr std::array<const char*, 4> kUpperBodyPrefixes = { "Spine", "Arm", "Hand", "Head" };
        // §C8: "Hips 및 그 자손 Leg*, Foot* 등"
        constexpr std::array<const char*, 3> kLowerBodyPrefixes = { "Hips", "Leg", "Foot" };

        template <size_t N>
        bool MatchesAnyPrefix(const std::string& name, const std::array<const char*, N>& prefixes)
        {
            for (const char* prefix : prefixes)
            {
                if (StartsWith(name, prefix))
                {
                    return true;
                }
            }
            return false;
        }
    }

    bool AreMaskPresetTablesNonOverlapping()
    {
        for (const char* upper : kUpperBodyPrefixes)
        {
            for (const char* lower : kLowerBodyPrefixes)
            {
                // 한쪽이 다른 쪽의 접두사이기만 해도(둘 다 접두사 매칭이므로) 실질적으로 겹친다.
                if (StartsWith(upper, lower) || StartsWith(lower, upper))
                {
                    return false;
                }
            }
        }
        return true;
    }

    float MaskWeightForBone(const LayerSpec& layer, int boneIndex, const Skeleton& skeleton)
    {
        const auto& bones = skeleton.GetBones();
        if (boneIndex < 0 || boneIndex >= static_cast<int>(bones.size()))
        {
            return 0.0f;
        }

        switch (layer.maskPreset)
        {
        case MaskPreset::Full:
            return 1.0f;

        case MaskPreset::UpperBody:
            return MatchesAnyPrefix(bones[boneIndex].name, kUpperBodyPrefixes) ? 1.0f : 0.0f;

        case MaskPreset::LowerBody:
            return MatchesAnyPrefix(bones[boneIndex].name, kLowerBodyPrefixes) ? 1.0f : 0.0f;

        case MaskPreset::Custom:
        {
            auto it = layer.customPerBoneWeight.find(boneIndex);
            if (it == layer.customPerBoneWeight.end())
            {
                return 0.0f; // §C7: missing entry = 0
            }
            return std::clamp(it->second, 0.0f, 1.0f);
        }
        }
        return 0.0f;
    }

    std::expected<Pose, MixerError> MixLayers(
        const Skeleton& skeleton,
        const Pose& baseLocalPose,
        const std::vector<LayerSpec>& orderedLayers,
        int* outFailedLayerIndex)
    {
        if (outFailedLayerIndex)
        {
            *outFailedLayerIndex = -1;
        }

        Pose result = baseLocalPose;

        for (size_t layerIdx = 0; layerIdx < orderedLayers.size(); ++layerIdx)
        {
            const LayerSpec& layer = orderedLayers[layerIdx];

            if (!layer.clip)
            {
                if (outFailedLayerIndex) *outFailedLayerIndex = static_cast<int>(layerIdx);
                return std::unexpected(MixerError::EmptyLayerClip);
            }

            if (IsCompatible(skeleton.GetSignature(), layer.clip->signature) != CompatibilityResult::Compatible)
            {
                if (outFailedLayerIndex) *outFailedLayerIndex = static_cast<int>(layerIdx);
                return std::unexpected(MixerError::IncompatibleAtLayerIndex);
            }

            float weight = std::clamp(layer.weight, 0.0f, 1.0f);
            if (!(weight >= 0.0f && weight <= 1.0f))  // NaN 등 방어 (§C13: "코딩 버그 신호")
            {
                if (outFailedLayerIndex) *outFailedLayerIndex = static_cast<int>(layerIdx);
                return std::unexpected(MixerError::InvalidWeightRangeAfterClamp);
            }

            if (layer.sampledPose.size() != result.size())
            {
                // 스켈레톤 본 개수와 안 맞는 포즈 - 호출자 버그 신호. 계약서에 전용 에러
                // 값이 없어서, "이 스켈레톤 구조에 맞지 않는 데이터"라는 의미가 같은
                // IncompatibleAtLayerIndex로 취급한다.
                if (outFailedLayerIndex) *outFailedLayerIndex = static_cast<int>(layerIdx);
                return std::unexpected(MixerError::IncompatibleAtLayerIndex);
            }

            for (size_t i = 0; i < result.size(); ++i)
            {
                float mask = MaskWeightForBone(layer, static_cast<int>(i), skeleton);
                float effectiveWeight = weight * mask;
                if (effectiveWeight <= 0.0f)
                {
                    continue;
                }

                result[i].position = glm::mix(result[i].position, layer.sampledPose[i].position, effectiveWeight);
                result[i].scale = glm::mix(result[i].scale, layer.sampledPose[i].scale, effectiveWeight);
                result[i].rotation = ShortestSlerp(result[i].rotation, layer.sampledPose[i].rotation, effectiveWeight);
            }
        }

        return result;
    }
}
