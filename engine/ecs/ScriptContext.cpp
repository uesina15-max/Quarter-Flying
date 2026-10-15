#include "ScriptContext.h"

namespace Engine
{
    namespace
    {
        thread_local ECSRegistry* t_currentRegistry = nullptr;
    }

    ECSRegistry* ScriptContext::CurrentRegistry()
    {
        return t_currentRegistry;
    }

    ScriptContext::ScopedRegistry::ScopedRegistry(ECSRegistry* registry)
        : previous_(t_currentRegistry)
    {
        t_currentRegistry = registry;
    }

    ScriptContext::ScopedRegistry::~ScopedRegistry()
    {
        t_currentRegistry = previous_;
    }
}
