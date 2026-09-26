#pragma once

#include "Transform.h"
#include <vector>

namespace Engine
{
    /// <summary>
    /// 스켈레톤의 모든 본에 대한 로컬(부모-상대) Transform 모음.
    /// 항상 대상 Skeleton::GetBones()와 같은 순서·같은 길이를 유지한다 (착수 계약서 §C1).
    /// </summary>
    using Pose = std::vector<Transform>;
}
