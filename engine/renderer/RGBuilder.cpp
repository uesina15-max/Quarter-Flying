#include "RenderGraph.h"
#include "../core/logging/Logger.h"
#include <string>

namespace Engine
{
    RGBuilder::RGBuilder(RenderGraph* graph, RGPass* currentPass)
        : graph(graph), currentPass(currentPass)
    {
    }

    RGTextureHandle RGBuilder::ReadTexture(const std::string& name)
    {
        // Find existing texture by name
        auto it = graph->resourceNameMap.find(name);
        if (it == graph->resourceNameMap.end())
        {
            Logger::Log(LogLevel::Fatal, ("RGBuilder::ReadTexture - Texture not found: " + name).c_str());
            return RGTextureHandle();  // Return invalid handle
        }

        RGTextureHandle handle = it->second;
        
        // Add to current pass's read list
        currentPass->reads.push_back(handle);
        
        // Update resource lifetime
        auto& lifetime = graph->lifetimes[handle];
        if (lifetime.firstUse == UINT32_MAX)
        {
            lifetime.firstUse = currentPass->passIndex;
        }
        lifetime.lastUse = currentPass->passIndex;

        return handle;
    }

    RGTextureHandle RGBuilder::WriteTexture(const std::string& name)
    {
        // Find existing texture by name
        auto it = graph->resourceNameMap.find(name);
        if (it == graph->resourceNameMap.end())
        {
            Logger::Log(LogLevel::Fatal, ("RGBuilder::WriteTexture - Texture not found: " + name).c_str());
            return RGTextureHandle();  // Return invalid handle
        }

        RGTextureHandle handle = it->second;
        
        // Add to current pass's write list
        currentPass->writes.push_back(handle);
        
        // Update resource lifetime
        auto& lifetime = graph->lifetimes[handle];
        if (lifetime.firstUse == UINT32_MAX)
        {
            lifetime.firstUse = currentPass->passIndex;
        }
        lifetime.lastUse = currentPass->passIndex;

        return handle;
    }

    RGTextureHandle RGBuilder::CreateTexture(const std::string& name, const RGTextureDesc& desc)
    {
        // Check if texture with this name already exists
        auto it = graph->resourceNameMap.find(name);
        if (it != graph->resourceNameMap.end())
        {
            Logger::Log(LogLevel::Warning, ("RGBuilder::CreateTexture - Texture already exists: " + name).c_str());
            return it->second;
        }

        // Create new texture
        RGTextureHandle id = graph->nextResourceId;
        graph->nextResourceId = RGTextureHandle(graph->nextResourceId.id + 1, graph->nextResourceId.generation);
        RGTexture texture(id, desc, false, name);
        
        // Add to resources
        graph->resources.push_back(texture);
        graph->resourceNameMap[name] = id;
        
        // Initialize lifetime
        graph->lifetimes[id] = ResourceLifetime();
        
        // Add to current pass's write list (creation implies writing)
        currentPass->writes.push_back(id);
        
        // Update resource lifetime
        auto& lifetime = graph->lifetimes[id];
        lifetime.firstUse = currentPass->passIndex;
        lifetime.lastUse = currentPass->passIndex;

        return id;
    }

} // namespace Engine