#include "JobScheduler.h"
#include "../core/logging/Logger.h"
#include <thread>
#include <chrono>

namespace Engine
{
    // 현재 스레드가 어느 스케줄러의 몇 번 워커인지(워커가 아니면 nullptr). EnqueueJob 주석 참고.
    thread_local const JobScheduler* tl_currentScheduler = nullptr;
    thread_local uint32_t tl_currentWorkerIndex = 0;

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
        
        // 예전에는 어느 스레드에서든 localQueues[0].Push(job)를 했다. 당시 WorkStealingDeque는 락 없는 Chase-Lev
        // deque라 Push/Pop은 소유 워커만 해야 했다. 메인 스레드(Dispatch)와 워커(완료 콜백의 후행 잡 제출)가 동시에
        // Push하면 같은 칸에 써서 잡 하나가 사라질 수 있었고, 메인 스레드 Push와 워커 0의 Pop도 경쟁했다(코드 검토로
        // 확인한 경쟁). 지금은 이 스케줄러의 워커 스레드에서 제출하면 자기 deque에, 그 외 스레드는 주입 큐에 넣는다.
        // deque 자체도 락으로 보호한다(WorkStealingDeque.h).
        if (!localQueues.empty())
        {
            if (tl_currentScheduler == this && tl_currentWorkerIndex < localQueues.size())
            {
                localQueues[tl_currentWorkerIndex].Push(job);
            }
            else
            {
                std::lock_guard<std::mutex> lock(injectMutex);
                injectQueue.push_back(job);
            }

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
        
        // Busy-wait with yielding.
        // Job 객체는 풀에서 재사용된다. 끝나서 회수된 뒤 다른 잡으로 재사용되면 completed가 다시 false가 되므로,
        // 핸들이 바뀌었으면 원래 잡은 이미 끝난 것으로 본다(예전에는 재사용된 다른 잡을 기다릴 수 있었다).
        const JobHandle waitedHandle = job->handle;
        while (job->handle == waitedHandle && !job->IsComplete())
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
        // 이 스레드가 이 스케줄러의 몇 번 워커인지 기억한다(EnqueueJob이 소유자 Push를 판단하는 데 씀).
        // 스케줄러 포인터도 저장하는 이유: 한 프로세스에 Engine(JobSystem)이 여럿일 수 있다.
        tl_currentScheduler = this;
        tl_currentWorkerIndex = threadIndex;

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
        
        // 워커가 아닌 스레드가 넣은 잡
        {
            std::lock_guard<std::mutex> lock(injectMutex);
            if (!injectQueue.empty())
            {
                Job* job = injectQueue.front();
                injectQueue.pop_front();
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
