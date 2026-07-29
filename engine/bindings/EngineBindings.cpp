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

    py::enum_<KeyCode>(m, "KeyCode")
        .value("A",      KeyCode::A)
        .value("W",      KeyCode::W)
        .value("S",      KeyCode::S)
        .value("D",      KeyCode::D)
        .value("Escape", KeyCode::Escape)
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
        .def_readwrite("windowHeight", &EngineConfig::windowHeight);

    // ============================================================
    // Engine
    // ============================================================
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
        .def("CreateWorld",   &Engine::CreateWorld,
             py::return_value_policy::reference);
}

} // namespace Bindings
} // namespace Engine
