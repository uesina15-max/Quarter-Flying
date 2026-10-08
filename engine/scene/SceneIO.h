#pragma once

#include "../core/EngineError.h"
#include <nlohmann/json.hpp>
#include <expected>
#include <string>
#include <vector>

namespace Engine
{
    class ECSRegistry;

    // ========================================
    // Scene file I/O (docs/GAME_BUILD_PLAN.md Phase 1, D6)
    // ========================================
    //
    // 씬 파일(*.scene.json) = SerializeRegistry() 출력 + 헤더 두 개:
    //   { "format": "quarterflying.scene", "version": 1, "entities": [ ... ] }
    // 새 형식을 만들지 않고 에디터 Play/Stop 스냅샷과 같은 직렬화를 쓴다. 그래야 에디터 저장과
    // Player 로드가 같은 코드를 지나고, 형식이 어긋날 수 없다.
    //
    // 실패는 원인별로 다른 에러 코드로 돌려준다(코드베이스 관례: 조용히 실패하지 않는다):
    //   FileNotFound        - 파일이 없다
    //   ResourceLoadFailed  - 읽기/쓰기 실패, JSON 문법 오류, 엔티티 데이터 손상
    //   InvalidParameter    - "format" 헤더가 없거나 다르다(씬 파일이 아님)
    //   NotSupported        - 이 엔진보다 새 버전의 씬 파일이다

    inline constexpr const char* kSceneFormatName = "quarterflying.scene";
    inline constexpr int kSceneFormatVersion = 1;

    struct SceneSaveReport
    {
        size_t entityCount = 0;
        // 엔티티에 붙어 있지만 ComponentRegistry에 등록되지 않아 파일에 들어가지 않은 컴포넌트 이름.
        // SerializeRegistry는 등록된 컴포넌트만 저장하므로, 이게 비어 있지 않으면 저장 → 불러오기에서
        // 그 데이터가 사라진다(PrefabInstanceComponent.h 주석과 같은 함정).
        std::vector<std::string> unregisteredComponents;
    };

    struct SceneLoadReport
    {
        size_t entityCount = 0;
        // 파일에 있지만 이 엔진에 등록되지 않은 컴포넌트 이름. DeserializeRegistry는 이런 컴포넌트를
        // 말없이 건너뛰므로, 여기서 따로 모아 경고한다.
        std::vector<std::string> unknownComponents;
    };

    // 레지스트리 → 씬 문서(JSON). report가 있으면 미등록 컴포넌트를 채운다.
    nlohmann::json BuildSceneDocument(ECSRegistry& registry, SceneSaveReport* report = nullptr);

    // 씬 문서의 헤더·구조를 검사한다. 레지스트리는 건드리지 않는다.
    std::expected<SceneLoadReport, EngineError> ValidateSceneDocument(const nlohmann::json& doc);

    // 씬 문서로 레지스트리를 교체한다. 먼저 빈 임시 레지스트리에 불러 보고, 성공했을 때만 대상
    // 레지스트리를 비우고 채운다 — 깨진 파일 때문에 편집 중인 월드를 잃지 않게 하기 위해서다.
    std::expected<SceneLoadReport, EngineError> ApplySceneDocument(ECSRegistry& registry, const nlohmann::json& doc);

    // 경로는 UTF-8 문자열이다(Python에서 넘어오는 한글 경로를 그대로 받기 위해).
    // 저장은 "<path>.tmp"에 먼저 쓰고 이름을 바꾼다 — 저장 도중 실패해도 기존 파일이 깨지지 않는다.
    std::expected<SceneSaveReport, EngineError> SaveSceneToFile(ECSRegistry& registry, const std::string& path);
    std::expected<SceneLoadReport, EngineError> LoadSceneFromFile(ECSRegistry& registry, const std::string& path);
}
