#pragma once

#include "System.h"

namespace Engine
{
    struct ScriptComponent;

    class ScriptSystem : public System
    {
    public:
        void Initialize(ECSRegistry& registry) override;
        void Shutdown(ECSRegistry& registry) override;
        void Update(ECSRegistry& registry, float deltaTime) override;
        
        void OnStart(ECSRegistry& registry);
        void OnStop(ECSRegistry& registry);

        const char* GetName() const override { return "ScriptSystem"; }

    private:
        void ClearComponentCache(ScriptComponent& comp);
    };
}
