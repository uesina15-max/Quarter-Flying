#include "GameConfig.h"
#include "../core/Utf8Path.h"
#include <fmt/format.h>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace Engine
{
    namespace
    {
        constexpr const char* kComponent = "GameConfig";

        std::unexpected<EngineError> Invalid(const std::string& key, const char* expected)
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  fmt::format("game.json: '{}' must be {}", key, expected), kComponent);
        }

        // 키가 있으면 타입을 검사해 out에 넣는다. 없으면 out(기본값)을 그대로 둔다.
        std::expected<void, EngineError> ReadString(const nlohmann::json& obj, const char* key,
                                                    const std::string& fullKey, std::string& out)
        {
            if (!obj.contains(key)) return {};
            if (!obj[key].is_string()) return Invalid(fullKey, "a string");
            out = obj[key].get<std::string>();
            return {};
        }

        std::expected<void, EngineError> ReadBool(const nlohmann::json& obj, const char* key,
                                                  const std::string& fullKey, bool& out)
        {
            if (!obj.contains(key)) return {};
            if (!obj[key].is_boolean()) return Invalid(fullKey, "true or false");
            out = obj[key].get<bool>();
            return {};
        }

        std::expected<void, EngineError> ReadUInt(const nlohmann::json& obj, const char* key,
                                                  const std::string& fullKey, uint32_t& out, bool positive)
        {
            if (!obj.contains(key)) return {};
            const auto& v = obj[key];
            if (!v.is_number_integer() || v.get<int64_t>() < (positive ? 1 : 0) || v.get<int64_t>() > 100000)
            {
                return Invalid(fullKey, positive ? "a positive integer" : "a non-negative integer");
            }
            out = static_cast<uint32_t>(v.get<int64_t>());
            return {};
        }

        std::expected<void, EngineError> ReadSection(const nlohmann::json& root, const char* key,
                                                     const nlohmann::json*& out)
        {
            out = nullptr;
            if (!root.contains(key)) return {};
            if (!root[key].is_object()) return Invalid(key, "an object");
            out = &root[key];
            return {};
        }
    }

    std::expected<GameConfig, EngineError> ParseGameConfig(const nlohmann::json& json)
    {
        if (!json.is_object())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter, "game.json: top level must be an object", kComponent);
        }

        GameConfig cfg;
        if (json.contains("version"))
        {
            if (!json["version"].is_number_integer()) return Invalid("version", "an integer");
            cfg.version = json["version"].get<int>();
            if (cfg.version > kGameConfigVersion)
            {
                return MakeUnexpected(EngineErrorCode::NotSupported,
                                      fmt::format("game.json: version {} is newer than this player supports ({})",
                                                  cfg.version, kGameConfigVersion), kComponent);
            }
        }

#define QF_TRY(expr) do { auto _r = (expr); if (!_r) return std::unexpected(_r.error()); } while (0)
        QF_TRY(ReadString(json, "title", "title", cfg.title));
        QF_TRY(ReadString(json, "startScene", "startScene", cfg.startScene));
        QF_TRY(ReadString(json, "assetRoot", "assetRoot", cfg.assetRoot));
        QF_TRY(ReadString(json, "inputActions", "inputActions", cfg.inputActions));

        const nlohmann::json* window = nullptr;
        QF_TRY(ReadSection(json, "window", window));
        if (window)
        {
            QF_TRY(ReadUInt(*window, "width", "window.width", cfg.windowWidth, true));
            QF_TRY(ReadUInt(*window, "height", "window.height", cfg.windowHeight, true));
            QF_TRY(ReadBool(*window, "fullscreen", "window.fullscreen", cfg.fullscreen));
            QF_TRY(ReadBool(*window, "cursorLock", "window.cursorLock", cfg.cursorLock));
        }

        const nlohmann::json* engine = nullptr;
        QF_TRY(ReadSection(json, "engine", engine));
        if (engine)
        {
            QF_TRY(ReadUInt(*engine, "numWorkerThreads", "engine.numWorkerThreads", cfg.numWorkerThreads, false));
            QF_TRY(ReadUInt(*engine, "logFrameInterval", "engine.logFrameInterval", cfg.logFrameInterval, false));
        }
#undef QF_TRY

        if (cfg.startScene.empty())
        {
            return MakeUnexpected(EngineErrorCode::InvalidParameter,
                                  "game.json: 'startScene' is required (path of the first scene, relative to game.json)",
                                  kComponent);
        }
        return cfg;
    }

    std::expected<GameConfig, EngineError> LoadGameConfig(const std::string& path)
    {
        const std::filesystem::path p = PathFromUtf8(path);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(p, ec))
        {
            return MakeUnexpected(EngineErrorCode::FileNotFound, fmt::format("game.json not found: '{}'", path), kComponent);
        }
        std::ifstream in(p, std::ios::binary);
        if (!in)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed, fmt::format("cannot read '{}'", path), kComponent);
        }
        nlohmann::json json;
        try
        {
            in >> json;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return MakeUnexpected(EngineErrorCode::ResourceLoadFailed,
                                  fmt::format("JSON syntax error in '{}': {}", path, e.what()), kComponent);
        }
        return ParseGameConfig(json);
    }
}
