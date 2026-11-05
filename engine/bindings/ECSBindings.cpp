#include "Bindings.h"
#include "../ecs/World.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Entity.h"
#include "../ecs/Components.h"
#include "../ecs/ScriptComponent.h"
#include "../ecs/Reflection.h"
#include "../ecs/CameraSystem.h"
#include "../ecs/Hierarchy.h"
#include "../ecs/StaticCollision.h"
#include "../ecs/ScriptContext.h"
#include "../core/logging/Logger.h"
#include "../core/CommandManager.h"
#include "../scene/SceneIO.h"
#include <pybind11/stl.h>
#include <nlohmann/json.hpp>
#include <vector>
#include <string>

namespace Engine {
namespace Bindings {

void RegisterEntityBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // Entity
    // ============================================================
    py::class_<Entity>(m, "Entity")
        .def(py::init<>())
        .def(py::init<EntityID>())
        .def_readwrite("id",   &Entity::id)
        .def("IsValid",        &Entity::IsValid)
        .def("__eq__", [](const Entity& a, const Entity& b){
            return a == b;
        })
        .def("__repr__", [](const Entity& e) {
            return "Entity(id=" + std::to_string(e.id) + ")";
        });
}

// Read-only snapshot of a TransformComponent exposed to Python.
// Values are copied out of the ECS at query time so Python scripts
// cannot mutate engine state by accident.
struct TransformComponentView {
    Vec3       position;
    Quaternion rotation;
    Vec3       scale;
};

void RegisterComponentBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // Components
    // ============================================================

    // TransformComponentView — read-only snapshot (Req 6.2, 6.6)
    // Must be registered before TransformComponent because ECSRegistry
    // may expose factory helpers that return views.
    py::class_<TransformComponentView>(m, "TransformComponentView")
        .def(py::init<>())
        .def_readonly("position", &TransformComponentView::position)
        .def_readonly("rotation", &TransformComponentView::rotation)
        .def_readonly("scale",    &TransformComponentView::scale)
        .def("__repr__", [](const TransformComponentView& v) {
            return "TransformComponentView(pos=(" +
                   std::to_string(v.position.x) + "," +
                   std::to_string(v.position.y) + "," +
                   std::to_string(v.position.z) + "))";
        });

    // TransformComponent — all fields read-only (Req 6.1, 6.3)
    // Transform mutations must go through EditorAPI.
    py::class_<TransformComponent>(m, "TransformComponent")
        .def(py::init<>())
        .def_readonly("position", &TransformComponent::position)
        .def_readonly("rotation", &TransformComponent::rotation)
        .def_readonly("scale",    &TransformComponent::scale)
        .def_property_readonly("position_x",
            [](const TransformComponent& tc){ return tc.position.x; })
        .def_property_readonly("position_y",
            [](const TransformComponent& tc){ return tc.position.y; })
        .def_property_readonly("position_z",
            [](const TransformComponent& tc){ return tc.position.z; })
        .def_property_readonly("rotation_x",
            [](const TransformComponent& tc){ return tc.rotation.x; })
        .def_property_readonly("rotation_y",
            [](const TransformComponent& tc){ return tc.rotation.y; })
        .def_property_readonly("rotation_z",
            [](const TransformComponent& tc){ return tc.rotation.z; })
        .def_property_readonly("scale_x",
            [](const TransformComponent& tc){ return tc.scale.x; })
        .def_property_readonly("scale_y",
            [](const TransformComponent& tc){ return tc.scale.y; })
        .def_property_readonly("scale_z",
            [](const TransformComponent& tc){ return tc.scale.z; });

    // RenderableComponent
    py::class_<RenderableComponent>(m, "RenderableComponent")
        .def(py::init<>())
        .def_readwrite("mesh_path",       &RenderableComponent::meshPath)
        .def_readwrite("material_path",   &RenderableComponent::materialPath)
        .def_readwrite("texture_path",    &RenderableComponent::texturePath)
        .def_readwrite("cast_shadows",    &RenderableComponent::castShadows);

    // CameraComponent
    py::class_<CameraComponent>(m, "CameraComponent")
        .def(py::init<>())
        .def_readwrite("fov",            &CameraComponent::fov)
        .def_readwrite("near_plane",     &CameraComponent::nearPlane)
        .def_readwrite("far_plane",      &CameraComponent::farPlane)
        .def_readwrite("is_main_camera", &CameraComponent::isMainCamera)
        .def_readwrite("priority",       &CameraComponent::priority)
        .def_readwrite("blend_in_seconds", &CameraComponent::blendInSeconds);

    // ScriptComponent
    py::class_<ScriptComponent>(m, "ScriptComponent")
        .def(py::init<>())
        .def_readwrite("module_name", &ScriptComponent::moduleName)
        .def_readwrite("class_name",  &ScriptComponent::className)
        .def_readwrite("dirty",       &ScriptComponent::dirty);
}

void RegisterRegistryBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // ECSRegistry
    // ============================================================
    py::class_<ECSRegistry>(m, "ECSRegistry")
        .def("CreateEntity",   &ECSRegistry::CreateEntity)
        .def("DestroyEntity",  &ECSRegistry::DestroyEntity)
        .def("GetEntityCount", &ECSRegistry::GetEntityCount)
        .def("GetAllEntities",  &ECSRegistry::GetAllEntities)
        .def("GetEntityName",   &ECSRegistry::GetEntityName)
        .def("SetEntityName",   &ECSRegistry::SetEntityName)

        .def("HasTransformComponent", &ECSRegistry::HasTransformComponent)
        .def("GetTransformComponent",
            &ECSRegistry::GetTransformComponent,
            py::return_value_policy::reference)
        .def("SetTransformPosition", &ECSRegistry::SetTransformPosition)
        .def("MoveCharacter", [](ECSRegistry& registry, Entity entity, float dx, float dy, float dz) {
            const glm::vec3 start(ComputeWorldMatrix(registry, entity)[3]);
            const auto result = MoveCharacter(registry, entity, start, {dx, dy, dz});
            const auto local = LocalPositionForWorld(registry, entity, result.feet);
            registry.SetTransformPosition(entity, local.x, local.y, local.z);
            return py::make_tuple(local.x, local.y, local.z, result.hasGeometry, result.grounded, result.hitCeiling);
        })
        .def("SetTransformRotation", &ECSRegistry::SetTransformRotation)
        .def("SetTransformScale",    &ECSRegistry::SetTransformScale)

        .def("HasRenderableComponent", &ECSRegistry::HasRenderableComponent)
        .def("GetRenderableComponent",
            &ECSRegistry::GetRenderableComponent,
            py::return_value_policy::reference)

        .def("HasCameraComponent", &ECSRegistry::HasCameraComponent)
        .def("GetCameraComponent",
            &ECSRegistry::GetCameraComponent,
            py::return_value_policy::reference)
            
        .def("HasScriptComponent", &ECSRegistry::HasComponent<ScriptComponent>)
        .def("GetScriptComponent", 
            static_cast<ScriptComponent* (ECSRegistry::*)(Entity)>(&ECSRegistry::GetComponent<ScriptComponent>),
            py::return_value_policy::reference)

        .def("GetRegisteredComponents", [](ECSRegistry& reg) {
            std::vector<std::string> names;
            for (const auto& pair : ComponentRegistry::GetAllComponents()) {
                names.push_back(pair.first);
            }
            return names;
        })
        .def("GetComponentSchema", [](ECSRegistry& reg, const std::string& compName) -> std::string {
            auto info = ComponentRegistry::GetComponentInfo(compName);
            if (!info) return "[]";
            
            nlohmann::json schema = nlohmann::json::object();
            schema["version"] = info->version;
            
            nlohmann::json fields = nlohmann::json::array();
            for (const auto& field : info->fields) {
                nlohmann::json f;
                f["name"]        = field.name;
                f["type"]        = static_cast<int>(field.type);
                f["displayName"] = field.displayName;
                f["category"]    = field.category;
                f["min"]         = field.min;
                f["max"]         = field.max;
                if (field.type == Engine::FieldType::Enum) {
                    f["options"] = field.enumOptions;
                }
                fields.push_back(f);
            }
            schema["fields"] = fields;
            return schema.dump();
        })
        .def("GetComponentJson", [](ECSRegistry& reg, Entity entity, const std::string& compName) -> std::string {
            auto info = ComponentRegistry::GetComponentInfo(compName);
            if (!info) return "{}";
            
            nlohmann::json outJson;
            if (info->serialize) {
                info->serialize(reg, entity, outJson);
            }
            
            if (outJson.contains(compName)) {
                return outJson[compName].dump();
            }
            return "{}";
        })
        .def("SetComponentFieldJson", [](ECSRegistry& reg, Entity entity, const std::string& compName, const std::string& fieldName, const std::string& jsonValueStr, uint32_t expectedVersion) {
            auto info = ComponentRegistry::GetComponentInfo(compName);
            if (!info) return;
            if (info->version != expectedVersion) {
                Logger::Error("Schema version mismatch for {}: Expected {}, Got {}", compName, info->version, expectedVersion);
                return;
            }
            
            try {
                auto data = nlohmann::json::parse(jsonValueStr);
                if (info->patchField) {
                    info->patchField(reg, entity, fieldName, data);
                }
            } catch (const std::exception& e) {
                Logger::Error("Failed to patch component field JSON for {}.{}: {}", compName, fieldName, e.what());
            }
        })
        // EntityRef 필드는 UUID로 직렬화된다(Reflection.h). 에디터가 EntityRef를 편집하려면 runtime id가 아니라
        // UUID를 주고받아야 한다(inspector.py EntityRef 위젯 주석 참고).
        .def("GetEntityUUID", [](ECSRegistry& reg, Entity entity) { return reg.GetUUID(entity).GetValue(); })
        .def("GetEntityByUUID", [](ECSRegistry& reg, uint64_t uuid) { return reg.GetEntityByUUID(UUID(uuid)); })
        .def("SetComponentJson", [](ECSRegistry& reg, Entity entity, const std::string& compName, const std::string& jsonString) {
            auto info = ComponentRegistry::GetComponentInfo(compName);
            if (!info) return;
            
            try {
                auto data = nlohmann::json::parse(jsonString);
                if (info->deserialize) {
                    // 증상: SetComponentJson이 아무 예외도, 로그도 없이 "성공"하는데 값이
                    // 하나도 반영되지 않았다. 에디터가 시작할 때 Main Camera에 넣는
                    // {"fov":60, ..., "isMainCamera":true}도, 파티클 이펙트 설정도 전부
                    // 조용히 무시되고 컴포넌트가 기본값 그대로 남았다. 크래시도 없고
                    // 컴포넌트는 실제로 붙어 있어서(add_component는 따로 성공) 발견이
                    // 매우 어려웠다 - Inspector에 기본값이 떠 있는 걸 보고서야 알았다.
                    //
                    // 원인: 여기서 `wrapper[compName] = data`로 한 겹 감싸서 넘기고
                    // 있었는데, deserialize는 감싸지 않은 **컴포넌트 객체 자체**를
                    // 기대한다. GE_BEGIN_COMPONENT가 만드는 람다는 `data.contains(f.name)`
                    // 로 필드를 찾으므로, 감싼 JSON에서는 필드 이름이 하나도 매칭되지
                    // 않아 for 루프가 전부 헛돌고 조용히 끝난다. 손으로 등록한
                    // ScriptComponent/AIComponent의 람다도 같은 규약이고,
                    // DeserializeRegistry()가 `compsJson`의 각 항목(=안쪽 객체)을
                    // 넘기는 것도 같다 - 즉 이 호출부만 규약을 어기고 있었다.
                    info->deserialize(reg, entity, data);
                }
                
                if (compName == "ScriptComponent") {
                    auto sc = reg.GetComponent<ScriptComponent>(entity);
                    if (sc) sc->dirty = true;
                }
                
                reg.OnComponentModified(entity, compName);
            } catch (const std::exception& e) {
                Logger::Error("Failed to set component JSON for {}: {}", compName, e.what());
            }
        });
}

void RegisterActiveCameraBindings(pybind11::module_& m) {
    namespace py = pybind11;
    // 활성 카메라 선택 규칙(CameraSystem.h SelectActiveCamera)을 에디터에서 그대로 쓴다 - 파이썬에서
    // 규칙을 다시 구현하지 않기 위함.
    py::class_<ActiveCameraSelection>(m, "ActiveCameraSelection")
        .def_readonly("entity",          &ActiveCameraSelection::entity)
        .def_readonly("candidate_count", &ActiveCameraSelection::candidateCount)
        .def_readonly("tie",             &ActiveCameraSelection::tie);
    m.def("SelectActiveCamera", &SelectActiveCamera, py::arg("registry"));

    // 계층 조회(ecs/Hierarchy.h, 프리팹 Phase 5). Scene Hierarchy 트리가 이 규칙(무효 parent = 루트)을
    // 그대로 쓰게 한다. 변경은 Undo가 되는 EditorAPI.set_parent로 한다.
    m.def("GetParent", [](ECSRegistry& registry, Entity entity) { return GetParent(registry, entity); },
          py::arg("registry"), py::arg("entity"));
    m.def("GetChildren", [](ECSRegistry& registry, Entity entity) { return GetChildren(registry, entity); },
          py::arg("registry"), py::arg("entity"));
    // 뷰포트 드래그(docs/VIEWPORT_MANIPULATION_PLAN.md): 월드에서 계산한 위치를 로컬 값으로 바꾼다. 부모 행렬 계산은
    // C++(ComputeWorldMatrix) 한 곳 - 에디터가 부모 회전·스케일을 다시 구현하지 않는다.
    m.def("GetWorldPosition", [](ECSRegistry& registry, Entity entity) {
        const glm::vec3 p = glm::vec3(ComputeWorldMatrix(registry, entity)[3]);
        return py::make_tuple(p.x, p.y, p.z);
    }, py::arg("registry"), py::arg("entity"));
    // 월드 회전 (x, y, z, w). 부모 회전을 누적한다(ComputeWorldTransform). 회전 기즈모가 부모 기준 로컬 회전을 구할 때 쓴다.
    m.def("GetWorldRotation", [](ECSRegistry& registry, Entity entity) {
        const Quaternion q = ComputeWorldTransform(registry, entity).rotation;
        return py::make_tuple(q.x, q.y, q.z, q.w);
    }, py::arg("registry"), py::arg("entity"));
    m.def("LocalPositionForWorld", [](ECSRegistry& registry, Entity entity, float x, float y, float z) {
        const glm::vec3 p = LocalPositionForWorld(registry, entity, glm::vec3(x, y, z));
        return py::make_tuple(p.x, p.y, p.z);
    }, py::arg("registry"), py::arg("entity"), py::arg("x"), py::arg("y"), py::arg("z"));
}

void RegisterWorldBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // World
    // ============================================================
    py::class_<World>(m, "World")
        .def("GetRegistry", static_cast<ECSRegistry* (World::*)()>(&World::GetRegistry),
             py::return_value_policy::reference)
        // PIE(Play-In-Editor) 상태 제어 — C++에는 이미 구현되어 있었으나(World.h) 바인딩에서
        // 누락되어 에디터의 Play/Pause/Stop이 항상 AttributeError로 실패하고 있었다.
        .def("Play",  &World::Play)
        .def("Pause", &World::Pause)
        .def("Stop",  &World::Stop)
        // 씬 파일 저장/불러오기(docs/GAME_BUILD_PLAN.md Phase 1, scene/SceneIO.h).
        // 실패하면 원인 메시지를 담은 RuntimeError를 던진다 - None/False를 돌려주면 호출부가 확인을
        // 빠뜨렸을 때 조용히 "저장된 줄 아는" 상태가 되기 때문이다.
        // 반환값은 경고 표시용 dict: {"entity_count": int, "unregistered_components"/"unknown_components": [str]}.
        .def("SaveScene", [](World& self, const std::string& path) {
            auto result = SaveSceneToFile(*self.GetRegistry(), path);
            if (!result) throw std::runtime_error(result.error().message);
            py::dict report;
            report["entity_count"] = result->entityCount;
            report["unregistered_components"] = result->unregisteredComponents;
            return report;
        }, py::arg("path"))
        // 빈 씬으로 시작(File > New Scene). LoadScene과 같은 이유로 Edit 상태에서만 허용한다.
        .def("NewScene", [](World& self) {
            if (self.GetEditorState() != World::EditorState::Edit)
                throw std::runtime_error("NewScene: stop Play mode before starting a new scene");
            self.GetRegistry()->Clear();
            CommandManager::GetInstance().Clear();
        })
        // project_root: 컴포넌트의 프로젝트 경로(prefabPath)를 정규화할 기준(scene/SceneIO.h). 에디터는 현재 프로젝트 폴더를 넘긴다.
        .def("LoadScene", [](World& self, const std::string& path, const std::string& projectRoot) {
            // Play 중에 불러오면 Stop이 Play 시작 시점의 스냅샷으로 되돌려 버려, 불러온 씬이 사라진다.
            if (self.GetEditorState() != World::EditorState::Edit)
                throw std::runtime_error("LoadScene: stop Play mode before loading a scene");
            auto result = LoadSceneFromFile(*self.GetRegistry(), path, projectRoot);
            if (!result) throw std::runtime_error(result.error().message);
            // 엔티티가 통째로 바뀌었으므로 이전 씬을 가리키는 Undo 기록은 무효다(World::Stop과 같은 처리).
            CommandManager::GetInstance().Clear();
            py::dict report;
            report["entity_count"] = result->entityCount;
            report["unknown_components"] = result->unknownComponents;
            report["warnings"] = result->warnings;
            return report;
        }, py::arg("path"), py::arg("project_root") = std::string());
}

void RegisterECSBindings(pybind11::module_& m) {
    namespace py = pybind11;
    // [IMPORTANT] pybind11 하드 디펜던시 순서 강제!
    // ECSRegistry는 Entity와 Component 타입을 메서드 시그니처에서 사용하고,
    // World는 ECSRegistry를 반환하므로 반드시 아래 순서대로 호출되어야 합니다.
    RegisterEntityBindings(m);     // 1. Entity (기본 ID 타입)
    RegisterComponentBindings(m);  // 2. Components (Transform, Renderable 등)
    RegisterRegistryBindings(m);   // 3. ECSRegistry (Entity 및 Component 의존)
    RegisterActiveCameraBindings(m); // 3-1. 활성 카메라 선택 (ECSRegistry, Entity 의존)
    RegisterWorldBindings(m);      // 4. World (ECSRegistry 의존)

    // 5. 게임 스크립트 문맥(ecs/ScriptContext.h, docs/USABILITY_IMPROVEMENT_PLAN.md G1). 스크립트 훅이 실행되는
    //    동안의 레지스트리. 그 밖에서 부르면 조용히 None을 주지 않고 원인을 적은 예외를 낸다.
    py::module_ script = m.def_submodule("script", "Context for game scripts (valid inside ScriptComponent hooks)");
    script.def("registry", []() -> ECSRegistry& {
        ECSRegistry* registry = ScriptContext::CurrentRegistry();
        if (!registry) {
            throw std::runtime_error(
                "quarterflying.script.registry: no script is running. It is only available inside "
                "ScriptComponent hooks (on_create/on_update/on_destroy) while the engine updates the world");
        }
        return *registry;
    }, py::return_value_policy::reference);
}

} // namespace Bindings
} // namespace Engine
