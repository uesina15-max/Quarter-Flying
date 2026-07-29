#pragma once

#include <vector>
#include <memory>
#include "System.h"

namespace Engine
{
    class ParallelGroupBuilder
    {
    public:
        // Builds a list of parallel system groups using topological sort.
        // systems: The list of all systems
        // dependencyGraph: The output from SystemDependencyAnalyzer
        static std::vector<std::vector<System*>> Build(const std::vector<std::unique_ptr<System>>& systems, 
                                                       const std::vector<std::vector<size_t>>& dependencyGraph);

        // Shadow Validator (Debug / Assert Contract Verification)
        // Verifies that no two systems within the same parallel group have component access conflicts
        // or violated scheduling constraints.
        static void ValidateShadowGroupConflicts(const std::vector<std::vector<System*>>& groups);
    };
}
