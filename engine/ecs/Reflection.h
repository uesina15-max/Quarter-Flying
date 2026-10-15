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

    // ========================================
    // 파일 로드 업그레이드 훅 (docs/USABILITY_IMPROVEMENT_PLAN.md X-1a)
    // ========================================
    //
    // 옛 형식의 파일을 읽을 때 컴포넌트 데이터를 지금 형식으로 고치는 컴포넌트별 함수. 예: prefabPath 절대 경로 ->
    // 프로젝트 상대 경로, 옛 RenderableComponent의 기본색 보정(R-2).
    //
    // **파일 로더에서만** 부른다: SceneIO(씬 파일)와 PrefabAsset(프리팹 파일). PIE 스냅샷(Play/Stop), Undo 스냅샷,
    // AddComponent(빈 JSON {}), SetComponentJson(일부 필드만)에는 부르지 않는다 - 거기서 "키가 없다"는 레거시 파일이
    // 아니라 "기본값"이나 "그대로 둠"을 뜻하므로, 마이그레이션을 섞으면 Inspector 편집이 데이터 변환을 일으킨다.
    // 그래서 일반 역직렬화(ComponentInfo::deserialize)와 별개의 함수로 둔다.
    struct FileLoadContext
    {
        const char* fileKind = "";           // "scene" | "prefab" (로그용)
        int fileVersion = 0;                 // 파일의 "version" 값
        std::string projectRoot;             // UTF-8. 비어 있으면 경로 정규화를 하지 않는다
        std::vector<std::string>* warnings = nullptr;   // 있으면 사용자에게 보여 줄 경고를 추가한다
    };
    using ComponentFileUpgrader = std::function<void(nlohmann::json& data, const FileLoadContext& ctx)>;

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
        ComponentFileUpgrader upgradeFromFile;   // 선택. 파일 로더에서만 부른다(위 FileLoadContext 주석)
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

        // 등록된 컴포넌트에 파일 로드 업그레이드 훅을 단다(이름·해시 두 맵 모두). 없는 이름이면 false.
        static bool SetFileUpgrader(const std::string& name, ComponentFileUpgrader upgrader);

    private:
        static std::unordered_map<std::string, ComponentInfo>& GetComponentsMap();
        static std::unordered_map<StringHash, ComponentInfo>& GetComponentsByHashMap();
    };

    // 씬 전체 상태 직렬화/역직렬화
    nlohmann::json SerializeRegistry(ECSRegistry& registry);
    void DeserializeRegistry(ECSRegistry& registry, const nlohmann::json& data);

    // 파일 로더(SceneIO, PrefabAsset)가 컴포넌트 묶음({"이름": {필드...}, ...}) 하나에 대해 부른다.
    // 각 컴포넌트의 upgradeFromFile을 제자리에서 적용한다. 등록되지 않았거나 훅이 없는 컴포넌트는 그대로 둔다.
    void UpgradeComponentsFromFile(nlohmann::json& components, const FileLoadContext& ctx);

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
