#pragma once

#include "KeyframeClip.h"
#include "Pose.h"
#include "Skeleton.h"
#include <expected>
#include <unordered_map>
#include <vector>

namespace Engine
{
    /// <summary>
    /// 레이어가 스켈레톤의 어느 부분에 영향을 주는지 결정하는 프리셋 (착수 계약서 §C7/§C8).
    /// </summary>
    enum class MaskPreset
    {
        Full,        // 모든 본 = 1
        UpperBody,   // Spine/Arm/Hand/Head 접두사로 시작하는 본 = 1, 나머지 = 0
        LowerBody,   // Hips/Leg/Foot 접두사로 시작하는 본 = 1, 나머지 = 0
        Custom,      // customPerBoneWeight 맵 사용 (§C7: "missing entry = 0")
    };

    /// <summary>
    /// 믹서에 넣을 레이어 하나. 착수 계약서 §C7의 Layer를 따르되, sampledPose를 명시적으로
    /// 들고 있다 - 계약서 pseudocode의 `sampledPoseOf(L.clip)`(주석: "caller가 1회 샘플링한
    /// 결과 재사용")를 구체화한 것이다. 레이어마다 재생 시간(frame)이 다를 수 있으므로(각자
    /// AnimationPlayer를 갖는 상황), 어떤 frame에서 샘플링했는지는 LayerMixer의 관심사가
    /// 아니고 호출자(Phase 4/5의 재생 로직)의 책임이다.
    /// </summary>
    struct LayerSpec
    {
        const KeyframeClip* clip = nullptr;
        Pose sampledPose;                  // skeleton과 같은 길이/순서 - 호출자가 Sample()로 미리 채움
        float weight = 1.0f;               // clamp[0,1]
        MaskPreset maskPreset = MaskPreset::Full;

        // MaskPreset::Custom 전용, 1차 미사용 슬롯(§C7 원문: "posteri에" - 나중 확장 자리).
        // 키는 skeleton 본 인덱스, 값은 [0,1] 가중치. 없는 본은 0(§C7 "missing entry = 0").
        std::unordered_map<int, float> customPerBoneWeight;
    };

    /// <summary>
    /// 본 하나에 대한 레이어의 실효 마스크 가중치 [0,1]을 계산한다 (§C7/§C8).
    /// </summary>
    float MaskWeightForBone(const LayerSpec& layer, int boneIndex, const Skeleton& skeleton);

    /// <summary>
    /// §C8 마지막 문단: "MVP 에서 두 preset 이 동일 본을 1 로 예정하면 빌드/load 단계 단언
    /// failure". UpperBody/LowerBody 접두사 테이블 자체가 서로 겹치지 않는지 확인하는
    /// 정적 검증. 데이터가 코드에 고정돼 있어 매 MaskWeightForBone 호출마다 검사하는 건
    /// 낭비이므로, 유닛 테스트(MaskPresetTest)에서 한 번 호출해 회귀를 잡는 용도로 둔다.
    /// </summary>
    bool AreMaskPresetTablesNonOverlapping();

    enum class MixerError
    {
        IncompatibleAtLayerIndex,      // 스켈레톤과 레이어 클립의 시그니처가 호환되지 않음
        EmptyLayerClip,                // layer.clip == nullptr
        InvalidWeightRangeAfterClamp,  // clamp 후에도 [0,1] 밖(NaN 등 - 코딩 버그 신호)
    };

    /// <summary>
    /// 순서 있는 override 스택으로 레이어들을 baseLocalPose 위에 pairwise 누적 블렌드한다
    /// (착수 계약서 §C7 "mixLayers", §C13 "compose" - 같은 연산에 두 이름이 붙어 있어서
    /// Phase 3 요청에서 쓴 이름인 MixLayers로 통일).
    ///
    /// 의미론:
    ///   - "pairwise 누적" = base에 layer[0]을 (weight*mask)만큼 lerp한 결과에, layer[1]을
    ///     다시 그만큼 lerp... 순서대로 누적한다. "각 레이어가 N%씩 균등하게 섞인다"는
    ///     뜻이 아니다.
    ///   - "순서 있는 override" = 뒤에 오는 레이어일수록 우선순위가 높다(마지막에 덧칠됨).
    ///   - weight=1, mask=1인 단일 레이어는 그 클립 포즈로 완전히 교체(override)된다.
    ///
    /// 각 레이어 적용 전에 IsCompatible(skeleton.GetSignature(), layer.clip->signature)를
    /// 확인한다 - 호환되지 않으면 그 레이어부터는 아무것도 섞지 않고 즉시 실패를 반환한다
    /// (부분적으로 섞인 Pose를 반환하지 않는다 - §C10의 "partial object 반환 금지" 원칙을
    /// 여기서도 지킨다).
    ///
    /// outFailedLayerIndex: 실패 시 orderedLayers 안에서 몇 번째 레이어가 원인인지 채운다
    /// (nullptr이면 무시). 반환 타입 자체(§C13: `expected<Pose, MixerError>`)는 계약서
    /// 원문 그대로 유지하기 위해 인덱스를 별도 out 파라미터로 뺐다 - MixerError를 구조체로
    /// 바꾸는 대신 선택한 절충.
    /// </summary>
    std::expected<Pose, MixerError> MixLayers(
        const Skeleton& skeleton,
        const Pose& baseLocalPose,
        const std::vector<LayerSpec>& orderedLayers,
        int* outFailedLayerIndex = nullptr);
}
