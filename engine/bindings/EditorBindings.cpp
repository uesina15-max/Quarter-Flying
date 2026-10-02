#include "Bindings.h"
#include "EngineException.h"
#include "../editor/EditorAPI.h"
#include "../core/CommandManager.h"
#include "../ecs/Entity.h"
#include "../ecs/ECSRegistry.h"  // EditorAPI.h only forward-declares ECSRegistry; pybind11's
                                  // __is_base_of type-trait checks need the complete type here.
#include "../core/Types.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>
#include <filesystem>
#include <optional>

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
        // parent=None이면 루트로 만든다.
        .def("set_parent", [](Editor::EditorAPI& self, Entity child, std::optional<Entity> parent) {
            UnwrapOrThrow(self.SetParent(child, parent.value_or(Entity())));
        }, py::arg("child"), py::arg("parent") = std::nullopt)

        // ---- Prefabs ----
        .def("instantiate_prefab", [](Editor::EditorAPI& self, const std::filesystem::path& prefabPath,
                                       std::optional<Vec3> position) {
            return UnwrapOrThrow(self.InstantiatePrefab(prefabPath, position));
        }, py::arg("prefab_path"), py::arg("position") = std::nullopt)
        .def("capture_prefab", [](Editor::EditorAPI& self, Entity entity, const std::filesystem::path& outputPath) {
            UnwrapOrThrow(self.CapturePrefab(entity, outputPath));
        }, py::arg("entity"), py::arg("output_path"))
        .def("revert_prefab_instance", [](Editor::EditorAPI& self, Entity entity) {
            UnwrapOrThrow(self.RevertPrefabInstance(entity));
        }, py::arg("entity"))

        // ---- Component editing ----
        .def("add_component", [](Editor::EditorAPI& self, Entity entity, const std::string& componentType) {
            UnwrapOrThrow(self.AddComponent(entity, componentType));
        })
        .def("remove_component", [](Editor::EditorAPI& self, Entity entity, const std::string& componentType) {
            UnwrapOrThrow(self.RemoveComponent(entity, componentType));
        });

    // CommandManager is a singleton only ever exposed by reference via get_instance()
    // (never constructed or owned from the Python side), and it isn't copyable (holds
    // std::vector<std::unique_ptr<ICommand>>). A plain py::class_<CommandManager> makes
    // MSVC eagerly instantiate the implicit copy constructor while resolving pybind11's
    // internal traits, which fails because that copy ctor is deleted (C2672 via the
    // unique_ptr members). The nodelete holder tells pybind11 it never owns/copies/deletes
    // the instance, matching how this type is actually used.
    py::class_<CommandManager, std::unique_ptr<CommandManager, py::nodelete>>(m, "CommandManager")
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
