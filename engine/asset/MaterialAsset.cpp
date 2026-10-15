#include "MaterialAsset.h"
#include "../core/Utf8Path.h"
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace Engine
{
    namespace
    {
        constexpr const char* kComponent = "MaterialAsset";

        std::unexpected<EngineError> Invalid(const std::string& label, const std::string& what)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, fmt::format("{}: {}", label, what), kComponent);
        }
    }

    Result<MaterialData> ParseMaterial(const nlohmann::json& json, const std::string& label)
    {
        if (!json.is_object())
        {
            return Invalid(label, "top level must be an object");
        }
        if (!json.contains("format") || !json["format"].is_string() || json["format"].get<std::string>() != kMaterialFormatName)
        {
            return Invalid(label, fmt::format("'format' must be \"{}\" (not a material file?)", kMaterialFormatName));
        }
        if (!json.contains("version") || !json["version"].is_number_integer())
        {
            return Invalid(label, "needs an integer 'version'");
        }
        if (json["version"].get<int>() > kMaterialFormatVersion)
        {
            return MakeUnexpected(EngineErrorCode::NotSupported,
                                  fmt::format("{}: version {} is newer than this engine supports ({})", label,
                                              json["version"].get<int>(), kMaterialFormatVersion), kComponent);
        }

        MaterialData m;
        if (json.contains("baseColor"))
        {
            const auto& c = json["baseColor"];
            if (!c.is_array() || c.size() != 3 || !c[0].is_number() || !c[1].is_number() || !c[2].is_number())
            {
                return Invalid(label, "'baseColor' must be an array of 3 numbers");
            }
            m.baseColor = { c[0].get<float>(), c[1].get<float>(), c[2].get<float>() };
        }
        if (json.contains("texture"))
        {
            if (!json["texture"].is_string())
            {
                return Invalid(label, "'texture' must be a string (project-relative path)");
            }
            m.texture = json["texture"].get<std::string>();
        }
        for (auto [key, out] : { std::pair<const char*, float*>{ "roughness", &m.roughness },
                                 std::pair<const char*, float*>{ "metallic", &m.metallic } })
        {
            if (!json.contains(key))
            {
                continue;
            }
            if (!json[key].is_number())
            {
                return Invalid(label, fmt::format("'{}' must be a number", key));
            }
            const float v = json[key].get<float>();
            if (v < 0.0f || v > 1.0f)
            {
                return Invalid(label, fmt::format("'{}' must be between 0 and 1 (got {})", key, v));
            }
            *out = v;
        }
        return m;
    }

    Result<MaterialData> LoadMaterialFile(const std::string& utf8Path)
    {
        const std::filesystem::path path = PathFromUtf8(utf8Path);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(path, ec))
        {
            return MakeUnexpected(EngineErrorCode::FileNotFound, fmt::format("material file not found: '{}'", utf8Path),
                                  kComponent);
        }
        std::ifstream in(path, std::ios::binary);
        if (!in)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed, fmt::format("cannot read '{}'", utf8Path), kComponent);
        }
        nlohmann::json json;
        try
        {
            in >> json;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed,
                                  fmt::format("JSON syntax error in '{}': {}", utf8Path, e.what()), kComponent);
        }
        return ParseMaterial(json, utf8Path);
    }
}
