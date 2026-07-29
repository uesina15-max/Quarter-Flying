#include "DependencyResolver.h"
#include "JobLifecycleManager.h"
#include "../core/logging/Logger.h"
#include <vector>

namespace Engine
{
    DependencyResolver::DependencyResolver(JobLifecycleManager* lifecycleManager)
        : lifecycleManager(lifecycleManager)
    {
    }

    bool DependencyResolver::SetupDependencies(Job* job, const JobHandle* dependencies, uint32_t numDependencies)
    {
        if (!job)
        {
            Logger::Log(LogLevel::Error, "DependencyResolver: Cannot setup dependencies for null job");
            return false;
        }
        
        if (!dependencies || numDependencies == 0)
        {
            return true; // No dependencies to setup
        }

        // 의존성 설정
        for (uint32_t i = 0; i < numDependencies; ++i)
        {
            Job* dependency = lifecycleManager->FindJob(dependencies[i]);
            if (!dependency)
            {
                // Requirement 6.2: Log error with context
                Logger::Log(LogLevel::Error, "DependencyResolver: Invalid dependency handle at index %u (id=%u, gen=%u) for job (id=%u, gen=%u)", 
                           i, dependencies[i].id, dependencies[i].generation, job->handle.id, job->handle.generation);
                return false;
            }

            // 의존성이 이미 완료되었는지 확인
            if (!dependency->IsComplete())
            {
                // 의존성 카운터 증가
                job->unfinishedDependencies.fetch_add(1, std::memory_order_relaxed);
                
                // 의존성의 dependents 목록에 추가
                dependency->dependents.push_back(job);
            }
        }

        // 순환 의존성 검사
        if (HasCircularDependency(lifecycleManager->GetActiveJobs()))
        {
            // Requirement 6.3: Report circular dependency with context
            Logger::Log(LogLevel::Fatal, "DependencyResolver: Circular dependency detected for job (id=%u, gen=%u)", 
                       job->handle.id, job->handle.generation);
            return false;
        }

        return true;
    }

    std::vector<Job*> DependencyResolver::ResolveDependents(Job* completedJob)
    {
        std::vector<Job*> readyJobs;

        if (!completedJob)
        {
            Logger::Log(LogLevel::Warning, "DependencyResolver: Attempted to resolve dependents for null job");
            return readyJobs;
        }

        // 이 Job에 의존하는 모든 Job의 카운터 감소
        for (Job* dependent : completedJob->dependents)
        {
            if (!dependent)
            {
                Logger::Log(LogLevel::Warning, "DependencyResolver: Null dependent found in job (id=%u, gen=%u)", 
                           completedJob->handle.id, completedJob->handle.generation);
                continue;
            }
            
            DecrementDependencyCounter(dependent);

            // 의존성이 모두 해결되었으면 실행 준비 완료
            if (dependent->unfinishedDependencies.load(std::memory_order_acquire) == 0)
            {
                readyJobs.push_back(dependent);
            }
        }

        return readyJobs;
    }

    bool DependencyResolver::HasUnfinishedDependencies(Job* job) const
    {
        if (!job)
        {
            return false;
        }

        return job->unfinishedDependencies.load(std::memory_order_acquire) > 0;
    }

    bool DependencyResolver::HasCircularDependency(const std::vector<Job*>& activeJobs) const
    {
        if (activeJobs.empty())
        {
            return false;
        }

        // DFS를 위한 방문 추적
        std::vector<bool> visited(activeJobs.size(), false);
        std::vector<bool> recStack(activeJobs.size(), false);

        // 모든 Job에 대해 순환 의존성 검사
        for (size_t i = 0; i < activeJobs.size(); ++i)
        {
            if (!visited[i])
            {
                if (DetectCycle(activeJobs[i], visited, recStack, activeJobs))
                {
                    return true;
                }
            }
        }

        return false;
    }

    bool DependencyResolver::DetectCycle(Job* job, std::vector<bool>& visited, std::vector<bool>& recStack,
                                         const std::vector<Job*>& activeJobs) const
    {
        if (!job)
        {
            return false;
        }

        // 현재 Job의 인덱스 찾기
        size_t jobIndex = activeJobs.size();
        for (size_t i = 0; i < activeJobs.size(); ++i)
        {
            if (activeJobs[i] == job)
            {
                jobIndex = i;
                break;
            }
        }

        if (jobIndex >= activeJobs.size())
        {
            return false; // Job이 활성 목록에 없음
        }

        // 이미 재귀 스택에 있으면 순환 의존성
        if (recStack[jobIndex])
        {
            return true;
        }

        // 이미 방문했으면 순환 의존성 없음
        if (visited[jobIndex])
        {
            return false;
        }

        // 방문 표시
        visited[jobIndex] = true;
        recStack[jobIndex] = true;

        // 모든 dependents에 대해 재귀적으로 검사
        for (Job* dependent : job->dependents)
        {
            if (DetectCycle(dependent, visited, recStack, activeJobs))
            {
                return true;
            }
        }

        // 재귀 스택에서 제거
        recStack[jobIndex] = false;

        return false;
    }

    void DependencyResolver::DecrementDependencyCounter(Job* job)
    {
        if (!job)
        {
            return;
        }

        job->unfinishedDependencies.fetch_sub(1, std::memory_order_release);
    }

} // namespace Engine
