#pragma once

#include "KeyframeClip.h"
#include "LayerMixer.h"
#include "Pose.h"
#include "Skeleton.h"
#include <glm/vec3.hpp>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace Engine
{
    /// <summary>
    /// Phase 4A(착수 계약서 §C11)의 "Preview State" - Motion Editor가 값을 채우고
    /// EngineViewport(Renderer)가 읽어서 그리는 중간 상태.
    ///
    /// 의존 방향은 계약서 그대로: Motion Editor -> Preview State -> EngineViewport.
    /// Motion Editor는 이 객체의 값만 바꾸고 OpenGL/뷰포트에는 직접 손대지 않는다
    /// ("Editor <-> OpenGL Context 직접 결합 금지").
    ///
    /// Phase 5(믹서 UI)에서 Phase 4A의 "base + overlay 1개" 모델을 "base + 레이어 N개
    /// 스택"으로 확장했다. base 클립은 여전히 항상 켜져 있는 바탕 포즈(예: Idle)이고,
    /// 그 위에 순서대로 쌓인 레이어들이 LayerMixer::MixLayers 한 번 호출로 pairwise
    /// override 블렌드된다 - "UI blending 로직 재구현 금지"(§C) 그대로, Python은
    /// weight/mask/frame 값 전달까지만 하고 실제 블렌드 연산은 여기(그리고 LayerMixer)에서만
    /// 일어난다.
    ///
    /// 레이어는 벡터 인덱스가 아니라 안정적인 id로 식별한다 - Add 순서대로 index가 매겨지면
    /// 중간 레이어를 지울 때 UI(Python) 쪽이 나머지 레이어들의 index를 전부 다시 맞춰야
    /// 하는데, 그 재동기화 로직 자체가 버그 소지가 크다. id는 단조증가라 삭제해도 다른
    /// 레이어의 id가 절대 안 바뀐다.
    /// </summary>
    class MotionPreviewState
    {
    public:
        bool LoadSkeleton(const std::string& skeletonJson);
        bool LoadBaseClip(const std::string& clipJson);

        void SetBaseFrame(float frame) { baseFrame_ = frame; }

        bool HasSkeleton() const { return skeleton_.has_value(); }
        bool HasBaseClip() const { return baseClip_.has_value(); }

        // 마지막으로 실패한 호출의 사유(사람이 읽을 간단한 문자열). 성공하면 비워짐.
        const std::string& GetLastError() const { return lastError_; }

        // 스켈레톤이 로드된 상태에서, 주어진 클립 JSON이 그 스켈레톤과 실제로 맞물릴 수
        // 있는지(파싱 + 본 이름 대조 + optional signature 대조까지) 검사만 한다 - 상태를
        // 바꾸지 않는다. Phase 5 UI가 "호환 가능한 클립만 dropdown에 노출"하기 위한 용도
        // (착수 계약서 §C10 검증 로직을 그대로 재사용 - 별도 호환성 판정을 다시 구현하지 않음).
        bool CheckClipCompatible(const std::string& clipJson) const;

        // 레이어 스택 (Phase 5). 성공 시 새 레이어의 id(>=0)를, 실패 시 -1을 반환하고
        // lastError_를 채운다. 스켈레톤이 없거나 clipJson이 호환되지 않으면 실패.
        int AddLayer(const std::string& clipJson);
        bool RemoveLayer(int layerId);
        void ClearLayers();
        int GetLayerCount() const { return static_cast<int>(layers_.size()); }

        // 존재하지 않는 layerId를 넘기면 false를 반환하고 아무 것도 하지 않는다.
        bool SetLayerFrame(int layerId, float frame);
        bool SetLayerWeight(int layerId, float weight);
        bool SetLayerMask(int layerId, MaskPreset mask);

        // 이번 프레임에 그릴 본 라인(부모 월드 위치 -> 자식 월드 위치) 목록.
        // 스켈레톤/베이스 클립이 아직 없으면 빈 목록을 돌려준다.
        std::vector<std::pair<glm::vec3, glm::vec3>> ComputeBoneWorldLines() const;

    private:
        struct LayerEntry
        {
            int id = -1;
            KeyframeClip clip;
            float frame = 0.0f;
            float weight = 1.0f;
            MaskPreset mask = MaskPreset::Full;
        };

        LayerEntry* FindLayer(int layerId);
        const LayerEntry* FindLayer(int layerId) const;

        std::optional<Skeleton> skeleton_;
        std::optional<KeyframeClip> baseClip_;
        float baseFrame_ = 0.0f;

        std::vector<LayerEntry> layers_;  // Add된 순서 = 블렌드 override 순서(뒤일수록 우선)
        int nextLayerId_ = 0;

        std::string lastError_;
    };
}
