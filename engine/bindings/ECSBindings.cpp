#include "Bindings.h"
#include "../ecs/World.h"
#include "../ecs/ECSRegistry.h"
#include "../ecs/Entity.h"
#include "../ecs/Components.h"
#include "../ecs/ScriptComponent.h"
#include "../ecs/Reflection.h"
#include "../core/logging/Logger.h"
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
        .def_readwrite("mesh_handle",     &RenderableComponent::meshHandle)
        .def_readwrite("material_handle", &RenderableComponent::materialHandle)
        .def_readwrite("cast_shadows",    &RenderableComponent::castShadows);

    // CameraComponent
    py::class_<CameraComponent>(m, "CameraComponent")
        .def(py::init<>())
        .def_readwrite("fov",            &CameraComponent::fov)
        .def_readwrite("near_plane",     &CameraComponent::nearPlane)
        .def_readwrite("far_plane",      &CameraComponent::farPlane)
        .def_readwrite("is_main_camera", &CameraComponent::isMainCamera);

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
        .def("SetComponentJson", [](ECSRegistry& reg, Entity entity, const std::string& compName, const std::string& jsonString) {
            auto info = ComponentRegistry::GetComponentInfo(compName);
            if (!info) return;
            
            try {
                auto data = nlohmann::json::parse(jsonString);
                if (info->deserialize) {
                    nlohmann::json wrapper;
                    wrapper[compName] = data;
                    info->deserialize(reg, entity, wrapper);
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

void RegisterWorldBindings(pybind11::module_& m) {
    namespace py = pybind11;

    // ============================================================
    // World
    // ============================================================
    py::class_<World>(m, "World")
        .def("GetRegistry", static_cast<ECSRegistry* (World::*)()>(&World::GetRegistry),
             py::return_value_policy::reference);
}

void RegisterECSBindings(pybind11::module_& m) {
    // [IMPORTANT] pybind11 하드 디펜던시 순서 강제!
    // ECSRegistry는 Entity와 Component 타입을 메서드 시그니처에서 사용하고,
    // World는 ECSRegistry를 반환하므로 반드시 아래 순서대로 호출되어야 합니다.
    RegisterEntityBindings(m);     // 1. Entity (기본 ID 타입)
    RegisterComponentBindings(m);  // 2. Components (Transform, Renderable 등)
    RegisterRegistryBindings(m);   // 3. ECSRegistry (Entity 및 Component 의존)
    RegisterWorldBindings(m);      // 4. World (ECSRegistry 의존)
}

} // namespace Bindings
} // namespace Engine
