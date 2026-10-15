#include <gtest/gtest.h>
#include "../runtime/GameConfig.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

// docs/GAME_BUILD_PLAN.md Phase 2 — game.json 파싱(정상 / 키 누락 / 타입 오류).

TEST(GameConfigTest, FullConfig_ParsesEveryField)
{
    auto cfg = ParseGameConfig(nlohmann::json::parse(R"({
        "version": 1, "title": "My Game",
        "window": { "width": 800, "height": 450, "fullscreen": true },
        "startScene": "scenes/Main.scene.json", "assetRoot": "./data",
        "engine": { "numWorkerThreads": 2, "logFrameInterval": 0 } })"));
    ASSERT_TRUE(cfg.has_value()) << cfg.error().message;
    EXPECT_EQ(cfg->title, "My Game");
    EXPECT_EQ(cfg->windowWidth, 800u);
    EXPECT_EQ(cfg->windowHeight, 450u);
    EXPECT_TRUE(cfg->fullscreen);
    EXPECT_EQ(cfg->startScene, "scenes/Main.scene.json");
    EXPECT_EQ(cfg->assetRoot, "./data");
    EXPECT_EQ(cfg->numWorkerThreads, 2u);
}

TEST(GameConfigTest, MissingOptionalKeys_UseDefaults_UnknownKeysIgnored)
{
    auto cfg = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json", "futureKey": 1 })"));
    ASSERT_TRUE(cfg.has_value()) << cfg.error().message;
    GameConfig defaults;
    EXPECT_EQ(cfg->windowWidth, defaults.windowWidth);
    EXPECT_EQ(cfg->assetRoot, "./");
    EXPECT_FALSE(cfg->fullscreen);
}

TEST(GameConfigTest, MissingStartScene_IsAnError)
{
    auto cfg = ParseGameConfig(nlohmann::json::parse(R"({ "title": "x" })"));
    ASSERT_FALSE(cfg.has_value());
    EXPECT_EQ(cfg.error().code, EngineErrorCode::InvalidParameter);
    EXPECT_NE(cfg.error().message.find("startScene"), std::string::npos);
}

TEST(GameConfigTest, WrongType_IsAnError_NamingTheKey)
{
    // 조용히 기본값으로 바꾸면 "설정이 안 먹는" 원인을 찾을 수 없다.
    auto cfg = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s", "window": { "width": "wide" } })"));
    ASSERT_FALSE(cfg.has_value());
    EXPECT_EQ(cfg.error().code, EngineErrorCode::InvalidParameter);
    EXPECT_NE(cfg.error().message.find("window.width"), std::string::npos);

    auto zero = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s", "window": { "height": 0 } })"));
    ASSERT_FALSE(zero.has_value());
    EXPECT_NE(zero.error().message.find("window.height"), std::string::npos);

    auto flag = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s", "window": { "fullscreen": 1 } })"));
    ASSERT_FALSE(flag.has_value());
    EXPECT_NE(flag.error().message.find("window.fullscreen"), std::string::npos);
}

TEST(GameConfigTest, NewerVersion_IsNotSupported)
{
    auto cfg = ParseGameConfig(nlohmann::json::parse(R"({ "version": 99, "startScene": "s" })"));
    ASSERT_FALSE(cfg.has_value());
    EXPECT_EQ(cfg.error().code, EngineErrorCode::NotSupported);
}

TEST(GameConfigTest, LoadFromFile_MissingAndBrokenFiles_HaveDistinctErrors)
{
    const auto dir = std::filesystem::temp_directory_path() / "qf_gameconfig_tests";
    std::filesystem::create_directories(dir);

    auto missing = LoadGameConfig((dir / "nope.json").string());
    ASSERT_FALSE(missing.has_value());
    EXPECT_EQ(missing.error().code, EngineErrorCode::FileNotFound);

    const auto broken = dir / "broken.json";
    { std::ofstream(broken) << "{ \"startScene\": "; }
    auto parsed = LoadGameConfig(broken.string());
    ASSERT_FALSE(parsed.has_value());
    EXPECT_EQ(parsed.error().code, EngineErrorCode::ResourceLoadFailed);
}

// docs/USABILITY_IMPROVEMENT_PLAN.md G0b — 입력 액션 맵 경로. 옛 game.json(키 없음)은 빈 값 = 불러오지 않음.
TEST(GameConfigTest, InputActions_OptionalPath)
{
    auto withKey = ParseGameConfig(nlohmann::json::parse(
        R"({ "startScene": "s.scene.json", "inputActions": "assets/input/actions.json" })"));
    ASSERT_TRUE(withKey.has_value()) << withKey.error().message;
    EXPECT_EQ(withKey->inputActions, "assets/input/actions.json");

    auto without = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json" })"));
    ASSERT_TRUE(without.has_value());
    EXPECT_TRUE(without->inputActions.empty());

    auto wrongType = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json", "inputActions": 3 })"));
    ASSERT_FALSE(wrongType.has_value());
    EXPECT_NE(wrongType.error().message.find("inputActions"), std::string::npos);
}

// docs/USABILITY_IMPROVEMENT_PLAN.md G3 — 커서 잠금 옵션(기본 꺼짐)
TEST(GameConfigTest, CursorLock_DefaultsOff_AndParses)
{
    auto off = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json" })"));
    ASSERT_TRUE(off.has_value());
    EXPECT_FALSE(off->cursorLock);
    auto on = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json", "window": { "cursorLock": true } })"));
    ASSERT_TRUE(on.has_value()) << on.error().message;
    EXPECT_TRUE(on->cursorLock);
    auto bad = ParseGameConfig(nlohmann::json::parse(R"({ "startScene": "s.scene.json", "window": { "cursorLock": 1 } })"));
    ASSERT_FALSE(bad.has_value());
    EXPECT_NE(bad.error().message.find("window.cursorLock"), std::string::npos);
}
