#include "JobScheduler.h"
#include "../core/logging/Logger.h"
#include <thread>
#include <chrono>

namespace Engine
{
    // ========================================
    // Constructor / Destructor
    // ========================================
    
    JobScheduler::JobScheduler()
        : running(false)
        , activeWorkers(0)
        , completionCallback(nullptr)
    {
    }
    
    JobScheduler::~JobScheduler()
    {
        Shutdown();
    }
    
    // ========================================
    // Initialization / Shutdown
    // ========================================
    
    void JobScheduler::Initialize(uint32_t numThreads)
    {
        // 이미 실행 중이면 무시
        if (running.load(std::memory_order_acquire))
        {
            // Requirement 6.2: Log error with context
            Logger::Log(LogLevel::Error, "JobScheduler: Cannot initialize - Already initialized and running");
            return;
        }
        
        // 스레드 수 결정
        uint32_t threadCount = DetermineThreadCount(numThreads);
        
        Logger::Log(LogLevel::Info, "JobScheduler: Initializing with %u worker threads", threadCount);
        
        // 로컬 큐 생성
        localQueues.resize(threadCount);
        
        // 워커 스레드 생성
        running.store(true, std::memory_order_release);
        workers.reserve(threadCount);
        
        for (uint32_t i = 0; i < threadCount; ++i)
        {
            try
            {
                workers.emplace_back(&JobScheduler::WorkerThreadMain, this, i);
            }
            catch (const std::exception& e)
            {
                // Requirement 6.2: Log thread creation failure
                Logger::Log(LogLevel::Fatal, "JobScheduler: Failed to create worker thread %u - %s", i, e.what());
                
                // Cleanup and shutdown
                running.store(false, std::memory_order_release);
                wakeCondition.notify_all();
                
                for (auto& worker : workers)
                {
                    if (worker.joinable())
                    {
                        worker.join();
                    }
                }
                
                workers.clear();
                localQueues.clear();
                return;
            }
        }
        
        Logger::Log(LogLevel::Info, "JobScheduler: Successfully created %u worker threads", threadCount);
    }
    
    void JobScheduler::Shutdown()
    {
        // 이미 종료되었으면 무시
        if (!running.load(std::memory_order_acquire))
        {
            return;
        }
        
        Logger::Log(LogLevel::Info, "JobScheduler: Shutting down with %u worker threads", static_cast<uint32_t>(workers.size()));
        
        // 실행 플래그 해제
        running.store(false, std::memory_order_release);
        
        // 모든 워커 스레드 깨우기
        wakeCondition.notify_all();
        
        // 모든 워커 스레드 종료 대기
        for (uint32_t i = 0; i < workers.size(); ++i)
        {
            if (workers[i].joinable())
            {
                workers[i].join();
            }
            else
            {
                Logger::Log(LogLevel::Warning, "JobScheduler: Worker thread %u was not joinable during shutdown", i);
            }
        }
        
        workers.clear();
        localQueues.clear();
        
        Logger::Log(LogLevel::Info, "JobScheduler: All worker threads terminated successfully");
    }
    
    // ========================================
    // Job Management
    // ========================================
    
    void JobScheduler::EnqueueJob(Job* job)
    {
        if (!job)
        {
            // Requirement 6.2: Log error with context
            Logger::Log(LogLevel::Error, "JobScheduler: Cannot enqueue null job pointer");
            return;
        }
        
        if (!running.load(std::memory_order_acquire))
        {
            // Requirement 6.2: Log error with context
            Logger::Log(LogLevel::Error, "JobScheduler: Cannot enqueue job (id=%u, gen=%u) - Scheduler not running", 
                       job->handle.id, job->handle.generation);
            return;
        }
        
        // 첫 번째 워커의 큐에 추가 (간단한 전략)
        // 더 복잡한 로드 밸런싱은 나중에 추가 가능
        if (!localQueues.empty())
        {
            localQueues[0].Push(job);
            
            // 워커 스레드 깨우기
            wakeCondition.notify_one();
        }
        else
        {
            Logger::Log(LogLevel::Error, "JobScheduler: Cannot enqueue job (id=%u, gen=%u) - No worker queues available", 
                       job->handle.id, job->handle.generation);
        }
    }
    
    bool JobScheduler::IsJobComplete(Job* job) const
    {
        if (!job)
        {
            return true;
        }
        
        return job->IsComplete();
    }
    
    void JobScheduler::WaitForJob(Job* job)
    {
        if (!job)
        {
            return;
        }
        
        // Busy-wait with yielding
        while (!job->IsComplete())
        {
            std::this_thread::yield();
        }
    }
    
    uint32_t JobScheduler::GetWorkerThreadCount() const
    {
        return static_cast<uint32_t>(workers.size());
    }
    
    bool JobScheduler::IsRunning() const
    {
        return running.load(std::memory_order_acquire);
    }
    
    void JobScheduler::SetJobCompletionCallback(std::function<void(Job*)> callback)
    {
        completionCallback = callback;
    }
    
    // ========================================
    // Worker Thread
    // ========================================
    
    void JobScheduler::WorkerThreadMain(uint32_t threadIndex)
    {
        while (running.load(std::memory_order_acquire))
        {
            // Job 획득 시도
            Job* job = AcquireJob(threadIndex);
            
            if (job)
            {
                // 활성 워커 수 증가
                activeWorkers.fetch_add(1, std::memory_order_relaxed);
                
                try
                {
                    // Job 실행
                    job->Execute();
                }
                catch (const std::exception& e)
                {
                    // Requirement 6.2: Log runtime errors
                    Logger::Log(LogLevel::Error, "JobScheduler: Exception in worker thread %u executing job (id=%u, gen=%u) - %s", 
                               threadIndex, job->handle.id, job->handle.generation, e.what());
                    
                    // Mark job as complete to unblock dependents
                    job->completed.store(true, std::memory_order_release);
                }
                catch (...)
                {
                    // Requirement 6.2: Log unknown runtime errors
                    Logger::Log(LogLevel::Error, "JobScheduler: Unknown exception in worker thread %u executing job (id=%u, gen=%u)", 
                               threadIndex, job->handle.id, job->handle.generation);
                    
                    // Mark job as complete to unblock dependents
                    job->completed.store(true, std::memory_order_release);
                }
                
                // 완료 콜백 호출
                if (completionCallback)
                {
                    try
                    {
                        completionCallback(job);
                    }
                    catch (const std::exception& e)
                    {
                        Logger::Log(LogLevel::Error, "JobScheduler: Exception in completion callback for job (id=%u, gen=%u) - %s", 
                                   job->handle.id, job->handle.generation, e.what());
                    }
                }
                
                // 활성 워커 수 감소
                activeWorkers.fetch_sub(1, std::memory_order_relaxed);
            }
            else
            {
                // Job이 없으면 대기
                WaitForWork();
            }
        }
    }
    
    Job* JobScheduler::AcquireJob(uint32_t threadIndex)
    {
        // 로컬 큐에서 Pop 시도
        if (threadIndex < localQueues.size())
        {
            Job* job = localQueues[threadIndex].Pop();
            if (job)
            {
                return job;
            }
        }
        
        // 로컬 큐가 비었으면 다른 스레드에서 Steal 시도
        return StealJob(threadIndex);
    }
    
    Job* JobScheduler::StealJob(uint32_t thiefIndex)
    {
        // 모든 다른 큐에서 Steal 시도
        size_t numQueues = localQueues.size();
        
        for (size_t i = 0; i < numQueues; ++i)
        {
            // 자신의 큐는 건너뛰기
            if (i == thiefIndex)
            {
                continue;
            }
            
            Job* job = localQueues[i].Steal();
            if (job)
            {
                return job;
            }
        }
        
        return nullptr;
    }
    
    void JobScheduler::WaitForWork()
    {
        std::unique_lock<std::mutex> lock(wakeMutex);
        
        // 짧은 시간 대기 (spurious wakeup 방지)
        wakeCondition.wait_for(lock, std::chrono::milliseconds(1));
    }
    
    // ========================================
    // Helper Methods
    // ========================================
    
    uint32_t JobScheduler::DetermineThreadCount(uint32_t requestedThreads) const
    {
        uint32_t hardwareThreads = std::thread::hardware_concurrency();
        
        // 0이면 하드웨어 스레드 수 - 1 사용
        if (requestedThreads == 0)
        {
            return (hardwareThreads > 1) ? (hardwareThreads - 1) : 1;
        }
        
        // 하드웨어 스레드 수를 초과하면 조정
        if (requestedThreads > hardwareThreads)
        {
            return hardwareThreads;
        }
        
        // 최소 1개의 스레드는 필요
        return (requestedThreads > 0) ? requestedThreads : 1;
    }

} // namespace Engine
