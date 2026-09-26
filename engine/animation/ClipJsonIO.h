#pragma once

#include "KeyframeClip.h"
#include "Skeleton.h"
#include <expected>
#include <string>

namespace Engine
{
    enum class ClipJsonError
    {
        ParseError,
        UnsupportedFormat,       // "format" != "core.clip"
        UnsupportedVersion,      // "version" != 1
        UnsupportedAxis,
        UnsupportedUnit,
        MissingField,
        InvalidTrackData,
        UnknownBone,             // 트랙의 본 이름이 참조 스켈레톤에 없음 - 즉시 거절(휴리스틱 매핑 없음)
        DuplicateFrame,          // 같은 트랙 안에서 같은 frame이 두 번 나옴
        NonMonotonicFrame,       // frame이 오름차순이 아님
        NegativeFrame,           // frame < 0
        FrameOutOfRange,         // frame >= totalFrames
        InvalidFps,              // fps <= 0
        InvalidQuaternion,       // (0,0,0,0) - 정규화 불가능한 영 쿼터니언
        SignatureMismatch,       // JSON의 optional "signature" 필드가 참조 스켈레톤과 다름
    };

    /// <summary>
    /// *.clip.json 문자열을 referenceSkeleton 기준으로 파싱/검증한다 (착수 계약서 §C10).
    /// referenceSkeleton과 호환되지 않는 클립(알 수 없는 본, 시그니처 불일치 등)은 즉시
    /// 거절한다 - "가장 비슷한 본으로 매핑" 같은 휴리스틱은 여기서 하지 않는다.
    ///
    /// 성공 시 clip.signature는 referenceSkeleton.GetSignature()로 채워진다(§C4:
    /// "검증된 시그니처") - 이후 Phase 3의 호환성 판정에서 원본 스켈레톤 객체 없이도
    /// 이 클립이 어떤 구조에 쓰였는지 알 수 있게 하기 위함.
    /// </summary>
    std::expected<KeyframeClip, ClipJsonError> ParseClipJson(
        const std::string& jsonText, const Skeleton& referenceSkeleton);
}
