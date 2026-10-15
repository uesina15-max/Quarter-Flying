#pragma once

#include "../core/EngineError.h"
#include <nlohmann/json.hpp>
#include <glm/vec3.hpp>
#include <string>

namespace Engine
{
    // ========================================
    // 머티리얼 파일 (*.material.json) — docs/USABILITY_IMPROVEMENT_PLAN.md R-4 (결정 2026-10-15)
    // ========================================
    //
    //   { "format": "quarterflying.material", "version": 1,
    //     "baseColor": [1, 1, 1], "texture": "assets/textures/a.png", "roughness": 0.6, "metallic": 0.0 }
    //
    // 컴포넌트는 이 파일을 경로(RenderableComponent.materialPath, 프로젝트 기준)로 참조한다 - 에셋 참조는 핸들이 아니라
    // 경로로 통일한다(메시·텍스처와 같은 규칙, Build Game 수집과 상대 경로 저장이 그대로 적용된다).
    // 형식을 아는 곳은 이 파서 하나다. Build Game(Python)은 경로 키("texture")만 수집 표로 안다.
    //
    // 규칙: 빠진 키는 기본값(아래 MaterialData). 타입이 틀린 키는 에러(키 이름을 적는다) - 조용히 기본값으로 바꾸면
    // "머티리얼이 안 먹는다"의 원인을 찾기 어렵다. roughness·metallic은 [0, 1]을 벗어나면 에러.
    struct MaterialData
    {
        glm::vec3 baseColor{ 1.0f, 1.0f, 1.0f };
        std::string texture;          // 프로젝트 기준 경로. 비면 텍스처 없음
        float roughness = 0.6f;       // 0 = 매끈(좁고 강한 하이라이트), 1 = 거침(하이라이트 없음)
        float metallic = 0.0f;        // 1이면 반사광이 표면색을 띤다
    };

    inline constexpr const char* kMaterialFormatName = "quarterflying.material";
    inline constexpr int kMaterialFormatVersion = 1;

    Result<MaterialData> ParseMaterial(const nlohmann::json& json, const std::string& sourceLabel = "<json>");
    // path는 UTF-8(core/Utf8Path.h). 실패 코드: FileNotFound, ResourceLoadFailed(읽기·JSON 문법), InvalidParameter, NotSupported.
    Result<MaterialData> LoadMaterialFile(const std::string& utf8Path);
}
