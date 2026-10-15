#pragma once

#include "InputState.h"
#include "../core/EngineError.h"
#include <nlohmann/json.hpp>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Engine
{
    // ========================================
    // 입력 액션 맵 (docs/USABILITY_IMPROVEMENT_PLAN.md G0a)
    // ========================================
    //
    // "Jump", "MoveX" 같은 이름을 키·마우스 입력에 묶는다. 게임 코드(C++ 시스템, Python 스크립트)는 키 대신
    // 액션 이름으로 읽고, 키 배치는 프로젝트의 input/actions.json에서 바꾼다.
    //
    // 이 클래스가 형식(스키마)과 합성 규칙을 아는 유일한 곳이다. 파일 읽기·쓰기(LoadActions/SaveActions)와
    // 에디터 편집(SetActionJson/Rebind)이 모두 같은 파서를 쓴다 - C++ 로더, Python API, 에디터 패널이
    // 각자 형식을 짐작해 구현하면 셋이 조금씩 어긋나기 때문이다.
    //
    // 파일 형식:
    //   { "format": "quarterflying.input-actions", "version": 1,
    //     "actions": {
    //       "Jump":  { "type": "button", "bindings": ["Key:Space", "Mouse:Right"] },
    //       "MoveX": { "type": "axis",   "bindings": [ {"negative": "Key:A", "positive": "Key:D"} ] },
    //       "LookX": { "type": "axis",   "bindings": [ {"source": "MouseDeltaX", "scale": 0.1} ] } } }
    //   입력 이름: "Key:<KeyCode 이름>"(A~Z, Num0~Num9, F1~F12, Escape, Space, Enter, Tab, Backspace, Shift,
    //   Control, Alt, Left, Up, Right, Down), "Mouse:Left|Right|Middle". 아날로그: MouseDeltaX, MouseDeltaY, Wheel.
    //
    // 합성 규칙:
    //   button - IsDown: 묶인 입력 중 하나라도 눌림. WasPressed: 직전 프레임에 안 눌렸고 이번 프레임에 묶인 입력
    //            중 하나라도 눌린 엣지가 있음. WasReleased: 직전 프레임에 눌렸고 지금은 아님. 한 프레임 안에 눌렀다
    //            떼면 둘 다 true. 이미 눌린 상태에서 다른 키를 더 눌러도 WasPressed는 다시 나지 않는다.
    //   axis   - 디지털 쌍은 각각 -1/0/+1, 그 합을 [-1, 1]로 자른다. 아날로그는 값 x scale, 자르지 않는다
    //            (프레임 이동량이라 상한이 없다). GetAxis = 잘린 디지털 합 + 아날로그 합.
    //   타입이 다른 호출(axis에 WasPressed 등)과 없는 이름은 0/false를 돌려주고 이름당 한 번 경고한다.
    //
    // 프레임 순서(Engine::TickFrame): InputState::BeginFrame -> 이벤트 Apply(포커스 상실은 ResetHeldState) ->
    // Update(프레임당 한 번, 스냅샷) -> World 시스템·스크립트가 읽기. 읽기는 상태를 바꾸지 않으므로 같은 프레임의
    // 모든 읽는 쪽이 같은 값을 본다.

    enum class InputActionType : uint8_t { Button, Axis };

    // 하나의 디지털 입력("Key:W", "Mouse:Left")
    struct InputSource
    {
        enum class Kind : uint8_t { Key, MouseButton };
        Kind kind = Kind::Key;
        KeyCode key = KeyCode::Unknown;
        MouseButton button = MouseButton::Left;
    };

    enum class AnalogSource : uint8_t { MouseDeltaX, MouseDeltaY, Wheel };

    struct InputBinding
    {
        enum class Kind : uint8_t { Digital, Pair, Analog };
        Kind kind = Kind::Digital;
        InputSource input;                  // Digital (button 액션)
        InputSource negative, positive;     // Pair (axis 액션)
        AnalogSource analog = AnalogSource::MouseDeltaX;   // Analog (axis 액션)
        float scale = 1.0f;
    };

    struct InputActionDefinition
    {
        InputActionType type = InputActionType::Button;
        std::vector<InputBinding> bindings;
    };

    // 불러오기 결과. 거절되지 않은 문제(모르는 키 이름, 모양이 틀린 바인딩 등)는 warnings에 담고 그 부분만 건너뛴다.
    struct InputActionsLoadReport
    {
        bool fileMissing = false;
        size_t actionCount = 0;
        std::vector<std::string> warnings;
    };

    class InputActions
    {
    public:
        static constexpr const char* kFormat = "quarterflying.input-actions";
        static constexpr int kVersion = 1;

        // 파일 읽기. 파일이 없으면 빈 맵으로 바꾸고 fileMissing=true(경고 한 번).
        // 형식이 다르거나 버전이 더 높거나 JSON이 깨졌으면 에러를 돌려주고 **지금 맵을 그대로 둔다**.
        Result<InputActionsLoadReport> LoadActions(const std::string& utf8Path);
        Result<void> SaveActions(const std::string& utf8Path) const;

        // 파일 없이 JSON으로 읽기/쓰기(LoadActions/SaveActions가 쓰는 같은 파서·직렬화기).
        Result<InputActionsLoadReport> LoadFromJson(const nlohmann::json& root, const std::string& sourceLabel = "<json>");
        nlohmann::json ToJson() const;

        // 편집(에디터 패널). def는 파일의 actions 항목 하나와 같은 모양이다. 경고 목록을 돌려준다.
        Result<std::vector<std::string>> SetActionJson(const std::string& name, const nlohmann::json& def);
        // 바인딩만 바꾼다(타입은 그대로). bindings는 파일의 "bindings" 배열과 같은 모양이다. 메모리만 바꾼다.
        Result<std::vector<std::string>> Rebind(const std::string& name, const nlohmann::json& bindings);
        bool RemoveAction(const std::string& name);
        void Clear();

        std::vector<std::string> GetActionNames() const;
        bool HasAction(const std::string& name) const;

        // 프레임당 한 번(Engine::TickFrame, 이벤트 적용 뒤·World 업데이트 전).
        void Update(const InputState& input);
        // 포커스 상실(Engine::HandleFocusLost). 직전 상태를 "안 눌림"으로 맞춰서 다음 Update가 WasReleased를
        // 만들지 않게 한다 - 창 전환으로 "뗀 순간" 동작(차지 공격 발사 등)이 발동하면 안 된다.
        void ResetHeldState();

        float GetAxis(const std::string& name) const;
        bool IsDown(const std::string& name) const;
        bool WasPressed(const std::string& name) const;
        bool WasReleased(const std::string& name) const;

        // 지금 프레임을 돌리는 엔진의 액션 맵(Python 스크립트용 quarterflying.input이 읽는다). Engine::TickFrame이
        // World 업데이트 동안에만 ScopedCurrent로 설정하고, 그 밖에서는 nullptr이다. 스크립트는 Entity만 받으므로
        // 어느 Engine의 입력인지 이것으로 정한다(에디터 프로세스에는 Engine이 여러 개 있을 수 있다).
        // 값은 .cpp의 thread_local에 있다 - 헤더의 inline 변수로 두면 DLL(엔진)과 .pyd(바인딩)가 각자 사본을
        // 가져서 바인딩 쪽에서는 항상 nullptr로 보인다(core/logging/Logger의 같은 종류 버그, GAME_BUILD_PLAN §9.3).
        static InputActions* Current();

        class ScopedCurrent
        {
        public:
            explicit ScopedCurrent(InputActions* actions);
            ~ScopedCurrent();
            ScopedCurrent(const ScopedCurrent&) = delete;
            ScopedCurrent& operator=(const ScopedCurrent&) = delete;
        private:
            InputActions* previous_;
        };

        // 게임 스크립트의 커서 잠금 요청(quarterflying.input.set_cursor_locked). 여기에는 요청만 쌓고, Engine::TickFrame이
        // World 업데이트 뒤에 꺼내(TakeCursorLockRequest) 플랫폼에 전한다(IPlatform::SetCursorLockRequested).
        void RequestCursorLock(bool locked) { cursorLockRequest_ = locked; }
        std::optional<bool> TakeCursorLockRequest()
        {
            auto request = cursorLockRequest_;
            cursorLockRequest_.reset();
            return request;
        }

    private:
        struct ActionState
        {
            InputActionDefinition def;
            bool down = false;       // 이번 프레임 끝(Update 시점)
            bool pressed = false;
            bool released = false;
            float axis = 0.0f;
        };

        const ActionState* Find(const std::string& name, InputActionType expected, const char* api) const;
        void WarnOnce(const std::string& key, const std::string& message) const;

        std::map<std::string, ActionState> actions_;
        std::optional<bool> cursorLockRequest_;
        mutable std::set<std::string> warned_;   // 같은 경고를 매 프레임 찍지 않기 위한 기록
    };
}
