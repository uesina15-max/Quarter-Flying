#pragma once

#include <string>

namespace pybind11 { class object; }

namespace Engine
{
    struct ScriptComponent
    {
        std::string moduleName;
        std::string className;

        void* pyInstance = nullptr;
        void* cachedOnUpdate = nullptr;
        void* cachedOnDestroy = nullptr;
        bool hasUpdate = false;
        bool hasDestroy = false;
        bool dirty = false;

        ScriptComponent() = default;
    };
}
