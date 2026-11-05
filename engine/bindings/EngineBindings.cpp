#include "Bindings.h"
#include "../core/Engine.h"
#include "../core/Types.h"
#include "../ecs/World.h"
#include <pybind11/stl.h>
#include <stdexcept>

namespace Engine {
namespace Bindings {

namespace {
    // quarterflying.input.* 공통: 지금 프레임을 돌리는 엔진의 액션 맵. 프레임 밖(에디터 UI 코드 등)에서
    // 부르면 조용히 0을 돌려주지 않고 원인을 적은 예외를 낸다 - 0이 돌아오면 "키가 안 먹는다"로만 보인다.
    InputActions& CurrentInputActions(const char* api) {
        InputActions* actions = InputActions::Current();
        if (!actions) {
            throw std::runtime_error(std::string("quarterflying.input.") + api +
                ": no engine frame is running. Input can only be read from game scripts while the engine "
                "updates the world (ScriptComponent on_create/on_update, inside Engine.TickFrame)");
        }
        return *actions;
    }

    pybind11::dict LoadReportToDict(const InputActionsLoadReport& report) {
        pybind11::dict d;
        d["file_missing"] = report.fileMissing;
        d["action_count"] = report.actionCount;
        d["warnings"] = report.warnings;
        return d;
    }
}

void RegisterEngineBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // Input Enums
    // ============================================================
    py::enum_<InputEventType>(m, "InputEventType")
        .value("None",           InputEventType::None)
        .value("KeyDown",        InputEventType::KeyDown)
        .value("KeyUp",          InputEventType::KeyUp)
        .value("MouseMove",      InputEventType::MouseMove)
        .value("MouseButtonDown",InputEventType::MouseButtonDown)
        .value("MouseButtonUp",  InputEventType::MouseButtonUp)
        .value("MouseWheel",     InputEventType::MouseWheel)
        .value("WindowClose",    InputEventType::WindowClose)
        .value("WindowResize",   InputEventType::WindowResize)
        .value("FocusLost",      InputEventType::FocusLost)
        .export_values();

    // 예전에는 A/W/S/D/Escape만 바인딩돼 있었고, viewport.py는 모르는 키를 전부 A로 보냈다.
    // (A를 누르지 않았는데 게임에는 A가 눌린 것으로 전달됨.) 엔진 enum 전체를 바인딩한다.
    py::enum_<KeyCode>(m, "KeyCode")
        .value("Unknown", KeyCode::Unknown)
        .value("A", KeyCode::A)
        .value("B", KeyCode::B)
        .value("C", KeyCode::C)
        .value("D", KeyCode::D)
        .value("E", KeyCode::E)
        .value("F", KeyCode::F)
        .value("G", KeyCode::G)
        .value("H", KeyCode::H)
        .value("I", KeyCode::I)
        .value("J", KeyCode::J)
        .value("K", KeyCode::K)
        .value("L", KeyCode::L)
        .value("M", KeyCode::M)
        .value("N", KeyCode::N)
        .value("O", KeyCode::O)
        .value("P", KeyCode::P)
        .value("Q", KeyCode::Q)
        .value("R", KeyCode::R)
        .value("S", KeyCode::S)
        .value("T", KeyCode::T)
        .value("U", KeyCode::U)
        .value("V", KeyCode::V)
        .value("W", KeyCode::W)
        .value("X", KeyCode::X)
        .value("Y", KeyCode::Y)
        .value("Z", KeyCode::Z)
        .value("Num0", KeyCode::Num0)
        .value("Num1", KeyCode::Num1)
        .value("Num2", KeyCode::Num2)
        .value("Num3", KeyCode::Num3)
        .value("Num4", KeyCode::Num4)
        .value("Num5", KeyCode::Num5)
        .value("Num6", KeyCode::Num6)
        .value("Num7", KeyCode::Num7)
        .value("Num8", KeyCode::Num8)
        .value("Num9", KeyCode::Num9)
        .value("F1", KeyCode::F1)
        .value("F2", KeyCode::F2)
        .value("F3", KeyCode::F3)
        .value("F4", KeyCode::F4)
        .value("F5", KeyCode::F5)
        .value("F6", KeyCode::F6)
        .value("F7", KeyCode::F7)
        .value("F8", KeyCode::F8)
        .value("F9", KeyCode::F9)
        .value("F10", KeyCode::F10)
        .value("F11", KeyCode::F11)
        .value("F12", KeyCode::F12)
        .value("Escape", KeyCode::Escape)
        .value("Space", KeyCode::Space)
        .value("Enter", KeyCode::Enter)
        .value("Tab", KeyCode::Tab)
        .value("Backspace", KeyCode::Backspace)
        .value("Shift", KeyCode::Shift)
        .value("Control", KeyCode::Control)
        .value("Alt", KeyCode::Alt)
        .value("Left", KeyCode::Left)
        .value("Up", KeyCode::Up)
        .value("Right", KeyCode::Right)
        .value("Down", KeyCode::Down)
        .export_values();

    py::enum_<MouseButton>(m, "MouseButton")
        .value("Left",   MouseButton::Left)
        .value("Right",  MouseButton::Right)
        .value("Middle", MouseButton::Middle)
        .export_values();

    // ============================================================
    // Input & Config Structs
    // ============================================================
    py::class_<InputEvent>(m, "InputEvent")
        .def(py::init<>())
        .def_readwrite("type",            &InputEvent::type)
        .def_readwrite("keyCode",         &InputEvent::keyCode)
        .def_readwrite("mouseButton",     &InputEvent::mouseButton)
        .def_readwrite("mouseX",          &InputEvent::mouseX)
        .def_readwrite("mouseY",          &InputEvent::mouseY)
        .def_readwrite("mouseDeltaX",     &InputEvent::mouseDeltaX)
        .def_readwrite("mouseDeltaY",     &InputEvent::mouseDeltaY)
        .def_readwrite("mouseWheelDelta", &InputEvent::mouseWheelDelta)
        .def_readwrite("windowWidth",     &InputEvent::windowWidth)
        .def_readwrite("windowHeight",    &InputEvent::windowHeight);

    py::class_<EngineConfig>(m, "EngineConfig")
        .def(py::init<>())
        .def_readwrite("windowTitle",  &EngineConfig::windowTitle)
        .def_readwrite("windowWidth",  &EngineConfig::windowWidth)
        .def_readwrite("windowHeight", &EngineConfig::windowHeight)
        .def_readwrite("assetRoot",    &EngineConfig::assetRoot)
        .def_readwrite("showDebugGrid",   &EngineConfig::showDebugGrid)
        .def_readwrite("scriptHotReload", &EngineConfig::scriptHotReload)
        .def_readwrite("windowFullscreen", &EngineConfig::windowFullscreen)
        .def_readwrite("logFrameInterval", &EngineConfig::logFrameInterval);

    // ============================================================
    // Engine
    // ============================================================
    py::enum_<ViewCamera>(m, "ViewCamera")
        .value("Game", ViewCamera::Game)
        .value("Editor", ViewCamera::Editor);

    py::class_<Engine>(m, "Engine")
        .def(py::init<>())
        .def("Initialize", [](Engine& self, const EngineConfig& config) {
            auto result = self.Initialize(config);
            if (!result) {
                throw std::runtime_error("Engine Initialize failed [" + std::to_string(static_cast<int>(result.error().code)) + "]: " + result.error().message);
            }
        })
        .def("InitializeFromWindowHandle",
            [](Engine& self, size_t handle, const EngineConfig& config) {
                auto result = self.InitializeFromWindowHandle((void*)handle, config);
                if (!result) {
                    throw std::runtime_error("Engine InitializeFromWindowHandle failed [" + std::to_string(static_cast<int>(result.error().code)) + "]: " + result.error().message);
                }
            })
        .def("Shutdown",      &Engine::Shutdown)
        .def("TickFrame",     &Engine::TickFrame)
        .def("PushInputEvent",&Engine::PushInputEvent)
        .def("GetActiveWorld",&Engine::GetActiveWorld,
             py::return_value_policy::reference)
        .def("SetActiveWorld",&Engine::SetActiveWorld)
        .def("SetAssetRoot",  &Engine::SetAssetRoot, py::arg("asset_root"))
        .def("GetFrameCosts", &Engine::GetFrameCosts)
        .def("GetAssetRoot",  &Engine::GetAssetRoot)
        .def("CreateWorld",   &Engine::CreateWorld,
             py::return_value_policy::reference)
        // Motion Mixer 프리뷰 (Phase 4A, 착수 계약서 §C11)
        .def("GetMotionPreviewState", &Engine::GetMotionPreviewState,
             py::return_value_policy::reference)
        .def("SetRenderMode", &Engine::SetRenderMode)
        .def("GetRenderMode", &Engine::GetRenderMode)
        .def("SetViewCamera", &Engine::SetViewCamera)
        .def("GetViewCamera", &Engine::GetViewCamera)
        .def("SetEditorCameraView", &Engine::SetEditorCameraView,
             py::arg("eye_x"), py::arg("eye_y"), py::arg("eye_z"),
             py::arg("target_x"), py::arg("target_y"), py::arg("target_z"))
        // 뷰포트 조작(docs/VIEWPORT_MANIPULATION_PLAN.md). 좌표는 정규화 장치 좌표(-1..1, y 위가 +).
        // 피킹 규칙과 광선 계산은 C++(ecs/Picking.h) 한 곳 - 에디터는 결과만 받는다.
        .def("GetViewRay", [](const Engine& self, float ndcX, float ndcY) -> py::object {
            const auto ray = self.GetViewRay(ndcX, ndcY);
            if (!ray) return py::none();
            return py::make_tuple(py::make_tuple(ray->origin.x, ray->origin.y, ray->origin.z),
                                  py::make_tuple(ray->direction.x, ray->direction.y, ray->direction.z));
        }, py::arg("ndc_x"), py::arg("ndc_y"))
        .def("PickEntityAtScreen", [](Engine& self, float ndcX, float ndcY) -> py::object {
            const auto hit = self.PickEntityAtScreen(ndcX, ndcY);
            if (!hit) return py::none();
            py::dict result;
            result["entity"] = hit->entity;
            result["distance"] = hit->distance;
            result["point"] = py::make_tuple(hit->point.x, hit->point.y, hit->point.z);
            return result;
        }, py::arg("ndc_x"), py::arg("ndc_y"))
        .def("SetSelectedEntities", &Engine::SetSelectedEntities, py::arg("entities"))
        // 변환 기즈모. 손잡이·모드는 정수(GizmoHandle: 0 없음, 1 X, 2 Y, 3 Z, 4 전체 / GizmoMode: 0 이동, 1 회전, 2 크기).
        // 손잡이 형상·잡기 판정은 C++(renderer/TransformGizmo.h) 한 곳 - 에디터는 결과(원점·축·길이)로 끌기만 계산한다.
        .def("PickGizmo", [](const Engine& self, float ndcX, float ndcY) -> py::object {
            const auto hit = self.PickGizmoAt(ndcX, ndcY);
            if (!hit) return py::none();
            const glm::vec3 axis = hit->gizmo.AxisDirection(hit->handle);
            py::dict result;
            result["handle"] = static_cast<int>(hit->handle);
            result["mode"] = static_cast<int>(hit->gizmo.mode);
            result["entity"] = hit->entity;
            result["origin"] = py::make_tuple(hit->gizmo.origin.x, hit->gizmo.origin.y, hit->gizmo.origin.z);
            result["axis"] = py::make_tuple(axis.x, axis.y, axis.z);
            result["length"] = hit->gizmo.length;
            return result;
        }, py::arg("ndc_x"), py::arg("ndc_y"))
        .def("SetGizmoHighlight", [](Engine& self, int handle) {
            self.SetGizmoHighlight(handle >= 1 && handle <= 4 ? static_cast<GizmoHandle>(handle) : GizmoHandle::None);
        }, py::arg("handle"))
        .def("SetGizmoMode", [](Engine& self, int mode) {
            if (mode < 0 || mode > 2) throw std::invalid_argument("SetGizmoMode: 0(이동), 1(회전), 2(크기) 중 하나");
            self.SetGizmoMode(static_cast<GizmoMode>(mode));
        }, py::arg("mode"))
        .def("GetGizmoMode", [](const Engine& self) { return static_cast<int>(self.GetGizmoMode()); })
        // 입력 액션 맵 파일(docs/USABILITY_IMPROVEMENT_PLAN.md G0b). 형식은 C++(InputActions)만 안다 - 에디터는
        // 파일을 직접 파싱하지 않고 이 함수들을 부른다. 거절된 파일은 RuntimeError(기존 맵은 그대로).
        .def("LoadInputActions", [](Engine& self, const std::string& path) {
            auto report = self.GetInputActions().LoadActions(path);
            if (!report) throw std::runtime_error(report.error().message);
            return LoadReportToDict(*report);
        }, py::arg("path"))
        .def("SaveInputActions", [](Engine& self, const std::string& path) {
            auto result = self.GetInputActions().SaveActions(path);
            if (!result) throw std::runtime_error(result.error().message);
        }, py::arg("path"))
        .def("GetInputActionsJson", [](Engine& self) {
            return self.GetInputActions().ToJson().dump(2);
        })
        // 편집(Input Actions 패널, G2). def_json은 파일의 actions 항목 하나와 같은 모양의 JSON 문자열이다. 모양이 틀려
        // 건너뛴 바인딩은 경고 목록으로, 액션 자체가 거절되면(타입 없음 등) RuntimeError로 돌려준다. 파서는 C++ 하나뿐이다.
        .def("SetInputActionJson", [](Engine& self, const std::string& name, const std::string& defJson) {
            nlohmann::json def;
            try { def = nlohmann::json::parse(defJson); }
            catch (const nlohmann::json::parse_error& e) {
                throw std::runtime_error(std::string("action '") + name + "': JSON syntax error: " + e.what());
            }
            auto warnings = self.GetInputActions().SetActionJson(name, def);
            if (!warnings) throw std::runtime_error(warnings.error().message);
            return *warnings;
        }, py::arg("name"), py::arg("def_json"))
        .def("RemoveInputAction", [](Engine& self, const std::string& name) {
            return self.GetInputActions().RemoveAction(name);
        }, py::arg("name"));

    // 게임 스크립트용 입력 읽기. 바인딩은 C++ 규칙을 그대로 노출만 한다(합성·엣지 규칙은 InputActions.h).
    py::module_ input = m.def_submodule("input", "Input actions for game scripts (read during the engine frame)");
    input.def("get_axis", [](const std::string& name) { return CurrentInputActions("get_axis").GetAxis(name); },
              py::arg("name"));
    input.def("is_down", [](const std::string& name) { return CurrentInputActions("is_down").IsDown(name); },
              py::arg("name"));
    input.def("was_pressed", [](const std::string& name) { return CurrentInputActions("was_pressed").WasPressed(name); },
              py::arg("name"));
    input.def("was_released", [](const std::string& name) { return CurrentInputActions("was_released").WasReleased(name); },
              py::arg("name"));
    input.def("has_action", [](const std::string& name) { return CurrentInputActions("has_action").HasAction(name); },
              py::arg("name"));
    // 커서 잠금 요청(IPlatform 주석). 적용은 엔진이 프레임 끝에 하고, 창에 포커스가 있을 때만 걸린다.
    input.def("set_cursor_locked", [](bool locked) { CurrentInputActions("set_cursor_locked").RequestCursorLock(locked); },
              py::arg("locked"));
}

} // namespace Bindings
} // namespace Engine
