#include "SceneIO.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Reflection.h"
#include "../core/logging/Logger.h"
#include "../core/Utf8Path.h"   // 경로 문자열은 UTF-8
#include <fmt/format.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <system_error>

namespace Engine
{
    namespace
    {
        constexpr const char* kComponent = "SceneIO";

        std::unexpected<EngineError> Fail(EngineErrorCode code, std::string message)
        {
            Logger::Error("SceneIO - {}", message);
            return MakeUnexpected(code, std::move(message), kComponent);
        }

        void LogUnknownComponents(const char* verb, const std::string& path, const std::vector<std::string>& names)
        {
            if (names.empty()) return;
            std::string joined;
            for (const auto& n : names) { if (!joined.empty()) joined += ", "; joined += n; }
            Logger::Warning("SceneIO - {} '{}': components not registered in ComponentRegistry were skipped "
                            "(their data is not in the scene): {}", verb, path, joined);
        }
    }

    nlohmann::json BuildSceneDocument(ECSRegistry& registry, SceneSaveReport* report)
    {
        nlohmann::json doc = SerializeRegistry(registry);
        doc["format"] = kSceneFormatName;
        doc["version"] = kSceneFormatVersion;

        if (report)
        {
            report->entityCount = doc["entities"].size();
            const auto& registered = ComponentRegistry::GetAllComponents();
            std::set<std::string> missing;
            for (Entity entity : registry.GetAllEntities())
            {
                for (const std::string& name : registry.GetComponentTypeNames(entity))
                {
                    if (registered.find(name) == registered.end())
                    {
                        missing.insert(name);
                    }
                }
            }
            report->unregisteredComponents.assign(missing.begin(), missing.end());
        }
        return doc;
    }

    std::expected<SceneLoadReport, EngineError> ValidateSceneDocument(const nlohmann::json& doc)
    {
        if (!doc.is_object() || !doc.contains("format") || !doc["format"].is_string()
            || doc["format"].get<std::string>() != kSceneFormatName)
        {
            return Fail(EngineErrorCode::InvalidParameter,
                        fmt::format("not a scene file: missing or wrong \"format\" header (expected \"{}\")", kSceneFormatName));
        }
        if (!doc.contains("version") || !doc["version"].is_number_integer())
        {
            return Fail(EngineErrorCode::InvalidParameter, "scene file has no integer \"version\" field");
        }
        const int version = doc["version"].get<int>();
        if (version > kSceneFormatVersion)
        {
            return Fail(EngineErrorCode::NotSupported,
                        fmt::format("scene file version {} is newer than this engine supports ({})", version, kSceneFormatVersion));
        }
        if (!doc.contains("entities") || !doc["entities"].is_array())
        {
            return Fail(EngineErrorCode::ResourceLoadFailed, "scene file has no \"entities\" array");
        }

        SceneLoadReport report;
        report.entityCount = doc["entities"].size();
        const auto& registered = ComponentRegistry::GetAllComponents();
        std::set<std::string> unknown;
        for (const auto& entityJson : doc["entities"])
        {
            if (!entityJson.is_object() || !entityJson.contains("uuid") || !entityJson["uuid"].is_number_unsigned())
            {
                return Fail(EngineErrorCode::ResourceLoadFailed, "scene file has an entity without a numeric \"uuid\"");
            }
            if (entityJson.contains("components") && entityJson["components"].is_object())
            {
                for (const auto& item : entityJson["components"].items())
                {
                    if (registered.find(item.key()) == registered.end())
                    {
                        unknown.insert(item.key());
                    }
                }
            }
        }
        report.unknownComponents.assign(unknown.begin(), unknown.end());
        return report;
    }

    std::expected<SceneLoadReport, EngineError> ApplySceneDocument(ECSRegistry& registry, const nlohmann::json& doc)
    {
        auto validated = ValidateSceneDocument(doc);
        if (!validated)
        {
            return std::unexpected(validated.error());
        }

        // 먼저 빈 레지스트리에 불러 본다. 컴포넌트 deserialize가 타입 오류로 예외를 던지면 그 시점까지
        // 대상 레지스트리가 반쯤 채워진 채로 남기 때문이다.
        try
        {
            ECSRegistry probe;
            DeserializeRegistry(probe, doc);
        }
        catch (const std::exception& e)
        {
            return Fail(EngineErrorCode::ResourceLoadFailed, fmt::format("scene entity data is corrupt: {}", e.what()));
        }

        registry.Clear();
        DeserializeRegistry(registry, doc);
        return validated;
    }

    std::expected<SceneSaveReport, EngineError> SaveSceneToFile(ECSRegistry& registry, const std::string& path)
    {
        SceneSaveReport report;
        const nlohmann::json doc = BuildSceneDocument(registry, &report);

        const std::filesystem::path target = PathFromUtf8(path);
        std::filesystem::path tmp = target;
        tmp += ".tmp";

        std::error_code ec;
        if (target.has_parent_path())
        {
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec)
            {
                return Fail(EngineErrorCode::ResourceLoadFailed,
                            fmt::format("cannot create folder for '{}': {}", path, ec.message()));
            }
        }

        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            if (!out)
            {
                return Fail(EngineErrorCode::ResourceLoadFailed, fmt::format("cannot open '{}' for writing", path));
            }
            out << doc.dump(2) << '\n';
            out.flush();
            if (!out)
            {
                out.close();
                std::filesystem::remove(tmp, ec);
                return Fail(EngineErrorCode::ResourceLoadFailed, fmt::format("write failed for '{}'", path));
            }
        }

        // MSVC의 rename은 MoveFileExW(MOVEFILE_REPLACE_EXISTING)라 기존 파일을 한 번에 교체한다.
        std::filesystem::rename(tmp, target, ec);
        if (ec)
        {
            std::error_code ignore;
            std::filesystem::remove(tmp, ignore);
            return Fail(EngineErrorCode::ResourceLoadFailed,
                        fmt::format("cannot replace '{}': {}", path, ec.message()));
        }

        LogUnknownComponents("saved", path, report.unregisteredComponents);
        Logger::Info("SceneIO - saved '{}' ({} entities)", path, report.entityCount);
        return report;
    }

    void UpgradeSceneDocument(nlohmann::json& doc, const std::string& projectRoot, std::vector<std::string>* warnings)
    {
        if (!doc.is_object() || !doc.contains("version") || !doc["version"].is_number_integer()
            || !doc.contains("entities") || !doc["entities"].is_array())
        {
            return;
        }
        FileLoadContext ctx;
        ctx.fileKind = "scene";
        ctx.fileVersion = doc["version"].get<int>();
        ctx.projectRoot = projectRoot;
        ctx.warnings = warnings;
        for (auto& entityJson : doc["entities"])
        {
            if (entityJson.is_object() && entityJson.contains("components"))
            {
                UpgradeComponentsFromFile(entityJson["components"], ctx);
            }
        }
    }

    std::expected<SceneLoadReport, EngineError> LoadSceneFromFile(ECSRegistry& registry, const std::string& path,
                                                                  const std::string& projectRoot)
    {
        const std::filesystem::path source = PathFromUtf8(path);
        std::error_code ec;
        if (!std::filesystem::is_regular_file(source, ec))
        {
            return Fail(EngineErrorCode::FileNotFound, fmt::format("scene file not found: '{}'", path));
        }

        std::ifstream in(source, std::ios::binary);
        if (!in)
        {
            return Fail(EngineErrorCode::ResourceLoadFailed, fmt::format("cannot open '{}' for reading", path));
        }

        nlohmann::json doc;
        try
        {
            in >> doc;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            return Fail(EngineErrorCode::ResourceLoadFailed, fmt::format("JSON syntax error in '{}': {}", path, e.what()));
        }

        std::vector<std::string> upgradeWarnings;
        UpgradeSceneDocument(doc, projectRoot, &upgradeWarnings);

        auto result = ApplySceneDocument(registry, doc);
        if (!result)
        {
            // 원인 메시지에 파일 경로를 붙여서 돌려준다(어느 파일이 문제인지 호출자가 알 수 있게).
            EngineError err = result.error();
            err.message = fmt::format("{}: {}", path, err.message);
            return std::unexpected(err);
        }
        result->warnings = std::move(upgradeWarnings);

        LogUnknownComponents("loaded", path, result->unknownComponents);
        Logger::Info("SceneIO - loaded '{}' ({} entities)", path, result->entityCount);
        return result;
    }
}
