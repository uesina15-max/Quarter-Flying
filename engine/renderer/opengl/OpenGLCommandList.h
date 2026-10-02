#pragma once

#include "../CommandList.h"
#include "OpenGLResourceManager.h"
#include <memory>

namespace Engine
{
    class OpenGLCommandList : public CommandList
    {
    public:
        OpenGLCommandList();
        ~OpenGLCommandList() override;

        // Resource manager access
        OpenGLResourceManager* GetResourceManager() { return resourceManager.get(); }

        // CommandList interface implementation
        void SetRenderTarget(GPUTextureHandle renderTarget) override;
        void SetDepthStencil(GPUTextureHandle depthStencil) override;
        void SetTexture(uint32_t slot, GPUTextureHandle texture) override;

        void Clear(float r, float g, float b, float a) override;
        void ClearDepth(float depth) override;
        void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, uint32_t baseVertex = 0) override;
        bool DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0, uint32_t baseVertex = 0, uint32_t baseInstance = 0) override;
        void Draw(uint32_t vertexCount, uint32_t startVertex = 0) override;

        void ResourceBarrier(GPUTextureHandle resource, ResourceState srcState, ResourceState dstState) override;
        void BeginEvent(const char* name) override;
        void EndEvent() override;

        void Reset() override;
        void Close() override;
        void Finalize() override;

        // MVP 3D Rendering extensions
        void SetViewport(uint32_t width, uint32_t height) override;
        bool SetShader(uint32_t shaderId) override;
        void SetUniformMat4(uint32_t location, const float* matrix) override;
        void BindVertexArray(uint32_t vao) override;
        void DrawArrays(uint32_t vertexCount, uint32_t startVertex = 0) override;
        void DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t baseInstance = 0) override;

    private:
        std::unique_ptr<OpenGLResourceManager> resourceManager;
        
        // Current state tracking
        GPUTextureHandle currentRenderTarget;
        GPUTextureHandle currentDepthStencil;
        uint32_t currentShader;
        uint32_t currentVAO;
        
        // Command list state
        bool isOpen;
        bool isClosed;
    };
}
