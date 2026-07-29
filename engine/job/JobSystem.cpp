#include "JobSystem.h"
#include "../core/logging/Logger.h"
#include <algorithm>
#include <vector>
#include <memory>

namespace Engine
{
    // ========================================
    // Lifecycle Management
    // ========================================

    JobSystem::JobSystem()
        : lifecycleManager(std::make_unique<JobLifecycleManager>())
        , dependencyResolver(nullptr)  // Created after lifecycleManager
        , scheduler(std::make_unique<JobScheduler>())
        , initialized(false)
        , frameAllocator(nullptr)
    {
        // DependencyResolver needs lifecycleManager, so create it after
        dependencyResolver = std::make_unique<DependencyResolver>(lifecycleManager.get());
    }

    JobSystem::~JobSystem()
    {
        if (initialized)
        {
            Shutdown();
        }
    }

    // ========================================
    // System Initialization and Shutdown
    // ========================================

    void JobSystem::Initialize(uint32_t numThreads)
    {
        // Validate initialization parameters
        auto validation = ValidationLayer::ValidateInitialization(numThreads, initialized.load());
        
        if (!validation.isValid)
        {
            Logger::Log(LogLevel::Warning, validation.errorMessage);
            return;
        }

        numThreads = validation.adjustedThreadCount;
        Logger::Log(LogLevel::Info, "Initializing JobSystem with %u worker threads", numThreads);

        // Initialize scheduler with job completion callback
        // The callback is invoked when a job finishes, allowing us to resolve dependencies
        scheduler->SetJobCompletionCallback([this](Job* job) {
            OnJobComplete(job);
        });
        
        scheduler->Initialize(numThreads);

        initialized = true;
        Logger::Log(LogLevel::Info, "JobSystem initialized successfully");
    }

    void JobSystem::Shutdown()
    {
        if (!initialized)
        {
            return;
        }

        Logger::Log(LogLevel::Info, "Shutting down JobSystem...");

        // Shutdown scheduler (stops worker threads and waits for completion)
        scheduler->Shutdown();

        // Clear all jobs from lifecycle manager
        lifecycleManager->Clear();

        initialized = false;
        Logger::Log(LogLevel::Info, "JobSystem shutdown complete");
    }

    // ========================================
    // Job Dispatch and Execution
    // ========================================

    JobHandle JobSystem::Dispatch(
        JobFunction func,
        void* data,
        const JobHandle* dependencies,
        uint32_t numDependencies)
    {
        // Requirement 5.1, 5.3: Validate dispatch parameters
        auto validation = ValidationLayer::ValidateDispatch(
            func, 
            initialized.load(), 
            scheduler->IsRunning()
        );
        
        if (!validation.isValid)
        {
            // Requirement 6.2, 6.5: Log error and return invalid handle
            Logger::Log(LogLevel::Error, validation.errorMessage);
            return JobHandle();
        }

        // Requirement 5.2: Validate dependencies if provided
        if (numDependencies > 0)
        {
            auto depValidation = ValidationLayer::ValidateDependencies(dependencies, numDependencies);
            if (!depValidation.isValid)
            {
                // Requirement 6.2, 6.5: Log error and return invalid handle
                Logger::Log(LogLevel::Error, depValidation.errorMessage);
                return JobHandle();
            }
        }

        // Requirement 12.1: Handle job allocation failures
        Job* job = nullptr;
        try
        {
            // Create job using lifecycle manager
            job = lifecycleManager->CreateJob(func, data, numDependencies);
        }
        catch (const std::bad_alloc&)
        {
            // Requirement 6.2, 6.5: Log error and return invalid handle
            Logger::Log(LogLevel::Error, "Failed to allocate job - out of memory");
            return JobHandle();
        }
        
        // Setup dependencies if provided
        // This builds the dependency graph and checks for circular dependencies
        if (numDependencies > 0 && dependencies != nullptr)
        {
            if (!dependencyResolver->SetupDependencies(job, dependencies, numDependencies))
            {
                // Requirement 12.2: Clean up failed job on circular dependency
                lifecycleManager->CleanupFailedJob(job);
                
                // Requirement 6.3, 6.5: Log error and return invalid handle
                Logger::Log(LogLevel::Fatal, "Circular dependency detected in Job graph!");
                return JobHandle();
            }
        }

        // If no unfinished dependencies, enqueue immediately for execution
        // Otherwise, the job will be enqueued when its dependencies complete
        if (!dependencyResolver->HasUnfinishedDependencies(job))
        {
            scheduler->EnqueueJob(job);
        }

        // Requirement 10.2: Return valid handle (backward compatible behavior)
        return job->handle;
    }

    // ========================================
    // Job Status and Synchronization
    // ========================================

    void JobSystem::Wait(JobHandle handle)
    {
        // Requirement 5.5: Validate wait parameters
        auto validation = ValidationLayer::ValidateWait(handle);
        
        if (!validation.isValid)
        {
            // Requirement 12.4: Handle invalid handles gracefully without crashing
            Logger::Log(LogLevel::Warning, validation.errorMessage);
            return;
        }

        // Requirement 10.1, 10.3: Find job using lifecycle manager (backward compatible)
        Job* job = lifecycleManager->FindJob(handle);
        if (!job)
        {
            // Job not found (may have already completed and been retired)
            // Requirement 12.4: Return gracefully without crashing
            return;
        }

        // Requirement 10.3: Wait for job completion using scheduler (backward compatible behavior)
        scheduler->WaitForJob(job);
    }

    bool JobSystem::IsComplete(JobHandle handle)
    {
        // Requirement 10.1: Maintain backward compatible behavior
        // Invalid handles are considered complete (same as before refactoring)
        if (!handle.IsValid())
        {
            return true;
        }

        // Requirement 10.1, 10.3: Find job using lifecycle manager (backward compatible)
        Job* job = lifecycleManager->FindJob(handle);
        if (!job)
        {
            // Job not found (may have already completed and been retired)
            // Requirement 10.3: Return true for completed/retired jobs (backward compatible behavior)
            return true;
        }

        // Requirement 10.1, 10.3: Check completion status using scheduler (backward compatible behavior)
        return scheduler->IsJobComplete(job);
    }

    // ========================================
    // Configuration and Accessors
    // ========================================

    void JobSystem::SetFrameAllocator(FrameAllocator* allocator)
    {
        frameAllocator = allocator;
    }

    uint32_t JobSystem::GetWorkerThreadCount() const
    {
        return scheduler->GetWorkerThreadCount();
    }

    // ========================================
    // Private Methods - Job Completion Handling
    // ========================================

    void JobSystem::OnJobComplete(Job* job)
    {
        // Resolve dependents using dependency resolver
        // This decrements dependency counters and returns jobs that are now ready to execute
        std::vector<Job*> readyJobs = dependencyResolver->ResolveDependents(job);

        // Enqueue all ready jobs to the scheduler for execution
        for (Job* readyJob : readyJobs)
        {
            scheduler->EnqueueJob(readyJob);
        }

        // Retire job using lifecycle manager (returns it to the pool for reuse)
        lifecycleManager->RetireJob(job);
    }

} // namespace Engine
