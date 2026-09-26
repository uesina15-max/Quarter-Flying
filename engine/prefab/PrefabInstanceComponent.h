#pragma once

#include <cstdint>
#include <string>

namespace Engine
{
    // Marks an entity as having been spawned from a prefab file and remembers
    // where from, so the editor can offer Revert (Phase 4) and so ApplyToEntity's
    // Definition A knows not to strip this component back off (see
    // docs/PREFAB_IMPLEMENTATION_PLAN.md §2.3/§2.5).
    //
    // The struct lives here, but its ComponentRegistry registration (the
    // GE_BEGIN_COMPONENT/GE_FIELD/GE_END_COMPONENT macros) is added to
    // engine/ecs/Reflection.cpp instead of to this file or to PrefabAsset.cpp —
    // registration is kept alongside the other 5 registered component types so
    // PrefabAsset itself doesn't take on responsibility for reflection wiring
    // (see docs/PREFAB_IMPLEMENTATION_PLAN.md's "모듈 경계 정정" revision note).
    // "This struct exists" is not the same thing as "this struct is registered" —
    // only a registered component round-trips through SerializeRegistry() (the PIE
    // snapshot mechanism) and, eventually, scene save/load.
    struct PrefabInstanceComponent
    {
        std::string prefabPath;                // path this instance was spawned from
        uint32_t    sourcePrefabVersion = 1;    // PrefabAsset::version snapshot at spawn time
                                                 // (file-format version, NOT a content revision —
                                                 // see docs/PREFAB_IMPLEMENTATION_PLAN.md §2.3 Option A)
    };

    // The name this component is registered under in ComponentRegistry (must match
    // the GE_BEGIN_COMPONENT(PrefabInstanceComponent) stringification in
    // engine/ecs/Reflection.cpp). PrefabAsset.cpp's exclude/exempt checks compare
    // against this constant rather than repeating the string literal, so a future
    // rename only has one place to update instead of silently drifting.
    inline constexpr const char* kPrefabInstanceComponentName = "PrefabInstanceComponent";
}
