#include "AnimationPlayer.h"
#include "AnimationSampler.h"
#include <algorithm>

namespace Engine
{
    void AnimationPlayer::load(const Skeleton& skeleton, const KeyframeClip& clip, LoopMode loop)
    {
        skeleton_ = &skeleton;
        clip_ = &clip;
        loopMode_ = loop;
        currentTime_ = 0.0f;
    }

    void AnimationPlayer::setPlaybackSpeed(float speed)
    {
        playbackSpeed_ = speed;
    }

    void AnimationPlayer::update(float deltaSeconds)
    {
        if (!clip_)
        {
            return;
        }

        currentTime_ += deltaSeconds * playbackSpeed_;

        // clip 길이(초) = totalFrames / fps. fps<=0인 비정상 클립은 0초 취급(방어적).
        float clipLengthSeconds = (clip_->fps > 0.0f)
            ? static_cast<float>(clip_->totalFrames) / clip_->fps
            : 0.0f;

        switch (loopMode_)
        {
        case LoopMode::Clamp:
        default:
            currentTime_ = std::clamp(currentTime_, 0.0f, clipLengthSeconds);
            break;
        }
    }

    Pose AnimationPlayer::currentLocalPose() const
    {
        if (!skeleton_ || !clip_)
        {
            return {};
        }
        float frame = currentTime_ * clip_->fps;
        return Sample(*skeleton_, *clip_, frame);
    }
}
