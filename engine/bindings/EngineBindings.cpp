#include "Bindings.h"
#include "../core/Engine.h"
#include "../core/Types.h"
#include "../ecs/World.h"
#include <stdexcept>

namespace Engine {
namespace Bindings {

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
        .def_readwrite("assetRoot",    &EngineConfig::assetRoot);

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
             py::arg("target_x"), py::arg("target_y"), py::arg("target_z"));
}

} // namespace Bindings
} // namespace Engine
