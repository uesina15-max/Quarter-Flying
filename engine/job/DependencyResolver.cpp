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

    bool DependencyResolver::SetupDependencies(Job* job, const JobHandle* dependencies, uint32_t numDependencies, bool& outReady)
    {
        outReady = false;
        if (!job)
        {
            Logger::Log(LogLevel::Error, "DependencyResolver: Cannot setup dependencies for null job");
            return false;
        }

        if (!dependencies || numDependencies == 0)
        {
            outReady = true;
            return true; // No dependencies to setup
        }

        std::lock_guard<std::mutex> lock(graphMutex);

        // 설정 가드: 등록하는 동안 카운터가 0이 되어 ResolveDependents가 먼저 큐에 넣는 일을 막는다.
        // 끝에서 가드를 내리며 0이 되면(= 기다릴 것 없음) 호출자가 큐에 넣는다. 0으로 만든 쪽만 넣으므로
        // 같은 잡이 두 번 실행되지 않는다.
        job->unfinishedDependencies.fetch_add(1, std::memory_order_acq_rel);

        // 의존성 설정
        for (uint32_t i = 0; i < numDependencies; ++i)
        {
            Job* dependency = lifecycleManager->FindJob(dependencies[i]);
            if (!dependency && dependencies[i].IsValid())
            {
                // 유효한 핸들인데 활성 목록에 없으면 이미 끝나서 회수된 잡이다 - 충족된 의존성으로 본다.
                // 예전에는 여기서 에러를 내고 디스패치 자체를 실패시켰다(RenderGraph는 그 패스를 조용히 건너뜀).
                continue;
            }
            if (!dependency)
            {
                // Requirement 6.2: Log error with context
                Logger::Log(LogLevel::Error, "DependencyResolver: Invalid dependency handle at index %u (id=%u, gen=%u) for job (id=%u, gen=%u)", 
                           i, dependencies[i].id, dependencies[i].generation, job->handle.id, job->handle.generation);
                return false;
            }

            // 의존성이 이미 완료되었는지 확인. graphMutex 안이라 ResolveDependents의 순회와 겹치지 않는다.
            // 완료 플래그는 완료 콜백보다 먼저 설정되므로, 여기서 "미완료"로 보고 등록하면 반드시 순회에 잡힌다.
            if (!dependency->IsComplete())
            {
                job->unfinishedDependencies.fetch_add(1, std::memory_order_acq_rel);
                dependency->dependents.push_back(job);
            }
        }

        // 순환 의존성 검사는 하지 않는다. 디스패치 시점의 새 잡은 "이미 존재하는 잡"에만 의존할 수 있고, 아직
        // 없는 잡에 누가 의존할 수는 없으므로 순환이 구조적으로 생기지 않는다.
        // 증상(예전): 의존성 잡을 많이 디스패치하면 가끔 "Circular dependency detected in Job graph!"가 나며 디스패치가
        // 실패했다. 무효 핸들을 받은 다음 잡도 "Invalid dependency handle"로 연쇄 실패해서 잡이 조용히 사라졌다.
        // 원인: 이 검사가 lifecycleManager->GetActiveJobs()(활성 잡 벡터 참조)를 락 없이 순회했고, 그동안 워커가
        // 그 벡터에서 잡을 지워(RetireJob) 깨진 상태를 읽어서 순환을 오탐했다.

        // 가드를 내린다. 이 감소로 0이 됐으면 기다릴 의존성이 없다.
        outReady = DecrementDependencyCounter(job);
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

        std::lock_guard<std::mutex> lock(graphMutex);

        // 이 Job에 의존하는 모든 Job의 카운터 감소
        for (Job* dependent : completedJob->dependents)
        {
            if (!dependent)
            {
                Logger::Log(LogLevel::Warning, "DependencyResolver: Null dependent found in job (id=%u, gen=%u)", 
                           completedJob->handle.id, completedJob->handle.generation);
                continue;
            }
            
            // 이 감소로 0이 됐을 때만 실행 준비 완료. 감소 후 따로 load하면, 선행 잡 둘이 동시에 끝났을 때
            // 양쪽 다 0을 보고 같은 잡을 두 번 큐에 넣을 수 있다.
            if (DecrementDependencyCounter(dependent))
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

    bool DependencyResolver::DecrementDependencyCounter(Job* job)
    {
        if (!job)
        {
            return false;
        }

        // 반환값: 이 감소로 카운터가 0이 됐으면 true (0을 만든 쪽은 정확히 하나)
        return job->unfinishedDependencies.fetch_sub(1, std::memory_order_acq_rel) == 1;
    }

} // namespace Engine
