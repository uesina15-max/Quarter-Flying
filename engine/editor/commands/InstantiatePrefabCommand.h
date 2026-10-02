#pragma once

#include "../../core/ICommand.h"
#include "../../core/UUID.h"
#include "../../core/Types.h"
#include "../../ecs/Entity.h"
#include "../../prefab/PrefabAsset.h"
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

// See docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 2 for the design this
// implements.

namespace Engine
{

class ECSRegistry;

// Spawns a brand new entity from a *.prefab.json file and tags it with a
// PrefabInstanceComponent (plan §2.3/§2.8). Follows CreateEntityCommand's Redo
// pattern (CreateEntityCommand.h): on the first Apply(), load the file once and
// spawn; on Redo (a second Apply() after Undo()), recreate the entity with the
// same UUID and re-apply the SAME already-loaded PrefabAsset — not a fresh
// SpawnInto (would create yet another entity) and not a fresh LoadFromFile (the
// file may have changed or been deleted between Undo and Redo; Redo must
// reproduce exactly what Undo removed).
class InstantiatePrefabCommand : public ICommand
{
public:
    InstantiatePrefabCommand(ECSRegistry* registry,
                              std::filesystem::path prefabPath,
                              std::optional<Vec3> position = std::nullopt);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Instantiate Prefab"; }

    // 스폰된 Entity를 외부에서 조회 (EditorAPI::InstantiatePrefab 반환용)
    Entity GetSpawnedEntity() const { return spawnedEntity_; }

private:
    ECSRegistry*           registry_;
    std::filesystem::path  prefabPath_;
    std::optional<Vec3>    position_;

    Entity                      spawnedEntity_;   // root
    // Phase 5: every spawned entity's UUID, [0] = root, same order as
    // PrefabAsset::SpawnHierarchyInto. Redo recreates all of them with these UUIDs
    // so anything referring to a child (e.g. a camera rig target) stays valid.
    std::vector<UUID>           savedUUIDs_;
    bool                        applied_ = false;
    std::optional<PrefabAsset>  loadedAsset_; // cached from the first Apply(); reused on Redo
};

} // namespace Engine
