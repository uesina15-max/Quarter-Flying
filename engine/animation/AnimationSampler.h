#pragma once

#include "KeyframeClip.h"
#include "Pose.h"
#include "Skeleton.h"

namespace Engine
{
    /// <summary>
    /// 착수 계약서 §C6: 두 쿼터니언 사이를 최단 경로로 slerp한다. 부호가 반대(같은 회전을
    /// 나타내는 -q)인 경우를 뒤집어서, 180도 근방에서 불필요하게 먼 길로 도는 걸 막는다.
    /// </summary>
    glm::quat ShortestSlerp(const glm::quat& a, const glm::quat& b, float t);

    /// <summary>
    /// 주어진 프레임에서 클립을 스켈레톤에 맞춰 샘플링한다 (착수 계약서 §C4, §C5).
    ///
    /// 계약서의 pseudocode(`Pose Sample(const KeyframeClip&, float)`)는 skeleton 인자가
    /// 없지만, "missing track = bind pose 유지"(체크리스트 #13)를 만족하려면 결과 Pose의
    /// 본 순서/개수와 트랙이 없는 본의 대체값(바인드 포즈)을 알아야 하므로 skeleton을
    /// 명시적으로 받는다 - 실질적으로 필요한 최소한의 시그니처 보정이다.
    ///
    /// 동작:
    ///   - frame < 0            -> 0으로 clamp
    ///   - frame >= totalFrames -> totalFrames-1로 clamp (totalFrames==0이면 0)
    ///   - 본에 해당하는 트랙이 없으면 그 본은 skeleton의 bind pose를 그대로 사용
    ///   - 트랙이 있으면 position은 lerp, rotation은 최단경로 slerp(ShortestSlerp)로 보간
    ///   - 트랙의 키 범위 밖(clamp된 frame이 첫/마지막 키보다 바깥)이면 가장 가까운
    ///     키 값을 그대로 유지(추가 보간 없이 hold, extrapolation 없음)
    ///   - scale은 항상 skeleton bind pose의 scale을 사용(Key에 scale이 없으므로)
    /// </summary>
    Pose Sample(const Skeleton& skeleton, const KeyframeClip& clip, float frame);
}
