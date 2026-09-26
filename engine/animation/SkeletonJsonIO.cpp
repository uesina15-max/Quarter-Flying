#include "SkeletonJsonIO.h"
#include <nlohmann/json.hpp>
#include <optional>

namespace Engine
{
    namespace
    {
        bool TryParseAxis(const std::string& s, AxisSystem& out)
        {
            if (s == "Y_UP_RH") { out = AxisSystem::Y_UP_RH; return true; }
            return false;
        }

        bool TryParseUnit(const std::string& s, LengthUnit& out)
        {
            if (s == "meter") { out = LengthUnit::Meter; return true; }
            if (s == "centimeter") { out = LengthUnit::Centimeter; return true; }
            return false;
        }

        std::optional<glm::vec3> ReadVec3(const nlohmann::json& parent, const char* key, const glm::vec3& fallback)
        {
            if (!parent.contains(key))
            {
                return fallback;
            }
            const auto& arr = parent[key];
            if (!arr.is_array() || arr.size() != 3)
            {
                return std::nullopt;
            }
            for (const auto& v : arr)
            {
                if (!v.is_number()) return std::nullopt;
            }
            return glm::vec3(arr[0].get<float>(), arr[1].get<float>(), arr[2].get<float>());
        }
    }

    std::expected<Skeleton, SkeletonJsonError> ParseSkeletonJson(const std::string& jsonText)
    {
        nlohmann::json root;
        try
        {
            root = nlohmann::json::parse(jsonText);
        }
        catch (const nlohmann::json::parse_error&)
        {
            return std::unexpected(SkeletonJsonError::ParseError);
        }

        if (!root.is_object()
            || !root.contains("format") || !root.contains("version")
            || !root.contains("axis") || !root.contains("unit")
            || !root.contains("bones"))
        {
            return std::unexpected(SkeletonJsonError::MissingField);
        }

        if (!root["format"].is_string() || root["format"].get<std::string>() != "core.skeleton")
        {
            return std::unexpected(SkeletonJsonError::UnsupportedFormat);
        }
        if (!root["version"].is_number_integer() || root["version"].get<int>() != 1)
        {
            return std::unexpected(SkeletonJsonError::UnsupportedVersion);
        }

        AxisSystem axis;
        if (!root["axis"].is_string() || !TryParseAxis(root["axis"].get<std::string>(), axis))
        {
            return std::unexpected(SkeletonJsonError::UnsupportedAxis);
        }

        LengthUnit unit;
        if (!root["unit"].is_string() || !TryParseUnit(root["unit"].get<std::string>(), unit))
        {
            return std::unexpected(SkeletonJsonError::UnsupportedUnit);
        }

        if (!root["bones"].is_array())
        {
            return std::unexpected(SkeletonJsonError::InvalidBoneData);
        }

        std::vector<Bone> bones;
        bones.reserve(root["bones"].size());
        for (const auto& boneJson : root["bones"])
        {
            if (!boneJson.is_object()
                || !boneJson.contains("name") || !boneJson["name"].is_string()
                || !boneJson.contains("parent") || !boneJson["parent"].is_number_integer())
            {
                return std::unexpected(SkeletonJsonError::InvalidBoneData);
            }

            Bone bone;
            bone.name = boneJson["name"].get<std::string>();
            bone.parentIndex = boneJson["parent"].get<int>();

            auto bindPos = ReadVec3(boneJson, "bindPos", glm::vec3(0.0f, 0.0f, 0.0f));
            auto bindScale = ReadVec3(boneJson, "bindScale", glm::vec3(1.0f, 1.0f, 1.0f));
            if (!bindPos || !bindScale)
            {
                return std::unexpected(SkeletonJsonError::InvalidBoneData);
            }

            glm::quat bindRot(1.0f, 0.0f, 0.0f, 0.0f);
            if (boneJson.contains("bindRot"))
            {
                const auto& rot = boneJson["bindRot"];
                if (!rot.is_array() || rot.size() != 4)
                {
                    return std::unexpected(SkeletonJsonError::InvalidBoneData);
                }
                for (const auto& v : rot)
                {
                    if (!v.is_number()) return std::unexpected(SkeletonJsonError::InvalidBoneData);
                }
                // JSON 표기는 [x,y,z,w] (계약서 §2.1 예시와 동일) - glm::quat 생성자는
                // (w,x,y,z) 순서이므로 순서를 바꿔서 넘긴다.
                float x = rot[0].get<float>();
                float y = rot[1].get<float>();
                float z = rot[2].get<float>();
                float w = rot[3].get<float>();
                if (x == 0.0f && y == 0.0f && z == 0.0f && w == 0.0f)
                {
                    return std::unexpected(SkeletonJsonError::InvalidBoneData);
                }
                bindRot = glm::normalize(glm::quat(w, x, y, z));
            }

            bone.bind = Transform(*bindPos, bindRot, *bindScale);
            bones.push_back(std::move(bone));
        }

        auto skeleton = Skeleton::Create(std::move(bones), axis, unit);
        if (!skeleton.has_value())
        {
            return std::unexpected(SkeletonJsonError::SkeletonValidationFailed);
        }

        return std::move(skeleton.value());
    }
}
