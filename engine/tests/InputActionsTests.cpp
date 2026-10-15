// 입력 액션 맵(input/InputActions) 유닛 테스트. docs/USABILITY_IMPROVEMENT_PLAN.md G0a의 검증 목록:
// 스키마 파싱, 모르는 키, 타입 불일치, 축 합성·자르기, 같은 프레임 눌렀다 떼기, 두 키 중복 눌림의 엣지,
// Pause 프레임, Load->Save->Load 왕복, 한글 프로젝트 경로, 포커스 상실 뒤 WasReleased 없음.

#include <gtest/gtest.h>
#include "../input/InputActions.h"
#include "../core/Engine.h"
#include "../core/Utf8Path.h"
#include <filesystem>
#include <fstream>

using namespace Engine;

namespace
{
    InputEvent KeyEv(InputEventType type, KeyCode key)
    {
        InputEvent e;
        e.type = type;
        e.keyCode = key;
        return e;
    }

    InputEvent MouseMove(int x, int y)
    {
        InputEvent e;
        e.type = InputEventType::MouseMove;
        e.mouseX = x;
        e.mouseY = y;
        return e;
    }

    InputEvent Wheel(float delta)
    {
        InputEvent e;
        e.type = InputEventType::MouseWheel;
        e.mouseWheelDelta = delta;
        return e;
    }

    // 한 프레임: BeginFrame -> 이벤트 -> Update (Engine::TickFrame과 같은 순서)
    void Frame(InputState& input, InputActions& actions, std::initializer_list<InputEvent> events = {})
    {
        input.BeginFrame();
        for (const auto& e : events) input.Apply(e);
        actions.Update(input);
    }

    nlohmann::json SampleJson()
    {
        return nlohmann::json::parse(R"({
            "format": "quarterflying.input-actions",
            "version": 1,
            "actions": {
                "Jump":  { "type": "button", "bindings": ["Key:Space", "Key:Up", "Mouse:Right"] },
                "MoveX": { "type": "axis",   "bindings": [ {"negative": "Key:A", "positive": "Key:D"},
                                                           {"negative": "Key:Left", "positive": "Key:Right"} ] },
                "LookX": { "type": "axis",   "bindings": [ {"source": "MouseDeltaX", "scale": 0.5} ] },
                "Zoom":  { "type": "axis",   "bindings": [ {"source": "Wheel"} ] }
            }
        })");
    }

    InputActions Loaded()
    {
        InputActions actions;
        auto report = actions.LoadFromJson(SampleJson());
        EXPECT_TRUE(report.has_value());
        return actions;
    }

    std::filesystem::path TempPath(const std::string& utf8Name)
    {
        return std::filesystem::temp_directory_path() / "qf_input_actions_tests" / PathFromUtf8(utf8Name);
    }
}

// ── 스키마 ───────────────────────────────────────────────────────────────────

TEST(InputActionsTest, LoadsSchemaExample)
{
    InputActions actions;
    auto report = actions.LoadFromJson(SampleJson());
    ASSERT_TRUE(report.has_value()) << report.error().message;
    EXPECT_EQ(report->actionCount, 4u);
    EXPECT_TRUE(report->warnings.empty());
    EXPECT_TRUE(actions.HasAction("Jump"));
    EXPECT_TRUE(actions.HasAction("Zoom"));
}

TEST(InputActionsTest, UnknownInputNames_SkipOnlyThatBinding_WithWarning)
{
    InputActions actions;
    auto json = SampleJson();
    json["actions"]["Jump"]["bindings"] = { "Key:Space", "Key:Spacebar", "Pad:A" };
    auto report = actions.LoadFromJson(json);
    ASSERT_TRUE(report.has_value());
    ASSERT_EQ(report->warnings.size(), 2u);
    EXPECT_NE(report->warnings[0].find("Key:Spacebar"), std::string::npos) << report->warnings[0];

    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    EXPECT_TRUE(actions.WasPressed("Jump"));   // 남은 바인딩은 동작한다
}

TEST(InputActionsTest, MalformedActions_AreSkippedWithWarnings)
{
    InputActions actions;
    auto json = SampleJson();
    json["actions"]["Bad1"] = { { "type", "trigger" }, { "bindings", nlohmann::json::array() } };
    json["actions"]["Bad2"] = { { "type", "button" } };
    json["actions"]["MoveX"]["bindings"].push_back("Key:W");   // axis에 문자열 바인딩
    auto report = actions.LoadFromJson(json);
    ASSERT_TRUE(report.has_value());
    EXPECT_FALSE(actions.HasAction("Bad1"));
    EXPECT_FALSE(actions.HasAction("Bad2"));
    EXPECT_EQ(report->warnings.size(), 3u);
}

TEST(InputActionsTest, WrongFormatOrNewerVersion_IsRejected_AndKeepsCurrentMap)
{
    InputActions actions = Loaded();

    auto wrongFormat = SampleJson();
    wrongFormat["format"] = "something-else";
    EXPECT_FALSE(actions.LoadFromJson(wrongFormat).has_value());

    auto newer = SampleJson();
    newer["version"] = InputActions::kVersion + 1;
    auto result = actions.LoadFromJson(newer);
    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error().code, EngineErrorCode::NotSupported);

    EXPECT_TRUE(actions.HasAction("Jump"));   // 거절해도 기존 맵은 그대로
}

// ── 합성 규칙 ────────────────────────────────────────────────────────────────

TEST(InputActionsTest, Axis_DigitalPairsSumAndClamp)
{
    InputActions actions = Loaded();
    InputState input;

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::D) });
    EXPECT_FLOAT_EQ(actions.GetAxis("MoveX"), 1.0f);

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Right) });   // 두 쌍이 같은 방향
    EXPECT_FLOAT_EQ(actions.GetAxis("MoveX"), 1.0f);                             // 2가 아니라 1로 잘림

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::A) });       // D + Right + A
    EXPECT_FLOAT_EQ(actions.GetAxis("MoveX"), 1.0f);                             // 1 + 1 - 1 = 1

    Frame(input, actions, { KeyEv(InputEventType::KeyUp, KeyCode::D), KeyEv(InputEventType::KeyUp, KeyCode::Right) });
    EXPECT_FLOAT_EQ(actions.GetAxis("MoveX"), -1.0f);
}

TEST(InputActionsTest, Axis_AnalogIsScaledAndNotClamped)
{
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { MouseMove(0, 0) });
    Frame(input, actions, { MouseMove(10, 0), Wheel(3.0f) });
    EXPECT_FLOAT_EQ(actions.GetAxis("LookX"), 5.0f);   // 10 x 0.5, [-1,1]로 자르지 않음
    EXPECT_FLOAT_EQ(actions.GetAxis("Zoom"), 3.0f);    // scale 생략 = 1

    Frame(input, actions);                              // 이동량은 프레임마다 0으로
    EXPECT_FLOAT_EQ(actions.GetAxis("LookX"), 0.0f);
}

TEST(InputActionsTest, Button_PressHoldRelease)
{
    InputActions actions = Loaded();
    InputState input;

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    EXPECT_TRUE(actions.IsDown("Jump"));
    EXPECT_TRUE(actions.WasPressed("Jump"));
    EXPECT_FALSE(actions.WasReleased("Jump"));

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });   // Win32 자동 반복
    EXPECT_TRUE(actions.IsDown("Jump"));
    EXPECT_FALSE(actions.WasPressed("Jump"));

    Frame(input, actions, { KeyEv(InputEventType::KeyUp, KeyCode::Space) });
    EXPECT_FALSE(actions.IsDown("Jump"));
    EXPECT_TRUE(actions.WasReleased("Jump"));

    Frame(input, actions);
    EXPECT_FALSE(actions.WasReleased("Jump"));
}

TEST(InputActionsTest, Button_PressAndReleaseInSameFrame_ReportsBothEdges)
{
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space), KeyEv(InputEventType::KeyUp, KeyCode::Space) });
    EXPECT_TRUE(actions.WasPressed("Jump"));
    EXPECT_TRUE(actions.WasReleased("Jump"));
    EXPECT_FALSE(actions.IsDown("Jump"));
}

TEST(InputActionsTest, Button_SecondBindingWhileHeld_DoesNotPressAgain)
{
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    ASSERT_TRUE(actions.WasPressed("Jump"));

    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Up) });       // 같은 액션의 다른 키
    EXPECT_FALSE(actions.WasPressed("Jump"));

    Frame(input, actions, { KeyEv(InputEventType::KeyUp, KeyCode::Space) });      // 하나 떼도 다른 키가 눌려 있음
    EXPECT_TRUE(actions.IsDown("Jump"));
    EXPECT_FALSE(actions.WasReleased("Jump"));

    Frame(input, actions, { KeyEv(InputEventType::KeyUp, KeyCode::Up) });
    EXPECT_TRUE(actions.WasReleased("Jump"));
}

TEST(InputActionsTest, MouseButtonBinding_Works)
{
    InputActions actions = Loaded();
    InputState input;
    InputEvent down;
    down.type = InputEventType::MouseButtonDown;
    down.mouseButton = MouseButton::Right;
    Frame(input, actions, { down });
    EXPECT_TRUE(actions.WasPressed("Jump"));
}

TEST(InputActionsTest, ReadsAreStableWithinAFrame)
{
    // 여러 시스템·스크립트가 같은 프레임에 읽어도 모두 같은 값을 본다(읽기가 엣지를 소비하지 않는다).
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    EXPECT_TRUE(actions.WasPressed("Jump"));
    EXPECT_TRUE(actions.WasPressed("Jump"));
    EXPECT_TRUE(actions.WasPressed("Jump"));
}

TEST(InputActionsTest, PressDuringPausedFrames_IsNotSeenAsPressAfterResume)
{
    // Pause 중에는 World 시스템·스크립트가 돌지 않는다. 그동안 프레임은 계속 지나가므로 그 엣지는 아무도
    // 보지 않고 사라진다(의도된 동작). 키를 계속 누르고 있었다면 계속한 뒤에는 IsDown만 true다.
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });   // 일시정지 중 프레임(읽는 쪽 없음)
    Frame(input, actions);                                                       // 계속 후 첫 프레임
    EXPECT_TRUE(actions.IsDown("Jump"));
    EXPECT_FALSE(actions.WasPressed("Jump"));
}

TEST(InputActionsTest, TypeMismatchAndMissingAction_ReturnZero)
{
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space), KeyEv(InputEventType::KeyDown, KeyCode::D) });
    EXPECT_FLOAT_EQ(actions.GetAxis("Jump"), 0.0f);     // button에 GetAxis
    EXPECT_FALSE(actions.IsDown("MoveX"));              // axis에 IsDown
    EXPECT_FALSE(actions.WasPressed("NoSuchAction"));
    EXPECT_FLOAT_EQ(actions.GetAxis("NoSuchAction"), 0.0f);
}

TEST(InputActionsTest, ResetHeldState_DoesNotProduceWasReleased)
{
    InputActions actions = Loaded();
    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    ASSERT_TRUE(actions.IsDown("Jump"));

    // 포커스 상실: Engine::HandleFocusLost와 같은 순서
    input.BeginFrame();
    input.ReleaseAll();
    actions.ResetHeldState();
    actions.Update(input);
    EXPECT_FALSE(actions.IsDown("Jump"));
    EXPECT_FALSE(actions.WasReleased("Jump"));
}

TEST(InputActionsTest, EngineFocusLost_ResetsActionsToo)
{
    class Engine engine;
    ASSERT_TRUE(engine.GetInputActions().LoadFromJson(SampleJson()).has_value());
    engine.DispatchInputEventForTesting(KeyEv(InputEventType::KeyDown, KeyCode::Space));
    engine.GetInputActions().Update(engine.GetInputStateForTesting());
    ASSERT_TRUE(engine.GetInputActions().IsDown("Jump"));

    InputEvent focusLost;
    focusLost.type = InputEventType::FocusLost;
    engine.DispatchInputEventForTesting(focusLost);
    engine.GetInputActions().Update(engine.GetInputStateForTesting());
    EXPECT_FALSE(engine.GetInputActions().IsDown("Jump"));
    EXPECT_FALSE(engine.GetInputActions().WasReleased("Jump"));
}

// ── 편집 ─────────────────────────────────────────────────────────────────────

TEST(InputActionsTest, RebindReplacesBindings_UsingSameParser)
{
    InputActions actions = Loaded();
    auto warnings = actions.Rebind("Jump", nlohmann::json::array({ "Key:J", "Key:Nope" }));
    ASSERT_TRUE(warnings.has_value());
    EXPECT_EQ(warnings->size(), 1u);

    InputState input;
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::Space) });
    EXPECT_FALSE(actions.IsDown("Jump"));
    Frame(input, actions, { KeyEv(InputEventType::KeyDown, KeyCode::J) });
    EXPECT_TRUE(actions.WasPressed("Jump"));

    EXPECT_FALSE(actions.Rebind("NoSuchAction", nlohmann::json::array()).has_value());
}

TEST(InputActionsTest, SetActionJsonAndRemove)
{
    InputActions actions;
    ASSERT_TRUE(actions.SetActionJson("Fire", { { "type", "button" }, { "bindings", { "Mouse:Left" } } }).has_value());
    EXPECT_TRUE(actions.HasAction("Fire"));
    EXPECT_FALSE(actions.SetActionJson("Bad", { { "type", "nope" } }).has_value());
    EXPECT_FALSE(actions.HasAction("Bad"));
    EXPECT_TRUE(actions.RemoveAction("Fire"));
    EXPECT_FALSE(actions.HasAction("Fire"));
}

// ── 파일 ─────────────────────────────────────────────────────────────────────

TEST(InputActionsTest, SaveThenLoad_RoundTripsInKoreanPath)
{
    const auto path = TempPath("한글 프로젝트/input/actions.json");
    std::error_code ec;
    std::filesystem::remove_all(path.parent_path().parent_path(), ec);

    InputActions original = Loaded();
    auto saved = original.SaveActions(Utf8FromPath(path));
    ASSERT_TRUE(saved.has_value()) << saved.error().message;
    ASSERT_TRUE(std::filesystem::is_regular_file(path));
    EXPECT_FALSE(std::filesystem::exists(std::filesystem::path(path).concat(".tmp")));

    InputActions reloaded;
    auto report = reloaded.LoadActions(Utf8FromPath(path));
    ASSERT_TRUE(report.has_value()) << report.error().message;
    EXPECT_FALSE(report->fileMissing);
    EXPECT_EQ(reloaded.ToJson(), original.ToJson());

    // 다시 저장해도 같은 내용(직렬화가 안정적이다)
    ASSERT_TRUE(reloaded.SaveActions(Utf8FromPath(path)).has_value());
    InputActions third;
    ASSERT_TRUE(third.LoadActions(Utf8FromPath(path)).has_value());
    EXPECT_EQ(third.ToJson(), original.ToJson());
}

TEST(InputActionsTest, MissingFile_GivesEmptyMapAndFlag)
{
    InputActions actions = Loaded();
    auto report = actions.LoadActions(Utf8FromPath(TempPath("없는 폴더/actions.json")));
    ASSERT_TRUE(report.has_value());
    EXPECT_TRUE(report->fileMissing);
    EXPECT_TRUE(actions.GetActionNames().empty());
}

TEST(InputActionsTest, BrokenJsonFile_IsRejected_AndKeepsCurrentMap)
{
    const auto path = TempPath("깨진/actions.json");
    std::filesystem::create_directories(path.parent_path());
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out << "{ \"format\": ";   // 잘린 JSON
    }
    InputActions actions = Loaded();
    auto result = actions.LoadActions(Utf8FromPath(path));
    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().message.find("JSON syntax error"), std::string::npos);
    EXPECT_TRUE(actions.HasAction("Jump"));
}

TEST(InputActionsTest, PressThenFocusLostInSameFrame_ProducesNoEdges)
{
    // 한 프레임의 이벤트 큐에 KeyDown과 FocusLost가 함께 들어온 경우(누르자마자 Alt+Tab).
    InputActions actions = Loaded();
    InputState input;
    input.BeginFrame();
    input.Apply(KeyEv(InputEventType::KeyDown, KeyCode::Space));
    input.ReleaseAll();
    actions.ResetHeldState();
    actions.Update(input);
    EXPECT_FALSE(actions.IsDown("Jump"));
    EXPECT_FALSE(actions.WasPressed("Jump"));
    EXPECT_FALSE(actions.WasReleased("Jump"));
}

TEST(InputActionsTest, CursorLockRequest_IsTakenOnce)
{
    // 스크립트 요청(set_cursor_locked)은 Engine::TickFrame이 프레임 끝에 한 번 꺼내 플랫폼에 전한다.
    InputActions actions;
    EXPECT_FALSE(actions.TakeCursorLockRequest().has_value());
    actions.RequestCursorLock(true);
    actions.RequestCursorLock(false);   // 같은 프레임에 여러 번 부르면 마지막 요청
    auto request = actions.TakeCursorLockRequest();
    ASSERT_TRUE(request.has_value());
    EXPECT_FALSE(*request);
    EXPECT_FALSE(actions.TakeCursorLockRequest().has_value());
}
