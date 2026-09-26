#include "Reflection.h"
#include <fstream>
#include "Components.h"
#include "ScriptComponent.h"
#include "../prefab/PrefabInstanceComponent.h"
#include <string>
#include <unordered_map>

namespace Engine {
    
    std::unordered_map<std::string, ComponentInfo>& ComponentRegistry::GetComponentsMap() {
        static std::unordered_map<std::string, ComponentInfo> s_map;
        return s_map;
    }

    std::unordered_map<StringHash, ComponentInfo>& ComponentRegistry::GetComponentsByHashMap() {
        static std::unordered_map<StringHash, ComponentInfo> s_map;
        return s_map;
    }

    void ComponentRegistry::Register(const ComponentInfo& info) {
        GetComponentsMap()[info.name] = info;
        GetComponentsByHashMap()[info.typeHash] = info;
    }

    const ComponentInfo* ComponentRegistry::GetComponentInfo(StringHash hash) {
        auto& map = GetComponentsByHashMap();
        auto it = map.find(hash);
        if (it != map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const ComponentInfo* ComponentRegistry::GetComponentInfo(const char* name) {
        return GetComponentInfo(StringHash(name));
    }

    const ComponentInfo* ComponentRegistry::GetComponentInfo(const std::string& name) {
        auto& map = GetComponentsMap();
        auto it = map.find(name);
        if (it != map.end()) {
            return &it->second;
        }
        return nullptr;
    }

    const std::unordered_map<std::string, ComponentInfo>& ComponentRegistry::GetAllComponents() {
        return GetComponentsMap();
    }

    nlohmann::json SerializeRegistry(ECSRegistry& registry) {
        nlohmann::json root;
        root["version"] = 1;
        auto entitiesArray = nlohmann::json::array();

        // registry.GetAllEntities() 대신 EntityManager를 통해 가져옴.
        auto entities = registry.GetAllEntities();

        for (Entity entity : entities) {
            nlohmann::json entityJson;
            entityJson["uuid"] = registry.GetUUID(entity).GetValue();
            nlohmann::json compsJson = nlohmann::json::object();

            for (const auto& pair : ComponentRegistry::GetAllComponents()) {
                const auto& info = pair.second;
                if (info.serialize) {
                    info.serialize(registry, entity, compsJson);
                }
            }

            entityJson["components"] = compsJson;
            entitiesArray.push_back(entityJson);
        }

        root["entities"] = entitiesArray;
        return root;
    }

    void DeserializeRegistry(ECSRegistry& registry, const nlohmann::json& data) {
        if (!data.contains("entities")) return;

        // Pass 1: Create all empty entities mapped to their UUIDs
        const auto& entitiesArray = data["entities"];
        for (const auto& entityJson : entitiesArray) {
            UUID uuid = entityJson["uuid"].get<uint64_t>();
            registry.CreateEntityWithUUID(uuid);
        }

        // Pass 2: Restore component data using ComponentFactory mapping
        for (const auto& entityJson : entitiesArray) {
            UUID uuid = entityJson["uuid"].get<uint64_t>();
            Entity entity = registry.GetEntityByUUID(uuid);
            if (!entity.IsValid()) continue;

            if (entityJson.contains("components")) {
                const auto& compsJson = entityJson["components"];
                for (const auto& pair : compsJson.items()) {
                    const std::string& compName = pair.key();
                    const nlohmann::json& compData = pair.value();
                    
                    auto info = ComponentRegistry::GetComponentInfo(compName);
                    if (info && info->deserialize) {
                        info->deserialize(registry, entity, compData);
                    }
                }
            }
        }
    }

    void SaveSnapshotToFile(const nlohmann::json& snapshot, const std::string& filepath) {
        std::ofstream file(filepath);
        if (file.is_open()) {
            file << snapshot.dump(4);
            file.close();
        }
    }

    void RegisterPODComponentsReflection() {
        GE_BEGIN_COMPONENT(TransformComponent)
            GE_FIELD(TransformComponent, position, Vec3, "Position")
            GE_FIELD(TransformComponent, rotation, Vec3, "Rotation")
            GE_FIELD(TransformComponent, scale, Vec3, "Scale")
        GE_END_COMPONENT(TransformComponent)

        GE_BEGIN_COMPONENT(RenderableComponent)
            GE_FIELD(RenderableComponent, meshHandle, Int, "Mesh Handle")
            GE_FIELD(RenderableComponent, materialHandle, Int, "Material Handle")
            GE_FIELD(RenderableComponent, castShadows, Bool, "Cast Shadows")
        GE_END_COMPONENT(RenderableComponent)

        GE_BEGIN_COMPONENT(CameraComponent)
            GE_FIELD(CameraComponent, fov, Float, "Field of View")
            GE_FIELD(CameraComponent, nearPlane, Float, "Near Plane")
            GE_FIELD(CameraComponent, farPlane, Float, "Far Plane")
            GE_FIELD(CameraComponent, isMainCamera, Bool, "Is Main Camera")
        GE_END_COMPONENT(CameraComponent)
    }

    void RegisterScriptComponentReflection() {
        // ScriptComponent (Not POD, Register directly)
        {
            Engine::ComponentInfo info;
            info.name = "ScriptComponent";
            info.typeHash = Engine::StringHash("ScriptComponent");
            info.version = 1;
            info.fields.push_back({Engine::StringHash("module_name"), "module_name", Engine::FieldType::String, 0, "Module Name", "", 0.0f, 0.0f, {}});
            info.fields.push_back({Engine::StringHash("class_name"), "class_name", Engine::FieldType::String, 0, "Class Name", "", 0.0f, 0.0f, {}});
            
            info.serialize = [](Engine::ECSRegistry& reg, Engine::Entity e, nlohmann::json& out) {
                const ScriptComponent* comp = reg.GetComponent<ScriptComponent>(e);
                if (!comp) return;
                nlohmann::json c;
                c["_version"] = 1;
                c["module_name"] = comp->moduleName;
                c["class_name"] = comp->className;
                out["ScriptComponent"] = c;
            };
            info.deserialize = [](Engine::ECSRegistry& reg, Engine::Entity e, const nlohmann::json& data) {
                if(!reg.HasComponent<ScriptComponent>(e)) {
                    ScriptComponent defaultComp;
                    reg.AddComponent(e, defaultComp);
                }
                ScriptComponent* comp = reg.GetComponent<ScriptComponent>(e);
                if (data.contains("module_name")) comp->moduleName = data["module_name"].get<std::string>();
                if (data.contains("class_name")) comp->className = data["class_name"].get<std::string>();
                comp->dirty = true;
            };
            
            info.patchField = [](Engine::ECSRegistry& reg, Engine::Entity e, StringHash fieldHash, const nlohmann::json& val) {
                if(!reg.HasComponent<ScriptComponent>(e)) return;
                ScriptComponent* comp = reg.GetComponent<ScriptComponent>(e);
                if (fieldHash == Engine::StringHash("module_name")) comp->moduleName = val.get<std::string>();
                else if (fieldHash == Engine::StringHash("class_name")) comp->className = val.get<std::string>();
                comp->dirty = true;
                reg.OnComponentModified(e, Engine::StringHash("ScriptComponent"));
            };
            info.remove = [](Engine::ECSRegistry& reg, Engine::Entity e) {
                reg.RemoveComponent<ScriptComponent>(e);
            };
            info.hasComponent = [](Engine::ECSRegistry& reg, Engine::Entity e) -> bool {
                return reg.HasComponent<ScriptComponent>(e);
            };
            
            Engine::ComponentRegistry::Register(info);
        }
    }

    void RegisterAIComponentReflection() {
        // AIComponent
        {
            Engine::ComponentInfo info;
            info.name = "AIComponent";
            info.typeHash = Engine::StringHash("AIComponent");
            info.version = 1;
            info.fields.push_back({Engine::StringHash("enabled"), "enabled", Engine::FieldType::Bool, 0, "Enabled", "", 0.0f, 0.0f, {}});
            info.fields.push_back({Engine::StringHash("sight_range"), "sight_range", Engine::FieldType::Float, 0, "Sight Range", "", 0.0f, 1000.0f, {}});
            info.fields.push_back({Engine::StringHash("attack_range"), "attack_range", Engine::FieldType::Float, 0, "Attack Range", "", 0.0f, 1000.0f, {}});
            info.fields.push_back({Engine::StringHash("idle_action"), "idle_action", Engine::FieldType::String, 0, "Idle Action", "", 0.0f, 0.0f, {}});
            info.fields.push_back({Engine::StringHash("attack_action"), "attack_action", Engine::FieldType::String, 0, "Attack Action", "", 0.0f, 0.0f, {}});
            info.fields.push_back({Engine::StringHash("hit_action"), "hit_action", Engine::FieldType::String, 0, "Hit Action", "", 0.0f, 0.0f, {}});
            info.fields.push_back({Engine::StringHash("default_state"), "default_state", Engine::FieldType::Enum, 0, "Default State", "", 0.0f, 0.0f, {"Idle", "Patrol", "Chase", "Attack", "Return"}});

            info.serialize = [](Engine::ECSRegistry& reg, Engine::Entity e, nlohmann::json& out) {
                const AIComponent* comp = reg.GetComponent<AIComponent>(e);
                if (!comp) return;
                nlohmann::json c;
                c["_version"] = 1;
                c["enabled"] = comp->enabled;
                c["sight_range"] = comp->sight_range;
                c["attack_range"] = comp->attack_range;
                c["idle_action"] = comp->idle_action;
                c["attack_action"] = comp->attack_action;
                c["hit_action"] = comp->hit_action;
                c["default_state"] = static_cast<int>(comp->default_state);
                out["AIComponent"] = c;
            };
            info.deserialize = [](Engine::ECSRegistry& reg, Engine::Entity e, const nlohmann::json& data) {
                if(!reg.HasComponent<AIComponent>(e)) {
                    AIComponent defaultComp;
                    reg.AddComponent(e, defaultComp);
                }
                AIComponent* comp = reg.GetComponent<AIComponent>(e);
                if (data.contains("enabled")) comp->enabled = data["enabled"].get<bool>();
                if (data.contains("sight_range")) comp->sight_range = data["sight_range"].get<float>();
                if (data.contains("attack_range")) comp->attack_range = data["attack_range"].get<float>();
                if (data.contains("idle_action")) comp->idle_action = data["idle_action"].get<std::string>();
                if (data.contains("attack_action")) comp->attack_action = data["attack_action"].get<std::string>();
                if (data.contains("hit_action")) comp->hit_action = data["hit_action"].get<std::string>();
                if (data.contains("default_state")) comp->default_state = static_cast<AIState>(data["default_state"].get<int>());
            };
            
            info.patchField = [](Engine::ECSRegistry& reg, Engine::Entity e, StringHash fieldHash, const nlohmann::json& val) {
                if(!reg.HasComponent<AIComponent>(e)) return;
                AIComponent* comp = reg.GetComponent<AIComponent>(e);
                if (fieldHash == Engine::StringHash("enabled")) comp->enabled = val.get<bool>();
                else if (fieldHash == Engine::StringHash("sight_range")) comp->sight_range = val.get<float>();
                else if (fieldHash == Engine::StringHash("attack_range")) comp->attack_range = val.get<float>();
                else if (fieldHash == Engine::StringHash("idle_action")) comp->idle_action = val.get<std::string>();
                else if (fieldHash == Engine::StringHash("attack_action")) comp->attack_action = val.get<std::string>();
                else if (fieldHash == Engine::StringHash("hit_action")) comp->hit_action = val.get<std::string>();
                else if (fieldHash == Engine::StringHash("default_state")) comp->default_state = static_cast<AIState>(val.get<int>());
                reg.OnComponentModified(e, Engine::StringHash("AIComponent"));
            };
            info.remove = [](Engine::ECSRegistry& reg, Engine::Entity e) {
                reg.RemoveComponent<AIComponent>(e);
            };
            info.hasComponent = [](Engine::ECSRegistry& reg, Engine::Entity e) -> bool {
                return reg.HasComponent<AIComponent>(e);
            };
            
            Engine::ComponentRegistry::Register(info);
        }
    }

    void RegisterPrefabComponentsReflection() {
        // PrefabInstanceComponent registration lives here (alongside the other 5
        // component types) rather than in engine/prefab/ — PrefabAsset itself
        // should not take on responsibility for its own reflection wiring. See
        // docs/PREFAB_IMPLEMENTATION_PLAN.md's "모듈 경계 정정" revision note.
        GE_BEGIN_COMPONENT(PrefabInstanceComponent)
            GE_FIELD(PrefabInstanceComponent, prefabPath, String, "Prefab Path")
            GE_FIELD(PrefabInstanceComponent, sourcePrefabVersion, Int, "Source Prefab Version")
        GE_END_COMPONENT(PrefabInstanceComponent)
    }

    void RegisterParticleComponentsReflection() {
        // VFX Lite Phase 1 — docs/VFX_LITE_IMPLEMENTATION_PLAN.md §2.4.
        // 여기 20개 필드 = 범위 정의서 §2의 18개 파라미터(Direction과 Alpha Fade가
        // 각각 필드 2개). 순서는 Components.h의 선언 순서·§2의 그룹 순서와 같게 유지한다.
        //
        // Enum 필드가 하나도 없는 것은 의도된 것이다 — 아래 매크로가 만드는 직렬화
        // switch에 Enum case가 없어서 조용히 누락되기 때문(docs/VFX_LITE_PLAN.md §7.3).
        // 블렌드 모드를 파라미터로 만들지 않기로 한 결정(구현 계획서 §2.2) 덕분에
        // enum이 필요한 필드가 애초에 생기지 않았다.
        GE_BEGIN_COMPONENT(ParticleEffectComponent)
            // 생성
            GE_FIELD(ParticleEffectComponent, spawnRate, Float, "Spawn Rate")
            GE_FIELD(ParticleEffectComponent, maxParticle, Int, "Max Particle")
            GE_FIELD(ParticleEffectComponent, lifetime, Float, "Lifetime")
            GE_FIELD(ParticleEffectComponent, burst, Int, "Burst")
            // 이동
            GE_FIELD(ParticleEffectComponent, initialSpeed, Float, "Initial Speed")
            GE_FIELD(ParticleEffectComponent, directionBase, Vec3, "Direction")
            GE_FIELD(ParticleEffectComponent, directionSpread, Float, "Direction Spread")
            GE_FIELD(ParticleEffectComponent, gravity, Float, "Gravity")
            GE_FIELD(ParticleEffectComponent, drag, Float, "Drag")
            // 크기
            GE_FIELD(ParticleEffectComponent, startSize, Float, "Start Size")
            GE_FIELD(ParticleEffectComponent, endSize, Float, "End Size")
            // 색상
            GE_FIELD(ParticleEffectComponent, startColor, Vec3, "Start Color")
            GE_FIELD(ParticleEffectComponent, startAlpha, Float, "Start Alpha")
            GE_FIELD(ParticleEffectComponent, endColor, Vec3, "End Color")
            GE_FIELD(ParticleEffectComponent, endAlpha, Float, "End Alpha")
            // 회전
            GE_FIELD(ParticleEffectComponent, startRotation, Float, "Start Rotation")
            GE_FIELD(ParticleEffectComponent, rotationSpeed, Float, "Rotation Speed")
            // 랜덤
            GE_FIELD(ParticleEffectComponent, speedVariance, Float, "Speed +/-")
            GE_FIELD(ParticleEffectComponent, sizeVariance, Float, "Size +/-")
            GE_FIELD(ParticleEffectComponent, rotationVariance, Float, "Rotation +/-")
        GE_END_COMPONENT(ParticleEffectComponent)
    }

    void RegisterActionPlayerComponentReflection() {
        // Sound Lite Phase 5 — docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.3.
        // 등록만 하면 에디터 Inspector 폼이 자동으로 나온다(VFX Lite Phase 1에서 확인된
        // 성질). Enum 필드가 없는 것은 의도된 것이다 - Components.h의 주석 참고.
        GE_BEGIN_COMPONENT(ActionPlayerComponent)
            GE_FIELD(ActionPlayerComponent, action, String, "Action File")
            GE_FIELD(ActionPlayerComponent, playOnStart, Bool, "Play On Start")
            GE_FIELD(ActionPlayerComponent, loop, Bool, "Loop")
        GE_END_COMPONENT(ActionPlayerComponent)
    }

    static int InitializeReflection() {
        RegisterPODComponentsReflection();
        RegisterScriptComponentReflection();
        RegisterAIComponentReflection();
        RegisterPrefabComponentsReflection();
        RegisterParticleComponentsReflection();
        RegisterActionPlayerComponentReflection();
        return 0;
    }
    static int s_dummyInit = InitializeReflection();

} // namespace Engine
