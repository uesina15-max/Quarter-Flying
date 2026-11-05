#include "Bindings.h"
#include "EngineException.h"
#include "../editor/EditorAPI.h"
#include "../core/CommandManager.h"
#include "../ecs/Entity.h"
#include "../ecs/ECSRegistry.h"  // EditorAPI.h only forward-declares ECSRegistry; pybind11's
                                  // __is_base_of type-trait checks need the complete type here.
#include "../core/Types.h"
#include "../prefab/PrefabAsset.h"
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
    m.def("ReadPrefab", [](const std::filesystem::path& path, const std::string& root) {
        return UnwrapOrThrow(PrefabAsset::LoadFromFile(path, root)).ToJson().dump();
    });
    m.def("SnapshotPrefab", [](ECSRegistry& registry, Entity entity) {
        return UnwrapOrThrow(PrefabAsset::CaptureFromEntity(registry, entity)).ToJson().dump();
    });
    // 프리팹 파일을 "지금 스폰하면 나올 모습"으로 돌려준다(임시 레지스트리에 스폰한 뒤 SnapshotPrefab과 같은 경로로 캡처).
    // 인스턴스 스냅샷과 필드 하나하나 비교하려면 양쪽이 같은 경로를 지나야 한다(core/prefab_variants.py).
    m.def("SnapshotPrefabFile", [](const std::filesystem::path& path, const std::string& root) {
        const PrefabAsset asset = UnwrapOrThrow(PrefabAsset::LoadFromFile(path, root));
        ECSRegistry scratch;
        const auto entities = UnwrapOrThrow(asset.SpawnHierarchyInto(scratch));
        return UnwrapOrThrow(PrefabAsset::CaptureFromEntity(scratch, entities.front())).ToJson().dump();
    }, py::arg("path"), py::arg("root"));

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
        .def("duplicate_entity", [](Editor::EditorAPI& self, Entity entity) {
            return UnwrapOrThrow(self.DuplicateEntity(entity));
        })
        .def("destroy_entity", [](Editor::EditorAPI& self, Entity entity) {
            UnwrapOrThrow(self.DestroyEntity(entity));
        })
        // parent=None이면 루트로 만든다.
        .def("set_parent", [](Editor::EditorAPI& self, Entity child, std::optional<Entity> parent) {
            UnwrapOrThrow(self.SetParent(child, parent.value_or(Entity())));
        }, py::arg("child"), py::arg("parent") = std::nullopt)

        // ---- Prefabs ----
        // 프로젝트 폴더: 프리팹 경로를 이 기준의 상대 경로로 기록·해석한다(core/ProjectPath.h). 에디터가 프로젝트를
        // 열 때 설정한다(project_actions._set_editor_project_root).
        .def("set_project_root", [](Editor::EditorAPI& self, const std::string& projectRoot) {
            self.SetProjectRoot(projectRoot);
        }, py::arg("project_root"))
        .def("get_project_root", &Editor::EditorAPI::GetProjectRoot)
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
        })
        // Inspector의 일반 필드 편집(docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F1). json_value는 json.dumps 결과.
        .def("set_component_field", [](Editor::EditorAPI& self, Entity entity, const std::string& componentType,
                                       const std::string& fieldName, const std::string& jsonValue, uint32_t expectedVersion) {
            UnwrapOrThrow(self.SetComponentField(entity, componentType, fieldName, jsonValue, expectedVersion));
        }, py::arg("entity"), py::arg("component"), py::arg("field"), py::arg("json_value"), py::arg("expected_version"));

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
        .def("is_in_merge_session", &CommandManager::IsInMergeSession)
        .def("get_change_count", &CommandManager::GetChangeCount);
}

} // namespace Engine::Bindings
