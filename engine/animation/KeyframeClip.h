#pragma once

#include "SkeletonSignature.h"
#include <cstdint>
#include <glm/gtc/quaternion.hpp>
#include <glm/vec3.hpp>
#include <string>
#include <vector>

namespace Engine
{
    /// <summary>
    /// 본 하나에 대한 키프레임 트랙 (착수 계약서 §C4).
    ///
    /// keys는 frame 오름차순(단조 증가, 중복 없음)이어야 한다 - 이 불변식은
    /// ClipJsonIO의 검증 단계에서 강제되고, 이 구조체 자체는 강제하지 않는다
    /// (유닛 테스트 등에서 직접 구성할 때도 이 순서를 지켜야 한다).
    ///
    /// scale은 애니메이션하지 않는다(Key에 scale 필드가 없음 - §C4 원문 그대로) - 트랙이
    /// 있는 본이라도 Transform.scale은 항상 스켈레톤의 바인드 스케일을 그대로 쓴다
    /// (AnimationSampler.cpp 참고).
    /// </summary>
    struct KeyframeTrack
    {
        std::string boneName;

        struct Key
        {
            float frame = 0.0f;             // >= 0
            glm::vec3 position{ 0.0f, 0.0f, 0.0f };
            glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };  // 정규화 책임은 Sample()에 있음
        };

        std::vector<Key> keys;
    };

    /// <summary>
    /// 하나의 애니메이션 클립. 착수 계약서 §C4/§C10.
    /// </summary>
    struct KeyframeClip
    {
        std::string name;
        std::string skeletonRef;              // 사람이 읽는 라벨 (예: "HumanoidBasic")
        SkeletonSignature signature;          // 검증에 사용된 참조 스켈레톤의 시그니처
        float fps = 30.0f;
        uint32_t totalFrames = 0;
        std::vector<KeyframeTrack> tracks;
    };
}
