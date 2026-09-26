#pragma once

#include "KeyframeClip.h"
#include "Pose.h"
#include "Skeleton.h"

namespace Engine
{
    enum class LoopMode
    {
        Clamp,  // 1차 MVP: 클립 끝(마지막 프레임)에서 정지. Loop/PingPong 등은 이후 확장 슬롯.
    };

    /// <summary>
    /// 스켈레톤+클립을 재생 상태(현재 시간/속도/루프 정책)와 함께 들고 있는 재생기.
    /// 착수 계약서 §C4/§C5. Sample()은 순수 함수이고, 이 클래스는 그 위에 시간 상태만 얹는다.
    ///
    /// 소유권: skeleton/clip은 참조만 보관한다(포인터) - 호출자가 두 객체의 수명을
    /// AnimationPlayer보다 길게 유지해야 한다.
    /// </summary>
    class AnimationPlayer
    {
    public:
        void load(const Skeleton& skeleton, const KeyframeClip& clip, LoopMode loop = LoopMode::Clamp);
        void setPlaybackSpeed(float speed);

        // currentTime += deltaSeconds * speed; 루프 정책 적용 (1차 MVP = Clamp).
        void update(float deltaSeconds);

        // = Sample(skeleton, clip, currentTime() * clip.fps)
        Pose currentLocalPose() const;

        float currentTime() const { return currentTime_; }

    private:
        const Skeleton* skeleton_ = nullptr;
        const KeyframeClip* clip_ = nullptr;
        float currentTime_ = 0.0f;
        float playbackSpeed_ = 1.0f;
        LoopMode loopMode_ = LoopMode::Clamp;
    };
}
