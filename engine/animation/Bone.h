#pragma once

#include "Transform.h"
#include <string>

namespace Engine
{
    /// <summary>
    /// 스켈레톤 자산 정의상의 본 하나. immutable한 자산 데이터 — 매 프레임 바뀌는 포즈는
    /// 여기 들어가지 않는다(포즈는 Pose에 별도로 담는다). 착수 계약서 §C2.
    /// </summary>
    struct Bone
    {
        std::string name;
        int parentIndex = -1;   // -1 = root
        Transform bind;         // 부모 기준 로컬 바인드 포즈 TRS (§C1: parent-local TRS 계약)
    };
}
