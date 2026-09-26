#include "MotionPreviewState.h"
#include "AnimationSampler.h"
#include "ClipJsonIO.h"
#include "SkeletonJsonIO.h"
#include <algorithm>

namespace Engine
{
    namespace
    {
        template <typename ErrorEnum>
        std::string DescribeError(const char* prefix, ErrorEnum code)
        {
            return std::string(prefix) + " (code=" + std::to_string(static_cast<int>(code)) + ")";
        }
    }

    bool MotionPreviewState::LoadSkeleton(const std::string& skeletonJson)
    {
        auto result = ParseSkeletonJson(skeletonJson);
        if (!result.has_value())
        {
            lastError_ = DescribeError("스켈레톤 로드 실패", result.error());
            return false;
        }
        skeleton_ = std::move(result.value());
        // 스켈레톤이 바뀌면 이전 클립/레이어들이 이 새 스켈레톤과 맞다는 보장이 없다 - 비운다.
        baseClip_.reset();
        layers_.clear();
        lastError_.clear();
        return true;
    }

    bool MotionPreviewState::LoadBaseClip(const std::string& clipJson)
    {
        if (!skeleton_.has_value())
        {
            lastError_ = "베이스 클립 로드 실패: 스켈레톤이 먼저 로드되어야 함";
            return false;
        }
        auto result = ParseClipJson(clipJson, *skeleton_);
        if (!result.has_value())
        {
            lastError_ = DescribeError("베이스 클립 로드 실패", result.error());
            return false;
        }
        baseClip_ = std::move(result.value());
        lastError_.clear();
        return true;
    }

    bool MotionPreviewState::CheckClipCompatible(const std::string& clipJson) const
    {
        if (!skeleton_.has_value())
        {
            return false;
        }
        // ParseClipJson 자체가 §C10 검증(스키마 + 본 이름 대조 + optional signature 대조)을
        // 전부 수행한다 - 여기서는 그 결과(성공/실패)만 보고 버린다. 상태는 절대 바꾸지 않는다.
        auto result = ParseClipJson(clipJson, *skeleton_);
        return result.has_value();
    }

    int MotionPreviewState::AddLayer(const std::string& clipJson)
    {
        if (!skeleton_.has_value())
        {
            lastError_ = "레이어 추가 실패: 스켈레톤이 먼저 로드되어야 함";
            return -1;
        }
        auto result = ParseClipJson(clipJson, *skeleton_);
        if (!result.has_value())
        {
            lastError_ = DescribeError("레이어 추가 실패", result.error());
            return -1;
        }
        LayerEntry entry;
        entry.id = nextLayerId_++;
        entry.clip = std::move(result.value());
        layers_.push_back(std::move(entry));
        lastError_.clear();
        return layers_.back().id;
    }

    bool MotionPreviewState::RemoveLayer(int layerId)
    {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [layerId](const LayerEntry& e) { return e.id == layerId; });
        if (it == layers_.end())
        {
            return false;
        }
        layers_.erase(it);
        return true;
    }

    void MotionPreviewState::ClearLayers()
    {
        layers_.clear();
    }

    MotionPreviewState::LayerEntry* MotionPreviewState::FindLayer(int layerId)
    {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [layerId](const LayerEntry& e) { return e.id == layerId; });
        return it != layers_.end() ? &(*it) : nullptr;
    }

    const MotionPreviewState::LayerEntry* MotionPreviewState::FindLayer(int layerId) const
    {
        auto it = std::find_if(layers_.begin(), layers_.end(),
            [layerId](const LayerEntry& e) { return e.id == layerId; });
        return it != layers_.end() ? &(*it) : nullptr;
    }

    bool MotionPreviewState::SetLayerFrame(int layerId, float frame)
    {
        LayerEntry* e = FindLayer(layerId);
        if (!e) return false;
        e->frame = frame;
        return true;
    }

    bool MotionPreviewState::SetLayerWeight(int layerId, float weight)
    {
        LayerEntry* e = FindLayer(layerId);
        if (!e) return false;
        e->weight = weight;
        return true;
    }

    bool MotionPreviewState::SetLayerMask(int layerId, MaskPreset mask)
    {
        LayerEntry* e = FindLayer(layerId);
        if (!e) return false;
        e->mask = mask;
        return true;
    }

    std::vector<std::pair<glm::vec3, glm::vec3>> MotionPreviewState::ComputeBoneWorldLines() const
    {
        std::vector<std::pair<glm::vec3, glm::vec3>> lines;
        if (!skeleton_.has_value() || !baseClip_.has_value())
        {
            return lines;
        }

        Pose pose = Sample(*skeleton_, *baseClip_, baseFrame_);

        if (!layers_.empty())
        {
            std::vector<LayerSpec> specs;
            specs.reserve(layers_.size());
            for (const auto& layer : layers_)
            {
                LayerSpec spec;
                spec.clip = &layer.clip;
                spec.sampledPose = Sample(*skeleton_, layer.clip, layer.frame);
                spec.weight = layer.weight;
                spec.maskPreset = layer.mask;
                specs.push_back(std::move(spec));
            }

            auto mixed = MixLayers(*skeleton_, pose, specs);
            if (mixed.has_value())
            {
                pose = std::move(mixed.value());
            }
            // 실패하면(같은 스켈레톤에서 만든 클립끼리라 이론상 발생하지 않아야 함) base pose로 폴백.
        }

        std::vector<Transform> world = skeleton_->ComputeWorldTransforms(pose);
        const auto& bones = skeleton_->GetBones();

        lines.reserve(bones.size());
        for (size_t i = 0; i < bones.size(); ++i)
        {
            int parent = bones[i].parentIndex;
            if (parent < 0)
            {
                continue; // 루트는 그릴 부모-자식 선분이 없음
            }
            lines.emplace_back(world[parent].position, world[i].position);
        }
        return lines;
    }
}
