#pragma once

#include "../core/EngineError.h"
#include "../core/Transaction.h"
#include "../core/Types.h"
#include "../ecs/Entity.h"
#include <expected>
#include <memory>
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
    std::expected<void, EngineError>   DestroyEntity(Entity entity);

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
