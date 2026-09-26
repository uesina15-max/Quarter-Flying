#pragma once

namespace Engine
{
    // Policy knobs for SerializeEntityComponents (PrefabAsset.h). Deliberately kept
    // in engine/prefab/ rather than engine/ecs/Reflection.h — Reflection.h's public
    // surface (SerializeRegistry/DeserializeRegistry/ComponentRegistry) must not
    // know about prefabs (dependency direction is prefab -> ecs, never reversed;
    // see docs/PREFAB_IMPLEMENTATION_PLAN.md's "모듈 경계 정정" revision note).
    struct SerializeOptions
    {
        // Skip components that are pure instance/editor metadata (currently just
        // PrefabInstanceComponent) when capturing a prefab, so a prefab captured
        // from an existing instance doesn't embed a stale reference to some other
        // prefab file.
        bool excludePrefabMetadata = false;

        // Fail instead of serializing when a component the entity carries declares
        // an EntityRef field (Reflection.h's FieldType::EntityRef). A captured
        // entity reference becomes meaningless once a prefab is instantiated into a
        // different scene/session, so v1 refuses to capture it silently.
        bool rejectEntityRefs = false;
    };
}
