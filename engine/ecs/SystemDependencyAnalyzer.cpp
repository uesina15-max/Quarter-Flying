#include "SystemDependencyAnalyzer.h"
#include "../core/logging/Logger.h"
#include <algorithm>

namespace Engine
{
    std::vector<std::vector<size_t>> SystemDependencyAnalyzer::Analyze(const std::vector<std::unique_ptr<System>>& systems)
    {
        std::vector<std::vector<size_t>> systemDependencies(systems.size());

        Logger::Debug("Analyzing dependencies for {} systems", systems.size());

        // Cache sorted read/write component types for O(N+M) intersection
        std::vector<std::vector<size_t>> systemReadTypes(systems.size());
        std::vector<std::vector<size_t>> systemWriteTypes(systems.size());

        for (size_t i = 0; i < systems.size(); ++i)
        {
            auto readTypes = systems[i]->GetReadComponentTypes();
            auto writeTypes = systems[i]->GetWriteComponentTypes();

            systemReadTypes[i] = readTypes;
            std::sort(systemReadTypes[i].begin(), systemReadTypes[i].end());

            systemWriteTypes[i] = writeTypes;
            std::sort(systemWriteTypes[i].begin(), systemWriteTypes[i].end());

            Logger::Debug("System '{}': {} read types, {} write types", 
                         systems[i]->GetName(), readTypes.size(), writeTypes.size());
        }

        for (size_t i = 0; i < systems.size(); ++i)
        {
            for (size_t j = 0; j < i; ++j) // j < i ensures System j runs before System i
            {
                bool hasConflict = false;
                std::string conflictReason;
                size_t conflictType = 0;

                // 1. Write-After-Write (WAW) conflict
                if (HasIntersection(systemWriteTypes[i], systemWriteTypes[j], conflictType))
                {
                    hasConflict = true;
                    conflictReason = "Write-Write conflict on component type " + std::to_string(conflictType);
                }

                // 2. Write-After-Read (WAR) conflict
                if (!hasConflict && HasIntersection(systemWriteTypes[i], systemReadTypes[j], conflictType))
                {
                    hasConflict = true;
                    conflictReason = "Write-After-Read conflict on component type " + std::to_string(conflictType);
                }

                // 3. Read-After-Write (RAW) conflict
                if (!hasConflict && HasIntersection(systemReadTypes[i], systemWriteTypes[j], conflictType))
                {
                    hasConflict = true;
                    conflictReason = "Read-After-Write conflict on component type " + std::to_string(conflictType);
                }

                // 4. Non-parallel execution constraint
                if (!hasConflict && (!systems[i]->CanRunInParallel() || !systems[j]->CanRunInParallel()))
                {
                    hasConflict = true;
                    conflictReason = "Non-parallel execution constraint";
                }

                if (hasConflict)
                {
                    systemDependencies[i].push_back(j);
                    Logger::Debug("Added dependency: '{}' depends on '{}' ({})", 
                                 systems[i]->GetName(), systems[j]->GetName(), conflictReason);
                }
            }
        }

        size_t totalDependencies = 0;
        for (size_t i = 0; i < systems.size(); ++i)
        {
            if (!systemDependencies[i].empty())
            {
                Logger::Debug("System '{}' has {} dependencies", 
                             systems[i]->GetName(), systemDependencies[i].size());
                totalDependencies += systemDependencies[i].size();
            }
        }

        Logger::Info("Dependency analysis completed: {} total dependencies found", totalDependencies);

        return systemDependencies;
    }

    bool SystemDependencyAnalyzer::HasIntersection(const std::vector<size_t>& a, const std::vector<size_t>& b, size_t& outConflictType)
    {
        auto itA = a.begin();
        auto itB = b.begin();
        while (itA != a.end() && itB != b.end()) {
            if (*itA == *itB) {
                outConflictType = *itA;
                return true;
            }
            if (*itA < *itB) ++itA;
            else ++itB;
        }
        return false;
    }
}
