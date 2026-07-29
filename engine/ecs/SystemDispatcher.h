#pragma once

#include <vector>
#include "System.h"
#include "ECSRegistry.h"

namespace Engine
{
    class JobSystem;

    class SystemDispatcher
    {
    public:
        // Dispatches parallel system groups to the job system and waits for their completion.
        static void Dispatch(const std::vector<std::vector<System*>>& parallelGroups,
                             JobSystem* jobSystem,
                             ECSRegistry& registry,
                             float deltaTime);
    };
}
