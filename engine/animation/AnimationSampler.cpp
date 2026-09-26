#include "AnimationSampler.h"
#include <algorithm>

namespace Engine
{
    glm::quat ShortestSlerp(const glm::quat& a, const glm::quat& b, float t)
    {
        glm::quat bFixed = b;
        if (glm::dot(a, bFixed) < 0.0f)
        {
            // a와 b가 같은 회전을 반대 부호로 표현하고 있다(quaternion double cover) - 부호를
            // 맞춰주지 않으면 slerp가 4D 구면에서 "먼 길"로 돌아가며 눈에 보이는 180도+ 회전이
            // 튀어나온다.
            bFixed = -bFixed;
        }
        return glm::slerp(a, bFixed, t);
    }

    namespace
    {
        const KeyframeTrack* FindTrack(const KeyframeClip& clip, const std::string& boneName)
        {
            for (const KeyframeTrack& track : clip.tracks)
            {
                if (track.boneName == boneName)
                {
                    return &track;
                }
            }
            return nullptr;
        }

        // track.keys는 frame 오름차순으로 정렬돼 있다고 가정한다(KeyframeClip.h 주석,
        // ClipJsonIO가 강제). bindTransform은 트랙이 있는 본이라도 scale은 애니메이션하지
        // 않으므로(Key에 scale 필드가 없음) scale 값을 그대로 가져오는 데 쓴다.
        Transform SampleTrack(const KeyframeTrack& track, float frame, const Transform& bindTransform)
        {
            Transform result;
            result.scale = bindTransform.scale;

            const auto& keys = track.keys;
            if (keys.empty())
            {
                result.position = bindTransform.position;
                result.rotation = bindTransform.rotation;
                return result;
            }

            if (frame <= keys.front().frame)
            {
                result.position = keys.front().position;
                result.rotation = glm::normalize(keys.front().rotation);
                return result;
            }
            if (frame >= keys.back().frame)
            {
                result.position = keys.back().position;
                result.rotation = glm::normalize(keys.back().rotation);
                return result;
            }

            // frame이 keys.front()와 keys.back() 사이 - 이분 탐색으로 [keyA, keyB] 구간을 찾는다.
            auto it = std::lower_bound(keys.begin(), keys.end(), frame,
                [](const KeyframeTrack::Key& key, float f) { return key.frame < f; });
            // 위 두 경계 체크(<=front, >=back) 덕분에 it은 항상 begin()과 end() 사이의
            // "내부" 위치를 가리킨다 (it != begin(), it != end()).
            const KeyframeTrack::Key& keyB = *it;
            const KeyframeTrack::Key& keyA = *(it - 1);

            float span = keyB.frame - keyA.frame;
            float t = (span > 0.0f) ? (frame - keyA.frame) / span : 0.0f;

            result.position = glm::mix(keyA.position, keyB.position, t);
            result.rotation = ShortestSlerp(glm::normalize(keyA.rotation), glm::normalize(keyB.rotation), t);
            return result;
        }
    }

    Pose Sample(const Skeleton& skeleton, const KeyframeClip& clip, float frame)
    {
        float clampedFrame = frame;
        if (clip.totalFrames == 0)
        {
            clampedFrame = 0.0f;
        }
        else if (clampedFrame < 0.0f)
        {
            clampedFrame = 0.0f;
        }
        else if (clampedFrame >= static_cast<float>(clip.totalFrames))
        {
            clampedFrame = static_cast<float>(clip.totalFrames - 1);
        }

        const auto& bones = skeleton.GetBones();
        Pose pose(bones.size());

        for (size_t i = 0; i < bones.size(); ++i)
        {
            const Bone& bone = bones[i];
            const KeyframeTrack* track = FindTrack(clip, bone.name);
            if (!track)
            {
                pose[i] = bone.bind; // 체크리스트 #13: 트랙 없는 본 -> 바인드 포즈 유지
                continue;
            }
            pose[i] = SampleTrack(*track, clampedFrame, bone.bind);
        }

        return pose;
    }
}
