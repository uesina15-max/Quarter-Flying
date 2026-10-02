#pragma once

#include "../core/Types.h"
#include "CommandList.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>

namespace Engine
{
    // Forward declarations
    class RGBuilder;

    // ========================================
    // RenderGraph Resource Types & Handle Tags
    // ========================================
    //
    // ARCHITECTURAL INVARIANT (RenderGraph Virtual Domain):
    // RGTextureHandle represents a virtual render graph resource during compilation and pass recording.
    // It is strictly distinct from physical backend allocations (GPUTextureHandle) and logical assets (TextureHandle).
    // Domain conversion occurs only during physical resource materialization (CreatePhysicalResources).

    struct RGTextureHandleTag {};
    using RGTextureHandle = Handle<RGTextureHandleTag>;

    struct RGTextureDesc
    {
        uint32_t width;
        uint32_t height;
        TextureFormat format;
        TextureUsage usage;

        RGTextureDesc() 
            : width(0), height(0), format(TextureFormat::RGBA8), usage(TextureUsage::ShaderResource) {}

        RGTextureDesc(uint32_t w, uint32_t h, TextureFormat fmt, TextureUsage use)
            : width(w), height(h), format(fmt), usage(use) {}
    };

    struct RGTexture
    {
        RGTextureHandle id;
        RGTextureDesc desc;
        bool imported;
        std::string name;

        RGTexture() : imported(false) {}
        RGTexture(RGTextureHandle id, const RGTextureDesc& desc, bool imported, const std::string& name)
            : id(id), desc(desc), imported(imported), name(name) {}
    };

    // ========================================
    // RenderGraph Pass Types
    // ========================================

    struct RGPass
    {
        std::string name;

        std::vector<RGTextureHandle> reads;
        std::vector<RGTextureHandle> writes;

        std::function<void(CommandList&)> execute;

        uint32_t passIndex;

        RGPass() : passIndex(0) {}
        RGPass(const std::string& name, uint32_t passIndex)
            : name(name), passIndex(passIndex) {}
    };

    // ========================================
    // RenderGraph Internal Types
    // ========================================

    struct ResourceLifetime
    {
        uint32_t firstUse;
        uint32_t lastUse;

        ResourceLifetime() : firstUse(UINT32_MAX), lastUse(0) {}
        ResourceLifetime(uint32_t first, uint32_t last) : firstUse(first), lastUse(last) {}
    };

    struct PhysicalTexture
    {
        GPUTextureHandle handle;
        uint32_t firstUse;
        uint32_t lastUse;
        uint32_t aliasedFrom;  // 0이면 독립, 아니면 다른 리소스와 공유

        PhysicalTexture() : firstUse(0), lastUse(0), aliasedFrom(0) {}
    };

    struct Barrier
    {
        RGTextureHandle resource;
        uint32_t passIndex;          // passOrder 상의 위치(이 패스 직전에 실행)
        ResourceState srcState;
        ResourceState dstState;

        Barrier() : resource(RGTextureHandle()), passIndex(0), srcState(ResourceState::Undefined), dstState(ResourceState::Undefined) {}
        Barrier(RGTextureHandle res, uint32_t pass, ResourceState src, ResourceState dst)
            : resource(res), passIndex(pass), srcState(src), dstState(dst) {}
    };

    struct ExecutionPlan
    {
        std::vector<uint32_t> passOrder;
        std::vector<PhysicalTexture> physicalResources;
        std::vector<Barrier> barriers;

        void Clear()
        {
            passOrder.clear();
            physicalResources.clear();
            barriers.clear();
        }
    };

    // ========================================
    // RenderGraph Class Declaration
    // ========================================

    // Forward declaration for JobSystem integration
    class JobSystem;
    struct JobHandle;

    class RenderGraph
    {
    public:
        RenderGraph();
        ~RenderGraph();

        // Resource management
        RGTextureHandle CreateTexture(const std::string& name, const RGTextureDesc& desc);
        RGTextureHandle ImportTexture(const std::string& name, GPUTextureHandle handle);

        // Pass management
        void AddPass(
            const std::string& name,
            std::function<void(RGBuilder&)> setup,
            std::function<void(CommandList&)> execute
        );

        // Compilation and execution
        void Compile();
        void Execute(CommandList& cmdList);
        
        // Parallel execution with JobSystem integration
        void ExecuteParallel(CommandList& cmdList, JobSystem* jobSystem);

        // 컴파일 결과 배리어 목록(테스트/진단용, 읽기 전용).
        const std::vector<Barrier>& GetBarriers() const { return executionPlan.barriers; }

        // Resource lifecycle management
        void ReleaseTransientResources();
        void ReturnResourcesToPool();

        // Utility
        void Reset();
        bool IsCompiled() const { return compiled; }

        // Debug/inspection methods
        size_t GetPassCount() const { return passes.size(); }
        size_t GetResourceCount() const { return resources.size(); }
        const RGPass& GetPass(size_t index) const { return passes[index]; }
        const RGTexture& GetResource(RGTextureHandle handle) const;
        
        // Physical resource management methods
        GPUTextureHandle GetPhysicalResource(RGTextureHandle logicalHandle) const;
        size_t GetPhysicalResourceCount() const { return executionPlan.physicalResources.size(); }
        const PhysicalTexture& GetPhysicalResourceByIndex(size_t index) const;
        bool IsResourceAliased(RGTextureHandle handle) const;
        RGTextureHandle GetAliasSource(RGTextureHandle handle) const;
        
        // Additional debug methods
        bool HasResource(const std::string& name) const 
        { 
            return resourceNameMap.find(name) != resourceNameMap.end(); 
        }
        
        RGTextureHandle GetResourceHandle(const std::string& name) const
        {
            auto it = resourceNameMap.find(name);
            return (it != resourceNameMap.end()) ? it->second : RGTextureHandle();
        }

    private:
        // Internal data
        std::vector<RGPass> passes;
        std::vector<RGTexture> resources;
        std::unordered_map<std::string, RGTextureHandle> resourceNameMap;
        std::unordered_map<RGTextureHandle, ResourceLifetime> lifetimes;
        std::unordered_map<RGTextureHandle, RGTextureHandle> aliasMap;  // resource -> aliased from
        std::unordered_map<RGTextureHandle, uint32_t> physicalResourceMap;  // logical resource -> physical resource index

        ExecutionPlan executionPlan;
        bool compiled;

        // Resource ID generation
        RGTextureHandle nextResourceId;

        // Internal helper methods
        std::vector<uint32_t> TopologicalSort();
        void BuildSortAdjacencyList(std::vector<std::vector<uint32_t>>& dependencies, std::vector<uint32_t>& inDegree) const;
        bool ValidateNoCycles(const std::vector<std::vector<uint32_t>>& dependencies);
        std::vector<uint32_t> PerformKahnSort(const std::vector<std::vector<uint32_t>>& dependencies, std::vector<uint32_t> inDegree);

        std::vector<std::vector<uint32_t>> BuildExecutionDependencyList() const;
        struct PassJobData;
        static void RunPassJob(void* data);
        void ExecuteSinglePassJob(JobSystem* jobSystem, CommandList& cmdList, size_t orderIndex, const std::vector<std::vector<uint32_t>>& passDependencies, std::vector<JobHandle>& passJobs, std::vector<bool>& passDispatched);

        void ComputeResourceLifetimes();
        void ComputeAliasing();
        std::vector<std::vector<Barrier>> parallelBarriersByPass;   // ExecuteParallel 동안 패스별 배리어
        void InsertBarriers();
        static ResourceState DesiredState(const RGTexture& texture, bool isWrite);

        void CreatePhysicalResources();
        bool DetectCycles();
        bool DetectCyclesDFS(uint32_t node, const std::vector<std::vector<uint32_t>>& dependencies, std::vector<int>& color);
        bool CanAlias(const RGTexture& a, const RGTexture& b) const;

        friend class RGBuilder;
    };

    // ========================================
    // RGBuilder Class Declaration
    // ========================================

    class RGBuilder
    {
    public:
        RGBuilder(RenderGraph* graph, RGPass* currentPass);

        RGTextureHandle ReadTexture(const std::string& name);
        RGTextureHandle WriteTexture(const std::string& name);
        RGTextureHandle CreateTexture(const std::string& name, const RGTextureDesc& desc);

    private:
        RenderGraph* graph;
        RGPass* currentPass;
    };

} // namespace Engine
