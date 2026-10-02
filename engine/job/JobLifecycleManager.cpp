#include "JobLifecycleManager.h"
#include "../core/logging/Logger.h"
#include <algorithm>
#include <vector>

namespace Engine
{
    JobLifecycleManager::JobLifecycleManager()
        : nextJobID(1)
        , nextGeneration(1)
    {
    }

    JobLifecycleManager::~JobLifecycleManager()
    {
        Clear();
    }

    Job* JobLifecycleManager::CreateJob(JobFunction func, void* data, uint32_t numDependencies)
    {
        Job* job = AllocateJob();
        
        if (!job)
        {
            // Requirement 6.2: Log allocation failure
            Logger::Log(LogLevel::Error, "JobLifecycleManager: Failed to allocate job - out of memory");
            return nullptr;
        }
        
        // Job 초기화
        job->function = func;
        job->data = data;
        job->completed.store(false, std::memory_order_release);
        // 0에서 시작한다. 실제로 기다려야 하는 의존성은 DependencyResolver::SetupDependencies가 센다.
        // 증상(예전): 의존성이 있는 잡은 절대 실행되지 않았다(RenderGraph::ExecuteParallel이 Wait에서 영원히 멈춤).
        // 원인: 여기서 numDependencies로 설정한 뒤 SetupDependencies가 미완료 의존성마다 또 +1 해서 이중으로 셌고,
        // 선행 잡 완료 시에는 1씩만 줄어 0이 되지 않았다. 의존성 디스패치 테스트가 없어서 드러나지 않았다.
        (void)numDependencies;
        job->unfinishedDependencies.store(0, std::memory_order_release);
        job->dependents.clear();

        // 핸들 생성 (atomic 연산으로 스레드 안전)
        uint32_t id = nextJobID.fetch_add(1, std::memory_order_relaxed);
        uint32_t generation = nextGeneration.load(std::memory_order_relaxed);
        job->handle = JobHandle(id, generation);

        // 활성 Job 목록에 추가
        {
            std::lock_guard<std::mutex> lock(activeJobsMutex);
            activeJobs.push_back(job);
        }

        return job;
    }

    void JobLifecycleManager::RetireJob(Job* job)
    {
        if (!job)
        {
            Logger::Log(LogLevel::Warning, "JobLifecycleManager: Attempted to retire null job");
            return;
        }
        
        // 활성 Job 목록에서 제거
        {
            std::lock_guard<std::mutex> lock(activeJobsMutex);
            auto it = std::find(activeJobs.begin(), activeJobs.end(), job);
            if (it != activeJobs.end())
            {
                activeJobs.erase(it);
            }
            else
            {
                Logger::Log(LogLevel::Warning, "JobLifecycleManager: Job (id=%u, gen=%u) not found in active jobs during retirement", 
                           job->handle.id, job->handle.generation);
            }
        }

        // Job을 풀에 반환 (재사용)
        {
            std::lock_guard<std::mutex> lock(jobPoolMutex);
            jobPool.push_back(job);
        }
    }

    Job* JobLifecycleManager::FindJob(JobHandle handle) const
    {
        std::lock_guard<std::mutex> lock(activeJobsMutex);
        return FindJobUnsafe(handle);
    }

    const std::vector<Job*>& JobLifecycleManager::GetActiveJobs() const
    {
        return activeJobs;
    }

    void JobLifecycleManager::CleanupFailedJob(Job* job)
    {
        if (!job)
        {
            Logger::Log(LogLevel::Warning, "JobLifecycleManager: Attempted to cleanup null job");
            return;
        }
        
        Logger::Log(LogLevel::Info, "JobLifecycleManager: Cleaning up failed job (id=%u, gen=%u)", 
                   job->handle.id, job->handle.generation);
        
        // Job을 활성 목록에서 제거
        {
            std::lock_guard<std::mutex> lock(activeJobsMutex);
            auto it = std::find(activeJobs.begin(), activeJobs.end(), job);
            if (it != activeJobs.end())
            {
                activeJobs.erase(it);
            }
        }

        // Job을 풀에 반환
        {
            std::lock_guard<std::mutex> lock(jobPoolMutex);
            jobPool.push_back(job);
        }
    }

    void JobLifecycleManager::Clear()
    {
        // Job 풀 정리
        {
            std::lock_guard<std::mutex> lock(jobPoolMutex);
            for (Job* job : jobPool)
            {
                delete job;
            }
            jobPool.clear();
        }

        // 활성 Job 정리
        {
            std::lock_guard<std::mutex> lock(activeJobsMutex);
            for (Job* job : activeJobs)
            {
                delete job;
            }
            activeJobs.clear();
        }
    }

    Job* JobLifecycleManager::AllocateJob()
    {
        std::lock_guard<std::mutex> lock(jobPoolMutex);

        Job* job = nullptr;

        // 풀에서 재사용
        if (!jobPool.empty())
        {
            job = jobPool.back();
            jobPool.pop_back();
        }
        else
        {
            // 새로 할당
            try
            {
                job = new Job();
            }
            catch (const std::bad_alloc&)
            {
                // Requirement 6.2: Log allocation failure
                Logger::Log(LogLevel::Fatal, "JobLifecycleManager: Failed to allocate new job - std::bad_alloc");
                return nullptr;
            }
        }

        return job;
    }

    Job* JobLifecycleManager::FindJobUnsafe(JobHandle handle) const
    {
        for (Job* job : activeJobs)
        {
            if (job->handle == handle)
            {
                return job;
            }
        }

        return nullptr;
    }

} // namespace Engine
