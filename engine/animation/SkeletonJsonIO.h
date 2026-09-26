#pragma once

#include "Skeleton.h"
#include <expected>
#include <string>

namespace Engine
{
    enum class SkeletonJsonError
    {
        ParseError,
        UnsupportedFormat,          // "format" != "core.skeleton"
        UnsupportedVersion,         // "version" != 1
        UnsupportedAxis,            // "axis"가 지원하는 값(Y_UP_RH)이 아님
        UnsupportedUnit,            // "unit"이 지원하는 값(meter/centimeter)이 아님
        MissingField,
        InvalidBoneData,
        SkeletonValidationFailed,   // Skeleton::Create가 거부(이름 중복/사이클/유효하지 않은 부모 등)
    };

    /// <summary>
    /// *.skeleton.json 문자열을 파싱해 검증된 Skeleton을 만든다 (착수 계약서 §C10).
    /// 스키마 검증(format/version/axis/unit/필드 존재) -> 의미 검증(Skeleton::Create) 순서.
    /// 실패 시 부분적으로 만들어진 Skeleton을 반환하지 않는다 - expected만 반환한다.
    /// </summary>
    std::expected<Skeleton, SkeletonJsonError> ParseSkeletonJson(const std::string& jsonText);
}
