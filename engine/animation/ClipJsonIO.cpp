#include "ClipJsonIO.h"
#include <nlohmann/json.hpp>

namespace Engine
{
    namespace
    {
        bool IsSupportedAxisString(const std::string& s) { return s == "Y_UP_RH"; }
        bool IsSupportedUnitString(const std::string& s) { return s == "meter" || s == "centimeter"; }
    }

    std::expected<KeyframeClip, ClipJsonError> ParseClipJson(
        const std::string& jsonText, const Skeleton& referenceSkeleton)
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(jsonText);
        }
        catch (const nlohmann::json::parse_error&)
        {
            return std::unexpected(ClipJsonError::ParseError);
        }

        if (!root.is_object()
            || !root.contains("format") || !root.contains("version")
            || !root.contains("axis") || !root.contains("unit")
            || !root.contains("fps") || !root.contains("totalFrames")
            || !root.contains("tracks"))
        {
            return std::unexpected(ClipJsonError::MissingField);
        }

        if (!root["format"].is_string() || root["format"].get<std::string>() != "core.clip")
        {
            return std::unexpected(ClipJsonError::UnsupportedFormat);
        }
        if (!root["version"].is_number_integer() || root["version"].get<int>() != 1)
        {
            return std::unexpected(ClipJsonError::UnsupportedVersion);
        }
        if (!root["axis"].is_string() || !IsSupportedAxisString(root["axis"].get<std::string>()))
        {
            return std::unexpected(ClipJsonError::UnsupportedAxis);
        }
        if (!root["unit"].is_string() || !IsSupportedUnitString(root["unit"].get<std::string>()))
        {
            return std::unexpected(ClipJsonError::UnsupportedUnit);
        }
        if (!root["fps"].is_number() || root["fps"].get<float>() <= 0.0f)
        {
            return std::unexpected(ClipJsonError::InvalidFps);
        }
        if (!root["totalFrames"].is_number_unsigned())
        {
            return std::unexpected(ClipJsonError::MissingField);
        }
        if (!root["tracks"].is_array())
        {
            return std::unexpected(ClipJsonError::InvalidTrackData);
        }

        KeyframeClip clip;
        clip.name = root.value("name", std::string());
        clip.skeletonRef = root.value("skeletonRef", std::string());
        clip.fps = root["fps"].get<float>();
        clip.totalFrames = root["totalFrames"].get<uint32_t>();

        // optional fast-reject signature 필드 (hex64) - 있으면 참조 스켈레톤과 미리 비교.
        if (root.contains("signature") && !root["signature"].is_null())
        {
            if (!root["signature"].is_string())
            {
                return std::unexpected(ClipJsonError::InvalidTrackData);
            }
            uint64_t declaredHash = 0;
            try
            {
                declaredHash = std::stoull(root["signature"].get<std::string>(), nullptr, 16);
            }
            catch (...)
            {
                return std::unexpected(ClipJsonError::InvalidTrackData);
            }
            if (declaredHash != referenceSkeleton.GetSignature().hash)
            {
                return std::unexpected(ClipJsonError::SignatureMismatch);
            }
        }

        for (const auto& trackJson : root["tracks"])
        {
            if (!trackJson.is_object()
                || !trackJson.contains("bone") || !trackJson["bone"].is_string()
                || !trackJson.contains("keyframes") || !trackJson["keyframes"].is_array())
            {
                return std::unexpected(ClipJsonError::InvalidTrackData);
            }

            KeyframeTrack track;
            track.boneName = trackJson["bone"].get<std::string>();

            if (referenceSkeleton.FindBoneIndex(track.boneName) < 0)
            {
                return std::unexpected(ClipJsonError::UnknownBone);
            }

            bool first = true;
            float previousFrame = -1.0f;
            for (const auto& keyJson : trackJson["keyframes"])
            {
                if (!keyJson.is_object()
                    || !keyJson.contains("frame") || !keyJson["frame"].is_number()
                    || !keyJson.contains("pos") || !keyJson["pos"].is_array() || keyJson["pos"].size() != 3
                    || !keyJson.contains("rot") || !keyJson["rot"].is_array() || keyJson["rot"].size() != 4)
                {
                    return std::unexpected(ClipJsonError::InvalidTrackData);
                }

                float frame = keyJson["frame"].get<float>();
                if (frame < 0.0f)
                {
                    return std::unexpected(ClipJsonError::NegativeFrame);
                }
                if (frame >= static_cast<float>(clip.totalFrames))
                {
                    return std::unexpected(ClipJsonError::FrameOutOfRange);
                }
                if (!first)
                {
                    if (frame == previousFrame)
                    {
                        return std::unexpected(ClipJsonError::DuplicateFrame);
                    }
                    if (frame < previousFrame)
                    {
                        return std::unexpected(ClipJsonError::NonMonotonicFrame);
                    }
                }
                first = false;
                previousFrame = frame;

                const auto& posArr = keyJson["pos"];
                const auto& rotArr = keyJson["rot"];

                KeyframeTrack::Key key;
                key.frame = frame;
                key.position = glm::vec3(posArr[0].get<float>(), posArr[1].get<float>(), posArr[2].get<float>());

                // JSON 표기는 [x,y,z,w] - glm::quat 생성자는 (w,x,y,z) 순서.
                float x = rotArr[0].get<float>();
                float y = rotArr[1].get<float>();
                float z = rotArr[2].get<float>();
                float w = rotArr[3].get<float>();
                if (x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f)
                {
                    return std::unexpected(ClipJsonError::InvalidQuaternion);
                }
                key.rotation = glm::quat(w, x, y, z);  // 정규화는 Sample()에서 수행

                track.keys.push_back(key);
            }

            clip.tracks.push_back(std::move(track));
        }

        clip.signature = referenceSkeleton.GetSignature();

        return clip;
    }
}
