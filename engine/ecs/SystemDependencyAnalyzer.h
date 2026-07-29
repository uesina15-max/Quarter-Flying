#pragma once

#include <vector>
#include <memory>
#include <cstddef>
#include "System.h"

namespace Engine
{
    // ========================================
    // SystemDependencyAnalyzer
    // ========================================
    //
    // ARCHITECTURAL INVARIANT (Single Source of Truth for Scheduling):
    // SystemDependencyAnalyzer is the single source of truth for all system scheduling constraints.
    // Any condition that prohibits parallel execution (e.g., component WAW/WAR/RAW conflicts,
    // main-thread affinity, global state mutation, external resource side effects, or phase barriers)
    // MUST be modeled as dependency edges or constraints in this analyzer.
    // ParallelGroupBuilder relies strictly on the graph produced here and performs pure topological sorting.
    class SystemDependencyAnalyzer
    {
    public:
        // Analyzes dependencies between systems based on their read/write component patterns.
        // Returns an adjacency list where dependencyGraph[i] contains indices of systems that System[i] depends on.
        static std::vector<std::vector<size_t>> Analyze(const std::vector<std::unique_ptr<System>>& systems);
        
        // Expose HasIntersection for use in other components like ParallelGroupBuilder
        static bool HasIntersection(const std::vector<size_t>& a, const std::vector<size_t>& b, size_t& outConflictType);
    };
}
