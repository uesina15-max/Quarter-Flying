#include "SystemDispatcher.h"
#include "../job/JobSystem.h"
#include "../core/logging/Logger.h"
#include <algorithm>

namespace Engine
{
    namespace
    {
        // System Context 구조체 정의
        struct SystemContext
        {
            System* system;
            ECSRegistry* registry;
            float deltaTime;
            
            SystemContext(System* sys, ECSRegistry* reg, float dt)
                : system(sys), registry(reg), deltaTime(dt) {}
        };

        // 청크 기반 System Context 구조체 정의
        struct ChunkedSystemContext
        {
            System* system;
            ECSRegistry* registry;
            float deltaTime;
            size_t chunkStart;
            size_t chunkSize;
            
            ChunkedSystemContext(System* sys, ECSRegistry* reg, float dt, size_t start, size_t size)
                : system(sys), registry(reg), deltaTime(dt), chunkStart(start), chunkSize(size) {}
        };

        size_t CalculateAndPrepareChunking(System* system, JobSystem* jobSystem, ECSRegistry& registry, float deltaTime, size_t totalEntities)
        {
            size_t workerThreadCount = jobSystem->GetWorkerThreadCount();
            size_t dynamicChunkSize = system->CalculateDynamicChunkSize(totalEntities, workerThreadCount);
            size_t chunkSize = std::min(dynamicChunkSize, system->GetOptimalChunkSize());
            
            Logger::Debug("Executing system '{}' with chunked processing: {} entities, chunk size {} (dynamic: {}, optimal: {})", 
                         system->GetName(), totalEntities, chunkSize, dynamicChunkSize, system->GetOptimalChunkSize());
            
            system->PrepareChunkedProcessing(registry, deltaTime);
            return chunkSize;
        }

        void DispatchChunkJobsLoop(System* system, JobSystem* jobSystem, ECSRegistry& registry, float deltaTime, 
                                   size_t totalEntities, size_t chunkSize, bool requiresSync, std::vector<JobHandle>& outChunkJobs)
        {
            for (size_t start = 0; start < totalEntities; start += chunkSize)
            {
                size_t actualChunkSize = std::min(chunkSize, totalEntities - start);
                
                ChunkedSystemContext context(system, &registry, deltaTime, start, actualChunkSize);
                ChunkedSystemContext* contextPtr = jobSystem->AllocateJobData<ChunkedSystemContext>(context);

                JobHandle job = jobSystem->Dispatch(
                    [](void* data) {
                        ChunkedSystemContext* ctx = static_cast<ChunkedSystemContext*>(data);
                        Logger::Debug("Executing chunked system: '{}' (chunk {}-{})", 
                                     ctx->system->GetName(), ctx->chunkStart, ctx->chunkStart + ctx->chunkSize - 1);
                        ctx->system->UpdateChunk(*ctx->registry, ctx->deltaTime, ctx->chunkStart, ctx->chunkSize);
                    },
                    contextPtr
                );

                if (job.IsValid())
                {
                    outChunkJobs.push_back(job);
                    
                    if (requiresSync)
                    {
                        jobSystem->Wait(job);
                        Logger::Debug("Synchronized after chunk {}-{} for system '{}'", 
                                     start, start + actualChunkSize - 1, system->GetName());
                    }
                }
                else
                {
                    Logger::Error("Failed to dispatch chunked job for system: {}", system->GetName());
                    system->Update(registry, deltaTime);
                    break;
                }
            }
        }

        void WaitForChunksAndFinalize(System* system, JobSystem* jobSystem, ECSRegistry& registry, float deltaTime, 
                                      const std::vector<JobHandle>& chunkJobs, bool requiresSync)
        {
            if (!requiresSync)
            {
                for (JobHandle job : chunkJobs)
                {
                    jobSystem->Wait(job);
                }
            }
            
            system->FinalizeChunkedProcessing(registry, deltaTime);
            
            Logger::Debug("Completed chunked processing for system '{}' with {} chunks", 
                         system->GetName(), chunkJobs.size());
        }

        void DispatchChunkedSystemJobs(System* system, JobSystem* jobSystem, ECSRegistry& registry, float deltaTime, std::vector<JobHandle>& outJobs)
        {
            size_t totalEntities = registry.GetEntityCount();
            
            if (totalEntities > 0)
            {
                size_t chunkSize = CalculateAndPrepareChunking(system, jobSystem, registry, deltaTime, totalEntities);
                
                std::vector<JobHandle> chunkJobs;
                bool requiresSync = system->RequiresChunkSynchronization();
                
                DispatchChunkJobsLoop(system, jobSystem, registry, deltaTime, totalEntities, chunkSize, requiresSync, chunkJobs);
                WaitForChunksAndFinalize(system, jobSystem, registry, deltaTime, chunkJobs, requiresSync);
            }
            else
            {
                Logger::Debug("No entities found, calling regular update for system: {}", system->GetName());
                system->Update(registry, deltaTime);
            }
        }

        void DispatchSingleSystemJob(System* system, JobSystem* jobSystem, ECSRegistry& registry, float deltaTime, std::vector<JobHandle>& outJobs)
        {
            SystemContext context(system, &registry, deltaTime);
            SystemContext* contextPtr = jobSystem->AllocateJobData<SystemContext>(context);

            JobHandle job = jobSystem->Dispatch(
                [](void* data) {
                    SystemContext* ctx = static_cast<SystemContext*>(data);
                    Logger::Debug("Executing system: {}", ctx->system->GetName());
                    ctx->system->Update(*ctx->registry, ctx->deltaTime);
                },
                contextPtr
            );

            if (job.IsValid())
            {
                outJobs.push_back(job);
            }
            else
            {
                Logger::Error("Failed to dispatch job for system: {}", system->GetName());
                system->Update(registry, deltaTime);
            }
        }
    }

    void SystemDispatcher::Dispatch(const std::vector<std::vector<System*>>& parallelGroups,
                                    JobSystem* jobSystem,
                                    ECSRegistry& registry,
                                    float deltaTime)
    {
        if (!jobSystem)
        {
            // Sequential fallback if jobSystem is absent
            for (const auto& group : parallelGroups)
            {
                for (System* system : group)
                {
                    system->Update(registry, deltaTime);
                }
            }
            return;
        }

        Logger::Debug("Executing {} parallel system groups", parallelGroups.size());

        for (size_t groupIndex = 0; groupIndex < parallelGroups.size(); ++groupIndex)
        {
            const auto& group = parallelGroups[groupIndex];
            Logger::Debug("Executing parallel group {} with {} systems", groupIndex, group.size());
            
            std::vector<JobHandle> groupJobs;
            groupJobs.reserve(group.size() * 4);

            for (System* system : group)
            {
                if (system->SupportsChunkedProcessing())
                {
                    DispatchChunkedSystemJobs(system, jobSystem, registry, deltaTime, groupJobs);
                }
                else
                {
                    DispatchSingleSystemJob(system, jobSystem, registry, deltaTime, groupJobs);
                }
            }

            for (JobHandle job : groupJobs)
            {
                jobSystem->Wait(job);
            }
            
            Logger::Debug("Completed parallel group {}", groupIndex);
        }
        
        Logger::Debug("All parallel system groups completed");
    }
}
