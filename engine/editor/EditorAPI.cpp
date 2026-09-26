#include "EditorAPI.h"
#include "commands/MoveEntityCommand.h"
#include "commands/RotateEntityCommand.h"
#include "commands/ScaleEntityCommand.h"
#include "commands/CreateEntityCommand.h"
#include "commands/DestroyEntityCommand.h"
#include "commands/AddComponentCommand.h"
#include "commands/RemoveComponentCommand.h"
#include "commands/InstantiatePrefabCommand.h"
#include "commands/RevertPrefabInstanceCommand.h"
#include "../prefab/PrefabAsset.h"
#include "../core/CommandManager.h"
#include "../ecs/ECSRegistry.h"

namespace Engine::Editor
{

// ============================================================
// Singleton
// ============================================================

EditorAPI& EditorAPI::GetInstance()
{
    static EditorAPI instance;
    return instance;
}

void EditorAPI::SetRegistry(ECSRegistry* registry)
{
    registry_ = registry;
}

// ============================================================
// Internal helpers
// ============================================================

std::expected<void, EngineError> EditorAPI::CheckRegistry() const
{
    if (!registry_)
    {
        return MakeError(EngineErrorCode::NotInitialized, "Editor registry not set", "EditorAPI");
    }
    return {};
}

std::expected<void, EngineError> EditorAPI::Dispatch(std::unique_ptr<ICommand> cmd)
{
    if (activeTransaction_)
    {
        // Inside a transaction: apply the command immediately for live preview
        // (Req 5.8), then accumulate it as an already-applied command so that
        // CommitTransaction's Apply() is a no-op for it, and CancelTransaction's
        // Undo() can roll it back correctly.
        auto result = cmd->Apply();
        if (!result)
        {
            return result;
        }
        activeTransaction_->AddAppliedCommand(std::move(cmd));
        return {};
    }

    // No active transaction: execute immediately via CommandManager.
    return CommandManager::GetInstance().Execute(std::move(cmd));
}

// ============================================================
// Transaction lifecycle
// ============================================================

std::expected<void, EngineError> EditorAPI::BeginTransaction(const std::string& name)
{
    if (activeTransaction_)
    {
        return MakeError(EngineErrorCode::TransactionAlreadyOpen,
                         "A transaction is already open", "EditorAPI");
    }
    activeTransaction_ = std::make_unique<Transaction>(name);
    return {};
}

std::expected<void, EngineError> EditorAPI::CommitTransaction()
{
    if (!activeTransaction_)
    {
        return MakeError(EngineErrorCode::NoOpenTransaction,
                         "No active transaction to commit", "EditorAPI");
    }

    // Discard empty transactions — don't pollute the undo stack.
    if (activeTransaction_->Empty())
    {
        activeTransaction_.reset();
        return {};
    }

    // Move the transaction out before Execute() so that any Apply() side-effects
    // that happen to call back into EditorAPI don't see an open transaction.
    auto txn = std::move(activeTransaction_);
    return CommandManager::GetInstance().Execute(std::move(txn));
}

std::expected<void, EngineError> EditorAPI::CancelTransaction()
{
    if (!activeTransaction_)
    {
        return MakeError(EngineErrorCode::NoOpenTransaction,
                         "No active transaction to cancel", "EditorAPI");
    }

    // Undo all commands that were pre-applied via Dispatch (Req 1.5).
    // Each command dispatched inside a transaction is immediately Apply()ed
    // and tracked via AddAppliedCommand(), so Transaction::Undo() reverses
    // all of them in reverse order.
    if (!activeTransaction_->Empty())
    {
        (void)activeTransaction_->Undo();
    }

    activeTransaction_.reset();
    return {};
}

// ============================================================
// Transform editing
// ============================================================

std::expected<void, EngineError> EditorAPI::MoveEntity(Entity entity, const Vec3& position)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }
    if (!registry_->HasTransformComponent(entity))
    {
        return MakeError(EngineErrorCode::ComponentNotFound,
                         "Entity has no TransformComponent", "EditorAPI");
    }

    return Dispatch(std::make_unique<MoveEntityCommand>(registry_, entity, position));
}

std::expected<void, EngineError> EditorAPI::RotateEntity(Entity entity, const Quaternion& rotation)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }
    if (!registry_->HasTransformComponent(entity))
    {
        return MakeError(EngineErrorCode::ComponentNotFound,
                         "Entity has no TransformComponent", "EditorAPI");
    }

    return Dispatch(std::make_unique<RotateEntityCommand>(registry_, entity, rotation));
}

std::expected<void, EngineError> EditorAPI::ScaleEntity(Entity entity, const Vec3& scale)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }
    if (!registry_->HasTransformComponent(entity))
    {
        return MakeError(EngineErrorCode::ComponentNotFound,
                         "Entity has no TransformComponent", "EditorAPI");
    }

    return Dispatch(std::make_unique<ScaleEntityCommand>(registry_, entity, scale));
}

// ============================================================
// Entity lifetime
// ============================================================

std::expected<Entity, EngineError> EditorAPI::CreateEntity(const std::string& name)
{
    if (auto r = CheckRegistry(); !r)
    {
        return std::unexpected(r.error());
    }

    auto cmd    = std::make_unique<CreateEntityCommand>(registry_, name);
    auto rawPtr = cmd.get();  // observe result after ownership transfer

    auto result = Dispatch(std::move(cmd));
    if (!result)
    {
        return std::unexpected(result.error());
    }

    // Dispatch() always Apply()s the command (either directly via CommandManager
    // or via the pre-apply path inside a transaction), so GetCreatedEntity() is
    // always valid here.
    Entity created = rawPtr->GetCreatedEntity();
    return created;
}

std::expected<void, EngineError> EditorAPI::DestroyEntity(Entity entity)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }

    return Dispatch(std::make_unique<DestroyEntityCommand>(registry_, entity));
}

// ============================================================
// Prefabs
// ============================================================

std::expected<Entity, EngineError> EditorAPI::InstantiatePrefab(const std::filesystem::path& prefabPath,
                                                                  std::optional<Vec3> position)
{
    if (auto r = CheckRegistry(); !r)
    {
        return std::unexpected(r.error());
    }

    auto cmd    = std::make_unique<InstantiatePrefabCommand>(registry_, prefabPath, position);
    auto rawPtr = cmd.get();  // observe result after ownership transfer

    auto result = Dispatch(std::move(cmd));
    if (!result)
    {
        return std::unexpected(result.error());
    }

    // Dispatch() always Apply()s the command (either directly via CommandManager
    // or via the pre-apply path inside a transaction), so GetSpawnedEntity() is
    // always valid here.
    return rawPtr->GetSpawnedEntity();
}

std::expected<void, EngineError> EditorAPI::CapturePrefab(Entity entity, const std::filesystem::path& outputPath)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }

    auto captured = PrefabAsset::CaptureFromEntity(*registry_, entity);
    if (!captured)
    {
        return std::unexpected(captured.error());
    }

    return captured->SaveToFile(outputPath);
}

std::expected<void, EngineError> EditorAPI::RevertPrefabInstance(Entity entity)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }

    return Dispatch(std::make_unique<RevertPrefabInstanceCommand>(registry_, entity));
}

// ============================================================
// Component editing
// ============================================================

std::expected<void, EngineError> EditorAPI::AddComponent(Entity entity,
                                                          const std::string& componentType)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }

    return Dispatch(std::make_unique<AddComponentCommand>(registry_, entity, componentType));
}

std::expected<void, EngineError> EditorAPI::RemoveComponent(Entity entity,
                                                             const std::string& componentType)
{
    if (auto r = CheckRegistry(); !r) return r;

    if (!registry_->IsValid(entity))
    {
        return MakeError(EngineErrorCode::EntityNotFound, "Invalid entity", "EditorAPI");
    }

    return Dispatch(std::make_unique<RemoveComponentCommand>(registry_, entity, componentType));
}

} // namespace Engine::Editor
