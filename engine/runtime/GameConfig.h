#pragma once

#include "../core/EngineError.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <expected>
#include <string>

namespace Engine
{
    // ========================================
    // game.json — 게임 실행 파일(Player)의 실행 설정 (docs/GAME_BUILD_PLAN.md §3.4)
    // ========================================
    //
    // Build Game이 project.json의 빌드 설정에서 생성한다(D10). 규칙:
    //   - 빠진 키는 기본값을 쓴다(startScene만 필수).
    //   - 타입이 틀린 키는 에러다. 조용히 기본값으로 바꾸면 "설정이 안 먹는" 원인을 찾기 어렵다.
    //   - 모르는 키는 무시한다(뒤 버전에서 추가된 설정을 앞 버전 Player가 읽어도 실행은 되게).
    //
    // {
    //   "version": 1,
    //   "title": "My Game",
    //   "window": { "width": 1280, "height": 720, "fullscreen": false, "cursorLock": false },
    //   "startScene": "scenes/Main.scene.json",
    //   "assetRoot": "./",
    //   "inputActions": "assets/input/actions.json",
    //   "engine": { "numWorkerThreads": 0, "logFrameInterval": 0 }
    // }
    // 경로(startScene, assetRoot, inputActions)는 game.json이 있는 폴더 기준이다.
    // inputActions가 없으면 액션 맵 없이 시작한다(스크립트의 quarterflying.input은 0/false를 읽는다 + 경고).

    inline constexpr int kGameConfigVersion = 1;

    struct GameConfig
    {
        int version = kGameConfigVersion;
        std::string title = "Quarter Flying Game";
        uint32_t windowWidth = 1280;
        uint32_t windowHeight = 720;
        bool fullscreen = false;
        bool cursorLock = false;           // 시작할 때 커서 잠금을 요청할지(USABILITY_IMPROVEMENT_PLAN G3)
        std::string startScene;
        std::string assetRoot = "./";
        std::string inputActions;          // 비면 액션 맵을 불러오지 않는다
        uint32_t numWorkerThreads = 0;
        uint32_t logFrameInterval = 0;
    };

    // 실패 코드: InvalidParameter(키 누락·타입 오류·값 범위), NotSupported(더 새 버전)
    std::expected<GameConfig, EngineError> ParseGameConfig(const nlohmann::json& json);

    // 실패 코드: FileNotFound, ResourceLoadFailed(읽기·JSON 문법), 그 밖은 ParseGameConfig와 같음.
    // path는 UTF-8.
    std::expected<GameConfig, EngineError> LoadGameConfig(const std::string& path);
}
