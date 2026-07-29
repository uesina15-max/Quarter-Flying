#include "RenderGraph.h"
#include "CommandList.h"
#include "../core/logging/Logger.h"
#include "../job/JobSystem.h"
#include <queue>
#include <algorithm>
#include <vector>
#include <string>
#include <unordered_map>

namespace Engine
{
    RenderGraph::RenderGraph()
        : compiled(false), nextResourceId(RGTextureHandle(1, 0))  // Start with id=1, generation=0
    {
    }

    RenderGraph::~RenderGraph()
    {
    }

    RGTextureHandle RenderGraph::CreateTexture(const std::string& name, const RGTextureDesc& desc)
    {
        // Check if texture with this name already exists
        auto it = resourceNameMap.find(name);
        if (it != resourceNameMap.end())
        {
            Logger::Log(LogLevel::Warning, ("RenderGraph::CreateTexture - Texture already exists: " + name).c_str());
            return it->second;
        }

        // Validate texture description
        if (desc.width == 0 || desc.height == 0)
        {
            Logger::Log(LogLevel::Fatal, ("RenderGraph::CreateTexture - Invalid texture dimensions: " + name).c_str());
            return RGTextureHandle();  // Return invalid handle
        }

        // Create new texture
        RGTextureHandle id = nextResourceId;
        nextResourceId = RGTextureHandle(nextResourceId.id + 1, nextResourceId.generation);
        RGTexture texture(id, desc, false, name);
        
        // Add to resources
        resources.push_back(texture);
        resourceNameMap[name] = id;
        
        // Initialize lifetime (will be updated when passes use this resource)
        lifetimes[id] = ResourceLifetime();

        Logger::Log(LogLevel::Info, ("RenderGraph::CreateTexture - Created texture: " + name + 
                                   " (" + std::to_string(desc.width) + "x" + std::to_string(desc.height) + ")").c_str());

        return id;
    }

    RGTextureHandle RenderGraph::ImportTexture(const std::string& name, GPUTextureHandle handle)
    {
        // Check if texture with this name already exists
        auto it = resourceNameMap.find(name);
        if (it != resourceNameMap.end())
        {
            Logger::Log(LogLevel::Warning, ("RenderGraph::ImportTexture - Texture already exists: " + name).c_str());
            return it->second;
        }

        // Validate GPU handle
        if (!handle.IsValid())
        {
            Logger::Log(LogLevel::Fatal, ("RenderGraph::ImportTexture - Invalid GPU handle for: " + name).c_str());
            return RGTextureHandle();  // Return invalid handle
        }

        // Create imported texture (desc will be filled later or queried from GPU)
        RGTextureHandle id = nextResourceId;
        nextResourceId = RGTextureHandle(nextResourceId.id + 1, nextResourceId.generation);
        RGTexture texture;
        texture.id = id;
        texture.imported = true;
        texture.name = name;
        // Note: desc is not set for imported textures as it should be queried from the actual GPU resource
        
        // Add to resources
        resources.push_back(texture);
        resourceNameMap[name] = id;
        
        // Initialize lifetime (will be updated when passes use this resource)
        lifetimes[id] = ResourceLifetime();

        Logger::Log(LogLevel::Info, ("RenderGraph::ImportTexture - Imported texture: " + name).c_str());

        return id;
    }

    void RenderGraph::AddPass(
        const std::string& name,
        std::function<void(RGBuilder&)> setup,
        std::function<void(CommandList&)> execute
    )
    {
        // Validate inputs
        if (name.empty())
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::AddPass - Pass name cannot be empty");
            return;
        }

        if (!setup)
        {
            Logger::Log(LogLevel::Fatal, ("RenderGraph::AddPass - Setup function is null for pass: " + name).c_str());
            return;
        }

        if (!execute)
        {
            Logger::Log(LogLevel::Fatal, ("RenderGraph::AddPass - Execute function is null for pass: " + name).c_str());
            return;
        }

        // Create new pass
        RGPass pass(name, static_cast<uint32_t>(passes.size()));
        pass.execute = execute;
        
        // Create builder and call setup to record read/write relationships
        RGBuilder builder(this, &pass);
        setup(builder);
        
        // Add pass to list
        passes.push_back(pass);
        
        // Mark as not compiled since we added a new pass
        compiled = false;

        Logger::Log(LogLevel::Info, ("RenderGraph::AddPass - Added pass: " + name + 
                                   " (reads: " + std::to_string(pass.reads.size()) + 
                                   ", writes: " + std::to_string(pass.writes.size()) + ")").c_str());
    }

    void RenderGraph::Compile()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::Compile - Starting compilation");

        if (passes.empty())
        {
            Logger::Log(LogLevel::Warning, "RenderGraph::Compile - No passes to compile");
            compiled = true;
            return;
        }

        // Clear previous execution plan
        executionPlan.Clear();

        // Step 1: Perform topological sort to determine pass execution order
        std::vector<uint32_t> passOrder = TopologicalSort();
        if (passOrder.empty() && !passes.empty())
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::Compile - Failed to compile due to cycle in pass dependencies");
            compiled = false;
            return;
        }

        // Store the pass order in execution plan
        executionPlan.passOrder = passOrder;

        // Step 2: Compute resource lifetimes
        ComputeResourceLifetimes();

        // Step 3: Compute aliasing (Task 12.3)
        ComputeAliasing();

        // Step 4: Insert barriers (Task 12.4)
        InsertBarriers();

        // Step 5: Create physical resources (Task 12.5)
        CreatePhysicalResources();

        compiled = true;
        Logger::Log(LogLevel::Info, ("RenderGraph::Compile - Successfully compiled " + std::to_string(passes.size()) + " passes").c_str());
    }

    void RenderGraph::Execute(CommandList& cmdList)
    {
        Logger::Log(LogLevel::Info, "RenderGraph::Execute - Starting execution");

        // Check if the graph has been compiled
        if (!compiled)
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::Execute - RenderGraph must be compiled before execution");
            return;
        }

        // Check if there are passes to execute
        if (executionPlan.passOrder.empty())
        {
            Logger::Log(LogLevel::Info, "RenderGraph::Execute - No passes to execute");
            return;
        }

        // Validate execution plan integrity
        for (uint32_t passId : executionPlan.passOrder)
        {
            if (passId >= passes.size())
            {
                Logger::Log(LogLevel::Fatal, ("RenderGraph::Execute - Invalid pass ID in execution plan: " + std::to_string(passId)).c_str());
                return;
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::Execute - Executing " + std::to_string(executionPlan.passOrder.size()) + " passes according to ExecutionPlan").c_str());

        // Execute barriers and passes in order
        size_t barrierIndex = 0;

        for (size_t passOrderIndex = 0; passOrderIndex < executionPlan.passOrder.size(); ++passOrderIndex)
        {
            // Insert barriers before this pass if needed
            while (barrierIndex < executionPlan.barriers.size() && 
                   executionPlan.barriers[barrierIndex].passIndex == passOrderIndex)
            {
                const Barrier& barrier = executionPlan.barriers[barrierIndex];
                
                // Find the resource name for logging
                std::string resourceName = "Unknown";
                for (const auto& resource : resources)
                {
                    if (resource.id == barrier.resource)
                    {
                        resourceName = resource.name;
                        break;
                    }
                }

                Logger::Log(LogLevel::Info, ("RenderGraph::Execute - Inserting barrier for resource '" + resourceName + 
                                           "' before pass " + std::to_string(passOrderIndex)).c_str());

                // Execute the barrier on the command list
                // For now, we'll use a placeholder GPU handle since we don't have actual GPU resource management
                GPUTextureHandle gpuHandle(barrier.resource.id, barrier.resource.generation);
                cmdList.ResourceBarrier(gpuHandle);

                barrierIndex++;
            }

            // Get the pass to execute
            uint32_t passId = executionPlan.passOrder[passOrderIndex];
            if (passId >= passes.size())
            {
                Logger::Log(LogLevel::Fatal, ("RenderGraph::Execute - Invalid pass ID: " + std::to_string(passId)).c_str());
                continue;
            }

            const RGPass& pass = passes[passId];

            Logger::Log(LogLevel::Info, ("RenderGraph::Execute - Executing pass: " + pass.name).c_str());

            // Begin debug event for the pass
            cmdList.BeginEvent(pass.name.c_str());

            // Execute the pass function with the command list
            if (pass.execute)
            {
                try
                {
                    pass.execute(cmdList);
                    Logger::Log(LogLevel::Info, ("RenderGraph::Execute - Pass '" + pass.name + "' executed successfully").c_str());
                }
                catch (const std::exception& e)
                {
                    Logger::Log(LogLevel::Fatal, ("RenderGraph::Execute - Pass '" + pass.name + "' failed with exception: " + e.what()).c_str());
                }
                catch (...)
                {
                    Logger::Log(LogLevel::Fatal, ("RenderGraph::Execute - Pass '" + pass.name + "' failed with unknown exception").c_str());
                }
            }
            else
            {
                Logger::Log(LogLevel::Warning, ("RenderGraph::Execute - Pass '" + pass.name + "' has no execute function").c_str());
            }

            // End debug event for the pass
            cmdList.EndEvent();
        }

        // Insert any remaining barriers
        while (barrierIndex < executionPlan.barriers.size())
        {
            const Barrier& barrier = executionPlan.barriers[barrierIndex];
            
            // Find the resource name for logging
            std::string resourceName = "Unknown";
            for (const auto& resource : resources)
            {
                if (resource.id == barrier.resource)
                {
                    resourceName = resource.name;
                    break;
                }
            }

            Logger::Log(LogLevel::Info, ("RenderGraph::Execute - Inserting final barrier for resource '" + resourceName + "'").c_str());

            // Execute the barrier on the command list
            GPUTextureHandle gpuHandle(barrier.resource.id, barrier.resource.generation);
            cmdList.ResourceBarrier(gpuHandle);

            barrierIndex++;
        }

        Logger::Log(LogLevel::Info, "RenderGraph::Execute - Execution completed successfully");
    }

    struct RenderGraph::PassJobData
    {
        RenderGraph* renderGraph;
        const RGPass* pass;
        CommandList* cmdList;
        uint32_t passId;
        
        PassJobData(RenderGraph* rg, const RGPass* p, CommandList* cl, uint32_t id)
            : renderGraph(rg), pass(p), cmdList(cl), passId(id) {}
    };

    std::vector<std::vector<uint32_t>> RenderGraph::BuildExecutionDependencyList() const
    {
        std::vector<std::vector<uint32_t>> passDependencies(passes.size());
        for (uint32_t i = 0; i < passes.size(); ++i)
        {
            const RGPass& currentPass = passes[i];
            
            for (RGTextureHandle readResource : currentPass.reads)
            {
                for (uint32_t j = 0; j < passes.size(); ++j)
                {
                    if (i == j) continue;
                    
                    const RGPass& otherPass = passes[j];
                    
                    for (RGTextureHandle writeResource : otherPass.writes)
                    {
                        if (writeResource == readResource)
                        {
                            auto currentIt = std::find(executionPlan.passOrder.begin(), executionPlan.passOrder.end(), i);
                            auto otherIt = std::find(executionPlan.passOrder.begin(), executionPlan.passOrder.end(), j);
                            
                            if (otherIt != executionPlan.passOrder.end() && currentIt != executionPlan.passOrder.end() && otherIt < currentIt)
                            {
                                passDependencies[i].push_back(j);
                                Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Pass " + std::to_string(i) + " (" + currentPass.name + ") depends on Pass " + std::to_string(j) + " (" + otherPass.name + ")").c_str());
                                break;
                            }
                        }
                    }
                }
            }
        }
        return passDependencies;
    }

    void RenderGraph::RunPassJob(void* data)
    {
        PassJobData* jobData = static_cast<PassJobData*>(data);
        const RGPass& pass = *jobData->pass;
        CommandList& cmdList = *jobData->cmdList;
        
        Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Executing pass job: " + pass.name).c_str());

        cmdList.BeginEvent(pass.name.c_str());

        if (pass.execute)
        {
            try
            {
                pass.execute(cmdList);
                Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Pass job '" + pass.name + "' executed successfully").c_str());
            }
            catch (const std::exception& e)
            {
                Logger::Log(LogLevel::Fatal, ("RenderGraph::ExecuteParallel - Pass job '" + pass.name + "' failed with exception: " + e.what()).c_str());
            }
            catch (...)
            {
                Logger::Log(LogLevel::Fatal, ("RenderGraph::ExecuteParallel - Pass job '" + pass.name + "' failed with unknown exception").c_str());
            }
        }
        else
        {
            Logger::Log(LogLevel::Warning, ("RenderGraph::ExecuteParallel - Pass job '" + pass.name + "' has no execute function").c_str());
        }

        cmdList.EndEvent();
    }

    void RenderGraph::ExecuteSinglePassJob(JobSystem* jobSystem, CommandList& cmdList, size_t orderIndex, const std::vector<std::vector<uint32_t>>& passDependencies, std::vector<JobHandle>& passJobs, std::vector<bool>& passDispatched)
    {
        uint32_t passId = executionPlan.passOrder[orderIndex];
        
        if (passDispatched[passId])
        {
            return;
        }

        const RGPass& pass = passes[passId];

        std::vector<JobHandle> dependencyHandles;
        for (uint32_t depPassId : passDependencies[passId])
        {
            if (passDispatched[depPassId])
            {
                dependencyHandles.push_back(passJobs[depPassId]);
            }
        }

        PassJobData* jobData = jobSystem->AllocateJobData<PassJobData>(this, &pass, &cmdList, passId);

        JobHandle jobHandle;
        if (dependencyHandles.empty())
        {
            jobHandle = jobSystem->Dispatch(RunPassJob, jobData);
            Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Dispatched independent pass job: " + pass.name).c_str());
        }
        else
        {
            jobHandle = jobSystem->Dispatch(RunPassJob, jobData, dependencyHandles.data(), static_cast<uint32_t>(dependencyHandles.size()));
            Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Dispatched dependent pass job: " + pass.name + " (depends on " + std::to_string(dependencyHandles.size()) + " passes)").c_str());
        }

        passJobs[passId] = jobHandle;
        passDispatched[passId] = true;
    }

    void RenderGraph::ExecuteParallel(CommandList& cmdList, JobSystem* jobSystem)
    {
        Logger::Log(LogLevel::Info, "RenderGraph::ExecuteParallel - Starting parallel execution with JobSystem");

        if (!jobSystem)
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::ExecuteParallel - JobSystem is null, falling back to sequential execution");
            Execute(cmdList);
            return;
        }

        if (!compiled)
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::ExecuteParallel - RenderGraph must be compiled before execution");
            return;
        }

        if (executionPlan.passOrder.empty())
        {
            Logger::Log(LogLevel::Info, "RenderGraph::ExecuteParallel - No passes to execute");
            return;
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Executing " + std::to_string(executionPlan.passOrder.size()) + " passes with parallel Job dispatch").c_str());

        std::vector<std::vector<uint32_t>> passDependencies = BuildExecutionDependencyList();
        std::vector<JobHandle> passJobs(passes.size());
        std::vector<bool> passDispatched(passes.size(), false);

        for (size_t orderIndex = 0; orderIndex < executionPlan.passOrder.size(); ++orderIndex)
        {
            ExecuteSinglePassJob(jobSystem, cmdList, orderIndex, passDependencies, passJobs, passDispatched);
        }

        Logger::Log(LogLevel::Info, "RenderGraph::ExecuteParallel - Waiting for all pass jobs to complete");
        for (size_t i = 0; i < passes.size(); ++i)
        {
            if (passDispatched[i])
            {
                jobSystem->Wait(passJobs[i]);
            }
        }

        Logger::Log(LogLevel::Info, "RenderGraph::ExecuteParallel - Inserting barriers after parallel execution");
        for (const Barrier& barrier : executionPlan.barriers)
        {
            std::string resourceName = "Unknown";
            for (const auto& resource : resources)
            {
                if (resource.id == barrier.resource)
                {
                    resourceName = resource.name;
                    break;
                }
            }

            Logger::Log(LogLevel::Info, ("RenderGraph::ExecuteParallel - Inserting barrier for resource '" + resourceName + "'").c_str());

            GPUTextureHandle gpuHandle(barrier.resource.id, barrier.resource.generation);
            cmdList.ResourceBarrier(gpuHandle);
        }

        Logger::Log(LogLevel::Info, "RenderGraph::ExecuteParallel - Parallel execution completed successfully");
    }

    void RenderGraph::Reset()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::Reset - Clearing render graph");
        
        passes.clear();
        resources.clear();
        resourceNameMap.clear();
        lifetimes.clear();
        aliasMap.clear();
        physicalResourceMap.clear();
        executionPlan.Clear();
        compiled = false;
        nextResourceId = RGTextureHandle(1, 0);  // Start from id=1, generation=0  // Start from 1, 0 is reserved for invalid handle
        
        Logger::Log(LogLevel::Info, "RenderGraph::Reset - Render graph cleared");
    }

    const RGTexture& RenderGraph::GetResource(RGTextureHandle handle) const
    {
        // Linear search through resources to find matching handle
        for (const auto& resource : resources)
        {
            if (resource.id == handle)
            {
                return resource;
            }
        }
        
        // Log error for invalid handle
        Logger::Log(LogLevel::Fatal, ("RenderGraph::GetResource - Invalid resource handle: " + std::to_string(handle.id)).c_str());
        
        // Return a static invalid texture as fallback
        static RGTexture invalidTexture;
        invalidTexture.id = RGTextureHandle();  // Invalid handle
        invalidTexture.name = "INVALID";
        return invalidTexture;
    }

    GPUTextureHandle RenderGraph::GetPhysicalResource(RGTextureHandle logicalHandle) const
    {
        if (!compiled)
        {
            Logger::Log(LogLevel::Warning, "RenderGraph::GetPhysicalResource - Graph must be compiled to access physical resources");
            return GPUTextureHandle();  // Invalid handle
        }

        // Find the logical resource
        const RGTexture* logicalResource = nullptr;
        for (const auto& resource : resources)
        {
            if (resource.id == logicalHandle)
            {
                logicalResource = &resource;
                break;
            }
        }

        if (!logicalResource)
        {
            Logger::Log(LogLevel::Warning, ("RenderGraph::GetPhysicalResource - Logical resource not found: " + std::to_string(logicalHandle.id)).c_str());
            return GPUTextureHandle();  // Invalid handle
        }

        // Check if resource is used (has lifetime)
        auto lifetimeIt = lifetimes.find(logicalHandle);
        if (lifetimeIt == lifetimes.end() || lifetimeIt->second.firstUse == UINT32_MAX)
        {
            Logger::Log(LogLevel::Warning, ("RenderGraph::GetPhysicalResource - Resource is never used: " + logicalResource->name).c_str());
            return GPUTextureHandle();  // Invalid handle
        }

        auto physicalIt = physicalResourceMap.find(logicalHandle);
        if (physicalIt != physicalResourceMap.end() && physicalIt->second < executionPlan.physicalResources.size())
        {
            return executionPlan.physicalResources[physicalIt->second].handle;
        }

        Logger::Log(LogLevel::Warning, ("RenderGraph::GetPhysicalResource - No physical resource found for: " + logicalResource->name).c_str());
        return GPUTextureHandle();  // Invalid handle
    }

    const PhysicalTexture& RenderGraph::GetPhysicalResourceByIndex(size_t index) const
    {
        if (index >= executionPlan.physicalResources.size())
        {
            Logger::Log(LogLevel::Fatal, ("RenderGraph::GetPhysicalResourceByIndex - Index out of range: " + std::to_string(index)).c_str());
            
            // Return a static invalid physical texture as fallback
            static PhysicalTexture invalidPhysical;
            invalidPhysical.handle = GPUTextureHandle();  // Invalid handle
            return invalidPhysical;
        }

        return executionPlan.physicalResources[index];
    }

    bool RenderGraph::IsResourceAliased(RGTextureHandle handle) const
    {
        return aliasMap.find(handle) != aliasMap.end();
    }

    RGTextureHandle RenderGraph::GetAliasSource(RGTextureHandle handle) const
    {
        auto it = aliasMap.find(handle);
        if (it != aliasMap.end())
        {
            return it->second;
        }
        
        // Return invalid handle if not aliased
        return RGTextureHandle();
    }

    // Internal helper method implementations
    void RenderGraph::BuildSortAdjacencyList(std::vector<std::vector<uint32_t>>& dependencies, std::vector<uint32_t>& inDegree) const
    {
        for (uint32_t i = 0; i < passes.size(); ++i)
        {
            const RGPass& currentPass = passes[i];
            
            for (RGTextureHandle readResource : currentPass.reads)
            {
                for (uint32_t j = 0; j < passes.size(); ++j)
                {
                    if (i == j) continue;
                    
                    const RGPass& otherPass = passes[j];
                    
                    for (RGTextureHandle writeResource : otherPass.writes)
                    {
                        if (writeResource == readResource)
                        {
                            dependencies[j].push_back(i);
                            inDegree[i]++;
                            Logger::Log(LogLevel::Info, ("RenderGraph::TopologicalSort - Pass " + std::to_string(i) + " depends on Pass " + std::to_string(j)).c_str());
                            break;
                        }
                    }
                }
            }
        }
    }

    bool RenderGraph::ValidateNoCycles(const std::vector<std::vector<uint32_t>>& dependencies)
    {
        std::vector<int> color(passes.size(), 0);
        for (uint32_t i = 0; i < passes.size(); ++i)
        {
            if (color[i] == 0)
            {
                if (DetectCyclesDFS(i, dependencies, color))
                {
                    Logger::Log(LogLevel::Fatal, "RenderGraph::TopologicalSort - Cycle detected in pass dependencies");
                    return false;
                }
            }
        }
        return true;
    }

    std::vector<uint32_t> RenderGraph::PerformKahnSort(const std::vector<std::vector<uint32_t>>& dependencies, std::vector<uint32_t> inDegree)
    {
        std::vector<uint32_t> result;
        std::queue<uint32_t> queue;

        for (uint32_t i = 0; i < passes.size(); ++i)
        {
            if (inDegree[i] == 0)
            {
                queue.push(i);
            }
        }

        while (!queue.empty())
        {
            uint32_t current = queue.front();
            queue.pop();
            result.push_back(current);

            for (uint32_t dependent : dependencies[current])
            {
                inDegree[dependent]--;
                if (inDegree[dependent] == 0)
                {
                    queue.push(dependent);
                }
            }
        }

        if (result.size() != passes.size())
        {
            Logger::Log(LogLevel::Fatal, "RenderGraph::TopologicalSort - Failed to sort all passes, cycle detected");
            return {};
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::TopologicalSort - Successfully sorted " + std::to_string(result.size()) + " passes").c_str());
        return result;
    }

    std::vector<uint32_t> RenderGraph::TopologicalSort()
    {
        if (passes.empty())
        {
            Logger::Log(LogLevel::Info, "RenderGraph::TopologicalSort - No passes to sort");
            return {};
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::TopologicalSort - Sorting " + std::to_string(passes.size()) + " passes").c_str());

        std::vector<std::vector<uint32_t>> dependencies(passes.size());
        std::vector<uint32_t> inDegree(passes.size(), 0);
        BuildSortAdjacencyList(dependencies, inDegree);

        if (!ValidateNoCycles(dependencies))
        {
            return {};
        }

        return PerformKahnSort(dependencies, std::move(inDegree));
    }

    bool RenderGraph::DetectCyclesDFS(uint32_t node, const std::vector<std::vector<uint32_t>>& dependencies, std::vector<int>& color)
    {
        color[node] = 1;  // Mark as gray (visiting)

        for (uint32_t neighbor : dependencies[node])
        {
            if (color[neighbor] == 1)  // Back edge found - cycle detected
            {
                Logger::Log(LogLevel::Error, ("RenderGraph::DetectCyclesDFS - Cycle detected involving pass " + std::to_string(node) + " -> " + std::to_string(neighbor)).c_str());
                return true;
            }
            if (color[neighbor] == 0 && DetectCyclesDFS(neighbor, dependencies, color))
            {
                return true;
            }
        }

        color[node] = 2;  // Mark as black (finished)
        return false;
    }

    void RenderGraph::ComputeResourceLifetimes()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::ComputeResourceLifetimes - Computing resource lifetimes");

        // Clear previous lifetime data
        lifetimes.clear();

        // Initialize all resource lifetimes
        for (const auto& resource : resources)
        {
            lifetimes[resource.id] = ResourceLifetime();
        }

        // Iterate through passes in execution order to compute lifetimes
        for (size_t passIndex = 0; passIndex < executionPlan.passOrder.size(); ++passIndex)
        {
            uint32_t passId = executionPlan.passOrder[passIndex];
            const RGPass& pass = passes[passId];

            // Process read resources
            for (RGTextureHandle readResource : pass.reads)
            {
                auto& lifetime = lifetimes[readResource];
                
                // Update first use if this is earlier
                if (lifetime.firstUse == UINT32_MAX)
                {
                    lifetime.firstUse = static_cast<uint32_t>(passIndex);
                }
                else
                {
                    lifetime.firstUse = std::min(lifetime.firstUse, static_cast<uint32_t>(passIndex));
                }
                
                // Update last use
                lifetime.lastUse = std::max(lifetime.lastUse, static_cast<uint32_t>(passIndex));
            }

            // Process write resources
            for (RGTextureHandle writeResource : pass.writes)
            {
                auto& lifetime = lifetimes[writeResource];
                
                // Update first use if this is earlier
                if (lifetime.firstUse == UINT32_MAX)
                {
                    lifetime.firstUse = static_cast<uint32_t>(passIndex);
                }
                else
                {
                    lifetime.firstUse = std::min(lifetime.firstUse, static_cast<uint32_t>(passIndex));
                }
                
                // Update last use
                lifetime.lastUse = std::max(lifetime.lastUse, static_cast<uint32_t>(passIndex));
            }
        }

        // Log computed lifetimes for debugging
        for (const auto& resource : resources)
        {
            const auto& lifetime = lifetimes[resource.id];
            if (lifetime.firstUse != UINT32_MAX)
            {
                Logger::Log(LogLevel::Info, ("RenderGraph::ComputeResourceLifetimes - Resource '" + resource.name + 
                                           "' lifetime: [" + std::to_string(lifetime.firstUse) + 
                                           ", " + std::to_string(lifetime.lastUse) + "]").c_str());
            }
            else
            {
                Logger::Log(LogLevel::Warning, ("RenderGraph::ComputeResourceLifetimes - Resource '" + resource.name + 
                                              "' is never used").c_str());
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::ComputeResourceLifetimes - Computed lifetimes for " + 
                                   std::to_string(resources.size()) + " resources").c_str());
    }

    void RenderGraph::ComputeAliasing()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::ComputeAliasing - Computing resource aliasing");

        // Clear previous aliasing data
        aliasMap.clear();

        // 수명이 겹치지 않는 리소스 쌍 찾기
        for (size_t i = 0; i < resources.size(); ++i)
        {
            for (size_t j = i + 1; j < resources.size(); ++j)
            {
                const RGTexture& resourceA = resources[i];
                const RGTexture& resourceB = resources[j];

                // Skip imported resources - they cannot be aliased
                if (resourceA.imported || resourceB.imported)
                {
                    continue;
                }

                auto lifetimeA = lifetimes.find(resourceA.id);
                auto lifetimeB = lifetimes.find(resourceB.id);

                // Skip resources that don't have lifetime information
                if (lifetimeA == lifetimes.end() || lifetimeB == lifetimes.end())
                {
                    continue;
                }

                // Skip unused resources
                if (lifetimeA->second.firstUse == UINT32_MAX || lifetimeB->second.firstUse == UINT32_MAX)
                {
                    continue;
                }

                // 수명이 겹치지 않으면 aliasing 가능
                bool lifetimesDoNotOverlap = 
                    (lifetimeA->second.lastUse < lifetimeB->second.firstUse) ||
                    (lifetimeB->second.lastUse < lifetimeA->second.firstUse);

                if (lifetimesDoNotOverlap)
                {
                    // 같은 크기/포맷이면 메모리 공유
                    if (CanAlias(resourceA, resourceB))
                    {
                        // B를 A에 alias (A가 먼저 사용되는 경우)
                        if (lifetimeA->second.firstUse < lifetimeB->second.firstUse)
                        {
                            aliasMap[resourceB.id] = resourceA.id;
                            Logger::Log(LogLevel::Info, ("RenderGraph::ComputeAliasing - Aliasing '" + resourceB.name + 
                                                       "' to '" + resourceA.name + "'").c_str());
                        }
                        else
                        {
                            aliasMap[resourceA.id] = resourceB.id;
                            Logger::Log(LogLevel::Info, ("RenderGraph::ComputeAliasing - Aliasing '" + resourceA.name + 
                                                       "' to '" + resourceB.name + "'").c_str());
                        }
                    }
                }
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::ComputeAliasing - Found " + std::to_string(aliasMap.size()) + 
                                   " aliasing opportunities").c_str());
    }

    void RenderGraph::ResolveReadAfterWriteBarriersForPass(
        const RGPass& pass,
        size_t passOrderIndex,
        const std::unordered_map<RGTextureHandle, uint32_t>& lastWritePass)
    {
        for (RGTextureHandle readResource : pass.reads)
        {
            auto lastWriteIt = lastWritePass.find(readResource);
            if (lastWriteIt != lastWritePass.end())
            {
                uint32_t lastWritePassId = lastWriteIt->second;
                
                uint32_t lastWriteOrderIndex = UINT32_MAX;
                for (size_t i = 0; i < passOrderIndex; ++i)
                {
                    if (executionPlan.passOrder[i] == lastWritePassId)
                    {
                        lastWriteOrderIndex = static_cast<uint32_t>(i);
                        break;
                    }
                }

                if (lastWriteOrderIndex != UINT32_MAX && lastWriteOrderIndex < passOrderIndex)
                {
                    Barrier barrier(readResource, static_cast<uint32_t>(passOrderIndex));
                    executionPlan.barriers.push_back(barrier);

                    Logger::Log(LogLevel::Info, ("RenderGraph::InsertBarriers - Inserted barrier for resource " + 
                                               std::to_string(readResource.id) + " before pass " + 
                                               std::to_string(passOrderIndex) + " (read after write)").c_str());
                }
            }
        }
    }

    void RenderGraph::ResolveWriteAfterAccessBarriersForPass(
        const RGPass& pass,
        size_t passOrderIndex,
        std::unordered_map<RGTextureHandle, uint32_t>& lastWritePass)
    {
        uint32_t passId = executionPlan.passOrder[passOrderIndex];
        for (RGTextureHandle writeResource : pass.writes)
        {
            bool needsBarrier = false;
            
            for (size_t prevPassOrderIndex = 0; prevPassOrderIndex < passOrderIndex; ++prevPassOrderIndex)
            {
                uint32_t prevPassId = executionPlan.passOrder[prevPassOrderIndex];
                const RGPass& prevPass = passes[prevPassId];
                
                for (RGTextureHandle prevReadResource : prevPass.reads)
                {
                    if (prevReadResource == writeResource)
                    {
                        needsBarrier = true;
                        break;
                    }
                }
                
                if (!needsBarrier)
                {
                    for (RGTextureHandle prevWriteResource : prevPass.writes)
                    {
                        if (prevWriteResource == writeResource)
                        {
                            needsBarrier = true;
                            break;
                        }
                    }
                }
                
                if (needsBarrier) break;
            }

            if (needsBarrier)
            {
                Barrier barrier(writeResource, static_cast<uint32_t>(passOrderIndex));
                executionPlan.barriers.push_back(barrier);

                Logger::Log(LogLevel::Info, ("RenderGraph::InsertBarriers - Inserted barrier for resource " + 
                                           std::to_string(writeResource.id) + " before pass " + 
                                           std::to_string(passOrderIndex) + " (write after access)").c_str());
            }

            lastWritePass[writeResource] = passId;
        }
    }

    void RenderGraph::InsertBarriers()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::InsertBarriers - Inserting resource barriers");

        executionPlan.barriers.clear();

        std::unordered_map<RGTextureHandle, uint32_t> lastWritePass;

        for (size_t passOrderIndex = 0; passOrderIndex < executionPlan.passOrder.size(); ++passOrderIndex)
        {
            uint32_t passId = executionPlan.passOrder[passOrderIndex];
            const RGPass& pass = passes[passId];

            ResolveReadAfterWriteBarriersForPass(pass, passOrderIndex, lastWritePass);
            ResolveWriteAfterAccessBarriersForPass(pass, passOrderIndex, lastWritePass);
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::InsertBarriers - Inserted " + 
                                   std::to_string(executionPlan.barriers.size()) + " barriers").c_str());
    }

    void RenderGraph::CreatePhysicalResources()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::CreatePhysicalResources - Creating physical GPU resources");

        // Clear previous physical resources
        executionPlan.physicalResources.clear();
        physicalResourceMap.clear();

        // First pass: Create physical resources for non-aliased resources
        for (const auto& resource : resources)
        {
            // Skip resources that are never used
            auto lifetimeIt = lifetimes.find(resource.id);
            if (lifetimeIt == lifetimes.end() || lifetimeIt->second.firstUse == UINT32_MAX)
            {
                Logger::Log(LogLevel::Info, ("RenderGraph::CreatePhysicalResources - Skipping unused resource: " + resource.name).c_str());
                continue;
            }

            // Skip aliased resources in first pass
            auto aliasIt = aliasMap.find(resource.id);
            if (aliasIt != aliasMap.end())
            {
                continue;  // Handle aliased resources in second pass
            }

            // Create new physical resource for non-aliased resources
            PhysicalTexture physicalResource;
            
            if (resource.imported)
            {
                // For imported resources, we use the existing GPU handle
                // In a real implementation, this would be the actual imported handle
                physicalResource.handle = GPUTextureHandle(resource.id.id, resource.id.generation);
                Logger::Log(LogLevel::Info, ("RenderGraph::CreatePhysicalResources - Using imported resource: " + resource.name + 
                                           " (handle: " + std::to_string(physicalResource.handle.id) + ")").c_str());
            }
            else
            {
                // Create new GPU resource with proper resource description
                // In a real implementation, this would call GPU API to create texture
                uint32_t newResourceId = static_cast<uint32_t>(executionPlan.physicalResources.size() + 1000);  // Offset to distinguish from logical IDs
                physicalResource.handle = GPUTextureHandle(newResourceId, 0);
                
                Logger::Log(LogLevel::Info, ("RenderGraph::CreatePhysicalResources - Created new GPU resource for: " + resource.name + 
                                           " (" + std::to_string(resource.desc.width) + "x" + std::to_string(resource.desc.height) + 
                                           ", format: " + std::to_string(static_cast<int>(resource.desc.format)) + 
                                           ", handle: " + std::to_string(physicalResource.handle.id) + ")").c_str());
            }
            
            // Set lifetime information
            const ResourceLifetime& lifetime = lifetimeIt->second;
            physicalResource.firstUse = lifetime.firstUse;
            physicalResource.lastUse = lifetime.lastUse;
            physicalResource.aliasedFrom = 0;  // Not aliased
            
            // Add to execution plan
            uint32_t physicalIndex = static_cast<uint32_t>(executionPlan.physicalResources.size());
            executionPlan.physicalResources.push_back(physicalResource);
            physicalResourceMap[resource.id] = physicalIndex;
        }

        // Second pass: Handle aliased resources
        for (const auto& resource : resources)
        {
            // Skip resources that are never used
            auto lifetimeIt = lifetimes.find(resource.id);
            if (lifetimeIt == lifetimes.end() || lifetimeIt->second.firstUse == UINT32_MAX)
            {
                continue;
            }

            // Only process aliased resources
            auto aliasIt = aliasMap.find(resource.id);
            if (aliasIt == aliasMap.end())
            {
                continue;  // Already handled in first pass
            }

            RGTextureHandle aliasedFrom = aliasIt->second;
            
            // Find the physical resource index of the aliased resource
            auto physicalIt = physicalResourceMap.find(aliasedFrom);
            if (physicalIt != physicalResourceMap.end())
            {
                // Reuse the same physical resource
                uint32_t physicalIndex = physicalIt->second;
                physicalResourceMap[resource.id] = physicalIndex;
                
                // Update the physical resource's usage range to include this resource's lifetime
                PhysicalTexture& physicalResource = executionPlan.physicalResources[physicalIndex];
                const ResourceLifetime& lifetime = lifetimeIt->second;
                
                // Extend the physical resource's lifetime to cover both resources
                physicalResource.firstUse = std::min(physicalResource.firstUse, lifetime.firstUse);
                physicalResource.lastUse = std::max(physicalResource.lastUse, lifetime.lastUse);
                
                Logger::Log(LogLevel::Info, ("RenderGraph::CreatePhysicalResources - Aliasing resource '" + resource.name + 
                                           "' to physical resource " + std::to_string(physicalIndex) + 
                                           " (handle: " + std::to_string(physicalResource.handle.id) + 
                                           ", extended lifetime: [" + std::to_string(physicalResource.firstUse) + 
                                           ", " + std::to_string(physicalResource.lastUse) + "])").c_str());
            }
            else
            {
                Logger::Log(LogLevel::Warning, ("RenderGraph::CreatePhysicalResources - Aliased resource not found for: " + resource.name + 
                                              ", creating new physical resource instead").c_str());
                
                // Create a new physical resource as fallback
                PhysicalTexture physicalResource;
                
                if (resource.imported)
                {
                    physicalResource.handle = GPUTextureHandle(resource.id.id, resource.id.generation);
                }
                else
                {
                    uint32_t newResourceId = static_cast<uint32_t>(executionPlan.physicalResources.size() + 1000);
                    physicalResource.handle = GPUTextureHandle(newResourceId, 0);
                }
                
                const ResourceLifetime& lifetime = lifetimeIt->second;
                physicalResource.firstUse = lifetime.firstUse;
                physicalResource.lastUse = lifetime.lastUse;
                physicalResource.aliasedFrom = 0;  // Not aliased (fallback)
                
                uint32_t physicalIndex = static_cast<uint32_t>(executionPlan.physicalResources.size());
                executionPlan.physicalResources.push_back(physicalResource);
                physicalResourceMap[resource.id] = physicalIndex;
            }
        }

        // Validate that all used resources have physical mappings
        for (const auto& resource : resources)
        {
            auto lifetimeIt = lifetimes.find(resource.id);
            if (lifetimeIt != lifetimes.end() && lifetimeIt->second.firstUse != UINT32_MAX)
            {
                auto physicalIt = physicalResourceMap.find(resource.id);
                if (physicalIt == physicalResourceMap.end())
                {
                    Logger::Log(LogLevel::Fatal, ("RenderGraph::CreatePhysicalResources - Failed to create physical mapping for resource: " + resource.name).c_str());
                }
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::CreatePhysicalResources - Successfully created " + 
                                   std::to_string(executionPlan.physicalResources.size()) + " physical resources for " + 
                                   std::to_string(physicalResourceMap.size()) + " logical resources").c_str());
    }

    void RenderGraph::ReleaseTransientResources()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::ReleaseTransientResources - Releasing transient GPU resources");

        if (!compiled)
        {
            Logger::Log(LogLevel::Warning, "RenderGraph::ReleaseTransientResources - Graph not compiled, no resources to release");
            return;
        }

        size_t releasedCount = 0;

        // Release all non-imported physical resources
        for (auto& physicalResource : executionPlan.physicalResources)
        {
            if (physicalResource.handle.IsValid())
            {
                // Find the corresponding logical resource to check if it's imported
                bool isImported = false;
                for (const auto& logicalResource : resources)
                {
                    if (logicalResource.imported)
                    {
                        // Check if this physical resource corresponds to the imported logical resource
                        // In a real implementation, we would have a direct mapping
                        auto lifetimeIt = lifetimes.find(logicalResource.id);
                        if (lifetimeIt != lifetimes.end() && 
                            physicalResource.firstUse <= lifetimeIt->second.firstUse && 
                            physicalResource.lastUse >= lifetimeIt->second.lastUse)
                        {
                            isImported = true;
                            break;
                        }
                    }
                }

                if (!isImported)
                {
                    // Release transient (non-imported) resource
                    Logger::Log(LogLevel::Info, ("RenderGraph::ReleaseTransientResources - Releasing GPU resource (handle: " + 
                                               std::to_string(physicalResource.handle.id) + ")").c_str());
                    
                    // In a real implementation, this would call GPU API to release the resource
                    // For now, we'll invalidate the handle to indicate it's been released
                    physicalResource.handle = GPUTextureHandle();  // Mark as released
                    releasedCount++;
                }
                else
                {
                    Logger::Log(LogLevel::Info, ("RenderGraph::ReleaseTransientResources - Keeping imported resource (handle: " + 
                                               std::to_string(physicalResource.handle.id) + ")").c_str());
                }
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::ReleaseTransientResources - Released " + std::to_string(releasedCount) + 
                                   " transient resources").c_str());
    }

    void RenderGraph::ReturnResourcesToPool()
    {
        Logger::Log(LogLevel::Info, "RenderGraph::ReturnResourcesToPool - Returning resources to resource pool");

        if (!compiled)
        {
            Logger::Log(LogLevel::Warning, "RenderGraph::ReturnResourcesToPool - Graph not compiled, no resources to return");
            return;
        }

        size_t returnedCount = 0;

        // Return all transient physical resources to the pool
        for (const auto& physicalResource : executionPlan.physicalResources)
        {
            if (physicalResource.handle.IsValid() && physicalResource.aliasedFrom == 0)
            {
                // This is a non-aliased resource that can be returned to the pool
                Logger::Log(LogLevel::Info, ("RenderGraph::ReturnResourcesToPool - Returning resource to pool (handle: " + 
                                           std::to_string(physicalResource.handle.id) + 
                                           ", lifetime: [" + std::to_string(physicalResource.firstUse) + 
                                           ", " + std::to_string(physicalResource.lastUse) + "])").c_str());
                
                // In a real implementation, this would return the resource to a resource pool
                // The pool would manage resource reuse across frames
                returnedCount++;
            }
        }

        Logger::Log(LogLevel::Info, ("RenderGraph::ReturnResourcesToPool - Returned " + std::to_string(returnedCount) + 
                                   " resources to pool").c_str());
    }

    bool RenderGraph::DetectCycles()
    {
        // This method is kept for backward compatibility but actual cycle detection
        // is now integrated into TopologicalSort for efficiency
        std::vector<uint32_t> sorted = TopologicalSort();
        return sorted.empty() && !passes.empty();
    }

    bool RenderGraph::CanAlias(const RGTexture& a, const RGTexture& b) const
    {
        // Check if two textures can share the same physical memory
        
        // Must have same dimensions
        if (a.desc.width != b.desc.width || a.desc.height != b.desc.height)
        {
            return false;
        }

        // Must have same format
        if (a.desc.format != b.desc.format)
        {
            return false;
        }

        // Must have compatible usage flags
        // For simplicity, require exact usage match for now
        // In a real implementation, we might allow more flexible usage compatibility
        if (a.desc.usage != b.desc.usage)
        {
            return false;
        }

        return true;
    }

} // namespace Engine
