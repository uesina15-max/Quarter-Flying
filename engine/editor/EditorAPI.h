#pragma once

#include "../core/EngineError.h"
#include "../core/Transaction.h"
#include "../core/Types.h"
#include "../ecs/Entity.h"
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace Engine
{

class ECSRegistry;
class ICommand;

namespace Editor
{

class EditorAPI
{
public:
    static EditorAPI& GetInstance();

    void SetRegistry(ECSRegistry* registry);
    ECSRegistry* GetRegistry() const { return registry_; }

    // ----------------------------------------
    // Transaction lifecycle
    // ----------------------------------------

    std::expected<void, EngineError> BeginTransaction(const std::string& name = "Transaction");
    std::expected<void, EngineError> CommitTransaction();
    std::expected<void, EngineError> CancelTransaction();
    bool IsInTransaction() const { return activeTransaction_ != nullptr; }

    // ----------------------------------------
    // Transform editing
    // ----------------------------------------

    std::expected<void, EngineError> MoveEntity(Entity entity, const Vec3& position);
    std::expected<void, EngineError> RotateEntity(Entity entity, const Quaternion& rotation);
    std::expected<void, EngineError> ScaleEntity(Entity entity, const Vec3& scale);

    // ----------------------------------------
    // Entity lifetime
    // ----------------------------------------

    std::expected<Entity, EngineError> CreateEntity(const std::string& name = "");
    std::expected<void, EngineError>   DestroyEntity(Entity entity);   // 자손까지 함께 파괴(Undo로 전부 복원)

    // 부모 변경(프리팹 Phase 5, ecs/Hierarchy.h). parent가 Entity()면 루트로. 자기 자신/자손을 부모로
    // 지정하면(순환) 에러. 월드 위치는 유지된다(화면에서 제자리) - SetParentCommand 참고.
    std::expected<void, EngineError>   SetParent(Entity child, Entity parent);

    // ----------------------------------------
    // Prefabs (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 2)
    // ----------------------------------------

    // Spawns a new entity from a *.prefab.json file. `position`, when given,
    // overrides only the spawned entity's position (plan §2.8) — omit it to keep
    // the prefab's own captured transform.
    std::expected<Entity, EngineError> InstantiatePrefab(const std::filesystem::path& prefabPath,
                                                           std::optional<Vec3> position = std::nullopt);

    // Captures `entity`'s current components into a *.prefab.json file (plan §3
    // Phase 3). Pure read + file write — does not mutate the registry, so unlike
    // the methods above this does not go through Dispatch()/CommandManager
    // (there is nothing ECS-side for Undo/Redo to act on).
    std::expected<void, EngineError> CapturePrefab(Entity entity, const std::filesystem::path& outputPath);

    // Reverts a prefab-instance entity back to its source file (Definition A,
    // plan §2.5/§3 Phase 4). Unlike CapturePrefab, this *does* mutate the
    // registry, so it goes through Dispatch()/CommandManager like the other
    // methods below — Undo restores the entity to its pre-revert state.
    std::expected<void, EngineError> RevertPrefabInstance(Entity entity);

    // ----------------------------------------
    // Component editing
    // ----------------------------------------

    std::expected<void, EngineError> AddComponent(Entity entity,
                                                   const std::string& componentType);
    std::expected<void, EngineError> RemoveComponent(Entity entity,
                                                      const std::string& componentType);

private:
    EditorAPI() = default;

    // Routes cmd into the active Transaction (AddCommand + Apply),
    // or directly to CommandManager::Execute when no Transaction is open.
    std::expected<void, EngineError> Dispatch(std::unique_ptr<ICommand> cmd);

    // Shared validation: registry must be set.
    std::expected<void, EngineError> CheckRegistry() const;

    ECSRegistry*                  registry_          = nullptr;
    std::unique_ptr<Transaction>  activeTransaction_ = nullptr;
};

} // namespace Editor
} // namespace Engine
