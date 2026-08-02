#include "Bindings.h"
#include "EngineException.h"
#include "../editor/EditorAPI.h"
#include "../core/CommandManager.h"
#include "../ecs/Entity.h"
#include "../core/Types.h"
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace Engine::Bindings
{

void RegisterEditorBindings(pybind11::module_& m)
{
    py::register_exception<EngineException>(m, "EngineError");

    py::class_<Editor::EditorAPI>(m, "EditorAPI")
        .def_static("get_instance", &Editor::EditorAPI::GetInstance, py::return_value_policy::reference)
        .def("set_registry", &Editor::EditorAPI::SetRegistry)

        // ---- Transform ----
        .def("move_entity", [](Editor::EditorAPI& self, Entity entity, const Vec3& position) {
            UnwrapOrThrow(self.MoveEntity(entity, position));
        })
        .def("rotate_entity", [](Editor::EditorAPI& self, Entity entity, const Quaternion& rotation) {
            UnwrapOrThrow(self.RotateEntity(entity, rotation));
        })
        .def("scale_entity", [](Editor::EditorAPI& self, Entity entity, const Vec3& scale) {
            UnwrapOrThrow(self.ScaleEntity(entity, scale));
        })

        // ---- Transaction lifecycle ----
        .def("begin_transaction", [](Editor::EditorAPI& self, const std::string& name) {
            UnwrapOrThrow(self.BeginTransaction(name));
        }, py::arg("name") = "Transaction")
        .def("commit_transaction", [](Editor::EditorAPI& self) {
            UnwrapOrThrow(self.CommitTransaction());
        })
        .def("cancel_transaction", [](Editor::EditorAPI& self) {
            UnwrapOrThrow(self.CancelTransaction());
        })
        .def("is_in_transaction", &Editor::EditorAPI::IsInTransaction)

        // ---- Entity lifetime ----
        .def("create_entity", [](Editor::EditorAPI& self, const std::string& name) {
            return UnwrapOrThrow(self.CreateEntity(name));
        }, py::arg("name") = "")
        .def("destroy_entity", [](Editor::EditorAPI& self, Entity entity) {
            UnwrapOrThrow(self.DestroyEntity(entity));
        })

        // ---- Component editing ----
        .def("add_component", [](Editor::EditorAPI& self, Entity entity, const std::string& componentType) {
            UnwrapOrThrow(self.AddComponent(entity, componentType));
        })
        .def("remove_component", [](Editor::EditorAPI& self, Entity entity, const std::string& componentType) {
            UnwrapOrThrow(self.RemoveComponent(entity, componentType));
        });

    py::class_<CommandManager>(m, "CommandManager")
        .def_static("get_instance", &CommandManager::GetInstance, py::return_value_policy::reference)
        .def("undo", [](CommandManager& self) {
            UnwrapOrThrow(self.Undo());
        })
        .def("redo", [](CommandManager& self) {
            UnwrapOrThrow(self.Redo());
        })
        .def("can_undo", &CommandManager::CanUndo)
        .def("can_redo", &CommandManager::CanRedo)
        .def("clear", &CommandManager::Clear)
        .def("begin_merge_session", &CommandManager::BeginMergeSession)
        .def("end_merge_session", &CommandManager::EndMergeSession)
        .def("is_in_merge_session", &CommandManager::IsInMergeSession);
}

} // namespace Engine::Bindings
