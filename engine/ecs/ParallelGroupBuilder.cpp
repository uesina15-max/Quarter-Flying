#include "ParallelGroupBuilder.h"
#include "SystemDependencyAnalyzer.h"
#include "../core/logging/Logger.h"
#include <algorithm>
#include <queue>
#include <cassert>
#include <string>

namespace Engine
{
    void ParallelGroupBuilder::ValidateShadowGroupConflicts(const std::vector<std::vector<System*>>& groups)
    {
        for (size_t groupIdx = 0; groupIdx < groups.size(); ++groupIdx)
        {
            const auto& group = groups[groupIdx];
            if (group.size() <= 1) continue;

            for (size_t i = 0; i < group.size(); ++i)
            {
                System* sysI = group[i];
                auto readI = sysI->GetReadComponentTypes();
                auto writeI = sysI->GetWriteComponentTypes();
                std::sort(readI.begin(), readI.end());
                std::sort(writeI.begin(), writeI.end());

                for (size_t j = i + 1; j < group.size(); ++j)
                {
                    System* sysJ = group[j];
                    auto readJ = sysJ->GetReadComponentTypes();
                    auto writeJ = sysJ->GetWriteComponentTypes();
                    std::sort(readJ.begin(), readJ.end());
                    std::sort(writeJ.begin(), writeJ.end());

                    bool conflict = false;
                    std::string reason;

                    if (!sysI->CanRunInParallel() || !sysJ->CanRunInParallel())
                    {
                        conflict = true;
                        reason = "Non-parallel execution constraint violated";
                    }

                    size_t conflictType = 0;
                    if (!conflict && SystemDependencyAnalyzer::HasIntersection(writeI, writeJ, conflictType))
                    {
                        conflict = true;
                        reason = "Write-Write conflict on component type " + std::to_string(conflictType);
                    }
                    if (!conflict && SystemDependencyAnalyzer::HasIntersection(writeI, readJ, conflictType))
                    {
                        conflict = true;
                        reason = "Write-After-Read conflict on component type " + std::to_string(conflictType);
                    }
                    if (!conflict && SystemDependencyAnalyzer::HasIntersection(readI, writeJ, conflictType))
                    {
                        conflict = true;
                        reason = "Read-After-Write conflict on component type " + std::to_string(conflictType);
                    }

                    if (conflict)
                    {
                        Logger::Error("Shadow Validator Contract Violation in Group {}: Systems '{}' and '{}' conflict ({})! SystemDependencyAnalyzer failed to model constraint!", groupIdx, sysI->GetName(), sysJ->GetName(), reason);
                        assert(false && "Shadow Validator detected component access conflict in parallel group!");
                    }
                }
            }
        }
    }

    std::vector<std::vector<System*>> ParallelGroupBuilder::Build(const std::vector<std::unique_ptr<System>>& systems, 
                                                                  const std::vector<std::vector<size_t>>& dependencyGraph)
    {
        std::vector<std::vector<System*>> groups;
        std::vector<bool> processed(systems.size(), false);

        Logger::Debug("Identifying parallel system groups using enhanced topological sort");

        std::vector<size_t> inDegree(systems.size(), 0);
        for (size_t i = 0; i < systems.size(); ++i)
        {
            inDegree[i] = dependencyGraph[i].size();
            Logger::Debug("System '{}' has in-degree {}", systems[i]->GetName(), inDegree[i]);
        }

        std::queue<size_t> queue;
        for (size_t i = 0; i < systems.size(); ++i)
        {
            if (inDegree[i] == 0)
            {
                queue.push(i);
                Logger::Debug("System '{}' has no dependencies, adding to initial queue", systems[i]->GetName());
            }
        }

        while (!queue.empty())
        {
            std::vector<System*> currentGroup;
            size_t levelSize = queue.size();

            for (size_t i = 0; i < levelSize; ++i)
            {
                size_t systemIndex = queue.front();
                queue.pop();
                
                currentGroup.push_back(systems[systemIndex].get());
                processed[systemIndex] = true;

                for (size_t j = 0; j < systems.size(); ++j)
                {
                    if (!processed[j])
                    {
                        const auto& deps = dependencyGraph[j];
                        auto it = std::find(deps.begin(), deps.end(), systemIndex);
                        if (it != deps.end())
                        {
                            inDegree[j]--;
                            if (inDegree[j] == 0)
                            {
                                queue.push(j);
                            }
                        }
                    }
                }
            }

            if (!currentGroup.empty())
            {
                groups.push_back(std::move(currentGroup));
            }
        }

        for (size_t i = 0; i < systems.size(); ++i)
        {
            if (!processed[i])
            {
                Logger::Error("System '{}' was not processed - possible circular dependency!", 
                             systems[i]->GetName());
            }
        }

        Logger::Info("Created {} parallel system groups", groups.size());

#if defined(_DEBUG) || defined(DEBUG) || !defined(NDEBUG)
        ValidateShadowGroupConflicts(groups);
#endif

        return groups;
    }
}
