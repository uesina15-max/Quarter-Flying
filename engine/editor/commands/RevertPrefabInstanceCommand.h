#pragma once

#include "../../core/ICommand.h"
#include "../../ecs/Entity.h"
#include "../../prefab/PrefabAsset.h"
#include "EntitySnapshot.h"
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <vector>

// See docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 4 for the design this
// implements.

namespace Engine
{

class ECSRegistry;

// Reverts a prefab-instance entity back to its source *.prefab.json
// (Definition A, plan §2.5) via PrefabAsset::ApplyToEntity, and makes that
// undoable. Unlike InstantiatePrefabCommand, this acts on an *existing* entity
// (it never creates or destroys one), so both Apply() and Undo() are
// implemented in terms of the same primitive: wrap a target component-set JSON
// in a throwaway PrefabAsset and call its ApplyToEntity — Apply() targets the
// prefab file's own componentsData, Undo() targets a snapshot of the entity's
// components captured just before the first Apply(). This reuses
// ApplyToEntity's Definition A + strong guarantee (plan §2.4) for both
// directions instead of duplicating that logic here.
class RevertPrefabInstanceCommand : public ICommand
{
public:
    RevertPrefabInstanceCommand(ECSRegistry* registry, Entity entity);

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override { return "Revert Prefab Instance"; }

private:
    ECSRegistry* registry_;
    Entity       entity_;
    bool         applied_ = false;

    nlohmann::json              preRevertSnapshot_; // captured once, on first Apply — Undo target
    std::string                 preRevertName_;
    std::optional<PrefabAsset>  loadedAsset_;        // cached once, on first Apply; reused on Redo
                                                      // (same "don't depend on the file between
                                                      // Undo/Redo" reasoning as InstantiatePrefabCommand)

    // Phase 5 (multi-entity prefab only): Revert makes the instance's children
    // exactly the prefab's children — the current descendants are destroyed and the
    // prefab's children are spawned fresh under the root. For a single-entity
    // prefab descendants are left alone (they were attached by the user, not by
    // the prefab — e.g. a camera parented under an instance).
    std::vector<EntitySnapshot> preRevertDescendants_; // Undo restores these (original UUIDs)
    std::vector<UUID>           spawnedChildUUIDs_;     // Redo recreates children with these
};

} // namespace Engine
