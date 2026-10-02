#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <cstddef>
#include <functional>
#include <algorithm>
#include "../core/StringHash.h"
#include "ECSRegistry.h"
#include "RotationConversion.h"
#include <type_traits>
#include <nlohmann/json.hpp>

namespace Engine {

    inline void to_json(nlohmann::json& j, const UUID& uuid) {
        j = uuid.GetValue();
    }

    inline void from_json(const nlohmann::json& j, UUID& uuid) {
        uuid = UUID(j.get<uint64_t>());
    }

    enum class FieldType {
        Int,
        Float,
        Bool,
        Vec3,
        String,
        EntityRef, // UUID로 직렬화 필요
        Enum,
        // Quaternion 멤버를 "오일러 각(도) [x,y,z]"로 직렬화/편집한다(RotationConversion.h).
        // 증상: TransformComponent::rotation(Quaternion, 16바이트)이 Vec3(12바이트)로 등록돼 있어서
        // 직렬화에서 w가 빠지고, 역직렬화는 x,y,z만 덮어써서 w가 기본값 1로 남았다. 그래서 회전된
        // 엔티티가 PIE Play->Stop 스냅샷 복원이나 프리팹 인스턴스화 뒤 조용히 다른 회전(정규화도 안 된
        // 쿼터니언)이 됐다. 크래시도 에러도 없었다. 아래 GE_FIELD의 static_assert가 이런 크기 불일치를
        // 이제 컴파일 단계에서 막는다.
        EulerRotation
    };

    // GE_FIELD가 선언된 FieldType과 실제 멤버 크기가 맞는지 컴파일 타임에 검사하기 위한 표.
    template<FieldType T> struct FieldStorageSize;
    template<> struct FieldStorageSize<FieldType::Int>           { static constexpr size_t value = sizeof(int); };
    template<> struct FieldStorageSize<FieldType::Float>         { static constexpr size_t value = sizeof(float); };
    template<> struct FieldStorageSize<FieldType::Bool>          { static constexpr size_t value = sizeof(bool); };
    template<> struct FieldStorageSize<FieldType::Vec3>          { static constexpr size_t value = sizeof(Vec3); };
    template<> struct FieldStorageSize<FieldType::String>        { static constexpr size_t value = sizeof(std::string); };
    template<> struct FieldStorageSize<FieldType::EntityRef>     { static constexpr size_t value = sizeof(Entity); };
    template<> struct FieldStorageSize<FieldType::Enum>          { static constexpr size_t value = sizeof(int); };
    template<> struct FieldStorageSize<FieldType::EulerRotation> { static constexpr size_t value = sizeof(Quaternion); };

    struct FieldInfo {
        StringHash nameHash;  // 컴파일 타임 해시로 변경
        std::string name;     // 디버그/에디터용 원본 문자열 (유지)
        FieldType type;
        size_t offset;

        // UX / Meta elements
        std::string displayName;
        std::string category;
        float min = 0.0f;
        float max = 0.0f;
        std::vector<std::string> enumOptions;
    };

    // Component 직렬화/역직렬화 함수 포인터 (리플렉션 기반 자동 생성)
    using ComponentSerializer = std::function<void(ECSRegistry&, Entity, nlohmann::json&)>;
    using ComponentFactory = std::function<void(ECSRegistry&, Entity, const nlohmann::json&)>;
    // 부분 데이터 업데이트 함수 (StringHash 기반으로 변경)
    using ComponentFieldPatcher = std::function<void(ECSRegistry&, Entity, StringHash, const nlohmann::json&)>;
    // 동적 컴포넌트 제거 함수 (타입 문자열 기반 dynamic remove)
    using ComponentRemover = std::function<void(ECSRegistry&, Entity)>;
    // 동적 컴포넌트 존재 여부 확인 함수
    using ComponentChecker = std::function<bool(ECSRegistry&, Entity)>;

    struct ComponentInfo {
        std::string name;
        StringHash typeHash;
        uint32_t version = 1;  // 스키마 버전 (불일치 차단용)
        std::vector<FieldInfo> fields;
        ComponentSerializer serialize;
        ComponentFactory deserialize;
        ComponentFieldPatcher patchField;
        ComponentRemover remove;    // dynamic remove by type name
        ComponentChecker hasComponent; // dynamic has-component check
    };

    // ========================================
    // ComponentRegistry (Reflection & Binding View Layer)
    // ========================================
    //
    // ARCHITECTURAL INVARIANT (Projection/View Layer over Canonical Registry):
    // ComponentRegistry serves strictly as a reflection, serialization, and script-binding projection/view layer.
    // It is NOT the canonical runtime owner of component types (ComponentTypeRegistry owns canonical ComponentTypeIDs).
    // - StringHash: Used here as the stable lookup key across DSO/module boundaries and script bindings.
    // - name: Human-readable debug key.
    //
    // Registration into ComponentRegistry MUST be synchronized with ComponentTypeRegistry during Phase 1 pipeline unification.
    class ComponentRegistry {
    public:
        static void Register(const ComponentInfo& info);
        
        template<typename T>
        static void RegisterPODComponent(const ComponentInfo& info) {
            static_assert(std::is_standard_layout<T>::value, "Component must be standard layout (POD-like)");
            Register(info);
        }

        static const ComponentInfo* GetComponentInfo(StringHash hash);
        static const ComponentInfo* GetComponentInfo(const char* name);
        static const ComponentInfo* GetComponentInfo(const std::string& name);
        static const std::unordered_map<std::string, ComponentInfo>& GetAllComponents();

    private:
        static std::unordered_map<std::string, ComponentInfo>& GetComponentsMap();
        static std::unordered_map<StringHash, ComponentInfo>& GetComponentsByHashMap();
    };

    // 씬 전체 상태 직렬화/역직렬화
    nlohmann::json SerializeRegistry(ECSRegistry& registry);
    void DeserializeRegistry(ECSRegistry& registry, const nlohmann::json& data);

    // 디버깅 용도 (Inspector ↔ PIE 문제 파악용)
    void SaveSnapshotToFile(const nlohmann::json& snapshot, const std::string& filepath);

    // 헬퍼 분리 함수들 (리플렉션 등록)
    void RegisterPODComponentsReflection();
    void RegisterScriptComponentReflection();
    void RegisterAIComponentReflection();
    void RegisterPrefabComponentsReflection(); // engine/ecs/Reflection.cpp — registers
                                                // PrefabInstanceComponent (engine/prefab/).
                                                // Declared here, not in engine/prefab/, so
                                                // Reflection.h stays the one place all
                                                // component registration is announced.

    // ========================================
    // Component Registration Macros
    // ========================================

#define GE_BEGIN_COMPONENT(CompType) \
    { \
        Engine::ComponentInfo info; \
        info.name = #CompType; \
        info.typeHash = Engine::StringHash(#CompType); \
        info.version = 1; \
        info.serialize = [](Engine::ECSRegistry& reg, Engine::Entity e, nlohmann::json& out) { \
            const CompType* comp = (const CompType*)reg.GetComponent<CompType>(e); \
            if(!comp) return; \
            nlohmann::json c; \
            const auto* cInfo = Engine::ComponentRegistry::GetComponentInfo(#CompType); \
            if (!cInfo) return; \
            c["_version"] = cInfo->version; \
            for(const auto& f : cInfo->fields) { \
                const void* ptr = reinterpret_cast<const uint8_t*>(comp) + f.offset; \
                switch(f.type) { \
                    case Engine::FieldType::Int: c[f.name] = *static_cast<const int*>(ptr); break; \
                    case Engine::FieldType::Float: c[f.name] = *static_cast<const float*>(ptr); break; \
                    case Engine::FieldType::Bool: c[f.name] = *static_cast<const bool*>(ptr); break; \
                    case Engine::FieldType::Vec3: { \
                        auto v = static_cast<const Engine::Vec3*>(ptr); \
                        c[f.name] = {v->x, v->y, v->z}; \
                        break; \
                    } \
                    case Engine::FieldType::String: { \
                        c[f.name] = *static_cast<const std::string*>(ptr); \
                        break; \
                    } \
                    case Engine::FieldType::EulerRotation: { \
                        Engine::Vec3 e = Engine::EulerDegreesFromQuaternion(*static_cast<const Engine::Quaternion*>(ptr)); \
                        c[f.name] = {e.x, e.y, e.z}; \
                        break; \
                    } \
                    case Engine::FieldType::EntityRef: { \
                        Engine::Entity refEntity = *static_cast<const Engine::Entity*>(ptr); \
                        c[f.name] = reg.GetUUID(refEntity).GetValue(); \
                        break; \
                    } \
                } \
            } \
            out[#CompType] = c; \
        }; \
        info.deserialize = [](Engine::ECSRegistry& reg, Engine::Entity e, const nlohmann::json& data) { \
            if(!reg.HasComponent<CompType>(e)) { \
                CompType defaultComp; \
                reg.AddComponent(e, defaultComp); \
            } \
            CompType* comp = (CompType*)reg.GetComponent<CompType>(e); \
            const auto* cInfo = Engine::ComponentRegistry::GetComponentInfo(#CompType); \
            for(const auto& f : cInfo->fields) { \
                if(data.contains(f.name)) { \
                    void* ptr = reinterpret_cast<uint8_t*>(comp) + f.offset; \
                    switch(f.type) { \
                        case Engine::FieldType::Int: *static_cast<int*>(ptr) = data[f.name].get<int>(); break; \
                        case Engine::FieldType::Float: *static_cast<float*>(ptr) = data[f.name].get<float>(); break; \
                        case Engine::FieldType::Bool: *static_cast<bool*>(ptr) = data[f.name].get<bool>(); break; \
                        case Engine::FieldType::Vec3: { \
                            auto arr = data[f.name]; \
                            *static_cast<Engine::Vec3*>(ptr) = {arr[0], arr[1], arr[2]}; \
                            break; \
                        } \
                        case Engine::FieldType::String: *static_cast<std::string*>(ptr) = data[f.name].get<std::string>(); break; \
                        case Engine::FieldType::EulerRotation: { \
                            auto arr = data[f.name]; \
                            *static_cast<Engine::Quaternion*>(ptr) = Engine::QuaternionFromEulerDegrees({arr[0], arr[1], arr[2]}); \
                            break; \
                        } \
                        case Engine::FieldType::EntityRef: { \
                            uint64_t refUuid = data[f.name].get<uint64_t>(); \
                            *static_cast<Engine::Entity*>(ptr) = reg.GetEntityByUUID(refUuid); \
                            break; \
                        } \
                    } \
                } \
            } \
        }; \
        info.patchField = [](Engine::ECSRegistry& reg, Engine::Entity e, StringHash fieldHash, const nlohmann::json& val) { \
            if(!reg.HasComponent<CompType>(e)) return; \
            CompType* comp = (CompType*)reg.GetComponent<CompType>(e); \
            const auto* cInfo = Engine::ComponentRegistry::GetComponentInfo(#CompType); \
            auto it = std::find_if(cInfo->fields.begin(), cInfo->fields.end(), [&](auto& f){ return f.nameHash == fieldHash; }); \
            if (it == cInfo->fields.end()) return; \
            void* ptr = reinterpret_cast<uint8_t*>(comp) + it->offset; \
            switch(it->type) { \
                case Engine::FieldType::Int: *static_cast<int*>(ptr) = val.get<int>(); break; \
                case Engine::FieldType::Float: *static_cast<float*>(ptr) = val.get<float>(); break; \
                case Engine::FieldType::Bool: *static_cast<bool*>(ptr) = val.get<bool>(); break; \
                case Engine::FieldType::Vec3: *static_cast<Engine::Vec3*>(ptr) = {val[0], val[1], val[2]}; break; \
                case Engine::FieldType::String: *static_cast<std::string*>(ptr) = val.get<std::string>(); break; \
                case Engine::FieldType::EulerRotation: *static_cast<Engine::Quaternion*>(ptr) = Engine::QuaternionFromEulerDegrees({val[0], val[1], val[2]}); break; \
                case Engine::FieldType::EntityRef: { \
                    uint64_t refUuid = val.get<uint64_t>(); \
                    *static_cast<Engine::Entity*>(ptr) = reg.GetEntityByUUID(refUuid); \
                    break; \
                } \
            } \
            reg.OnComponentModified(e, Engine::StringHash(#CompType)); \
        };

#define GE_FIELD(Type, Member, FieldEnum, DisplayName) \
        static_assert(sizeof(std::declval<Type&>().Member) == Engine::FieldStorageSize<Engine::FieldType::FieldEnum>::value, \
                      "GE_FIELD(" #Type ", " #Member "): member size does not match FieldType::" #FieldEnum \
                      " - the reflection layer would read/write the wrong number of bytes"); \
        info.fields.push_back({Engine::StringHash(#Member), #Member, Engine::FieldType::FieldEnum, offsetof(Type, Member), DisplayName, "", 0.0f, 0.0f, {}});

#define GE_END_COMPONENT(CompType) \
        info.remove = [](Engine::ECSRegistry& reg, Engine::Entity e) { \
            reg.RemoveComponent<CompType>(e); \
        }; \
        info.hasComponent = [](Engine::ECSRegistry& reg, Engine::Entity e) -> bool { \
            return reg.HasComponent<CompType>(e); \
        }; \
        Engine::ComponentRegistry::RegisterPODComponent<CompType>(info); \
    }

} // namespace Engine
