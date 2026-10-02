#pragma once

#include "../core/Types.h"
#include <string>
#include <vector>
#include <algorithm>

namespace Engine
{
    // ========================================
    // CommandList Interface
    // ========================================

    /// <summary>
    /// CommandList는 GPU 명령을 기록하는 인터페이스입니다.
    /// 실제 GPU API (DirectX, Vulkan 등)의 추상화 레이어 역할을 합니다.
    /// </summary>
    class CommandList
    {
    public:
        CommandList() = default;
        virtual ~CommandList() = default;

        // Resource binding
        virtual void SetRenderTarget(GPUTextureHandle renderTarget) = 0;
        virtual void SetDepthStencil(GPUTextureHandle depthStencil) = 0;
        virtual void SetTexture(uint32_t slot, GPUTextureHandle texture) = 0;

        // Drawing commands
        virtual void Clear(float r, float g, float b, float a) = 0;
        virtual void ClearDepth(float depth) = 0;
        virtual void DrawIndexed(uint32_t indexCount, uint32_t startIndex = 0, uint32_t baseVertex = 0) = 0;
        // 반환값: 실제로 draw를 발행했으면 true. 전제조건(인덱스 버퍼 바인딩 등)이 깨졌으면 draw를
        // 건너뛰고 false - 호출자는 이를 에러로 전파해야 한다(Renderer::SubmitIndexedInstancedBatch).
        virtual bool DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex = 0, uint32_t baseVertex = 0, uint32_t baseInstance = 0) = 0;
        virtual void Draw(uint32_t vertexCount, uint32_t startVertex = 0) = 0;

        // Resource barriers (for synchronization)
        // srcState -> dstState 전이. OpenGL은 암묵적 동기화라 기록만 하고, 명시적 배리어가 필요한 백엔드가
        // 이 정보로 전이를 만든다.
        virtual void ResourceBarrier(GPUTextureHandle resource, ResourceState srcState, ResourceState dstState) = 0;

        // Debug markers
        virtual void BeginEvent(const char* name) = 0;
        virtual void EndEvent() = 0;

        // Command list state
        virtual void Reset() = 0;
        virtual void Close() = 0;
        virtual void Finalize() = 0;

        // MVP 3D Rendering extensions
        virtual void SetViewport(uint32_t width, uint32_t height) = 0;
        // 반환값: 바인딩에 성공했으면 true(0 = 명시적 unbind도 true). 존재하지 않는 id면 false.
        virtual bool SetShader(uint32_t shaderId) = 0;
        virtual void SetUniformMat4(uint32_t location, const float* matrix) = 0;
        virtual void BindVertexArray(uint32_t vao) = 0;
        virtual void DrawArrays(uint32_t vertexCount, uint32_t startVertex = 0) = 0;
        virtual void DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t baseInstance = 0) = 0;
    };

    // ========================================
    // Mock CommandList for Testing
    // ========================================

    /// <summary>
    /// MockCommandList는 테스트용 CommandList 구현입니다.
    /// 실제 GPU 명령을 실행하지 않고 호출을 기록만 합니다.
    /// </summary>
    class MockCommandList : public CommandList
    {
    public:
        MockCommandList() = default;
        ~MockCommandList() override = default;

        // CommandList interface implementation
        void SetRenderTarget(GPUTextureHandle renderTarget) override
        {
            commands.push_back("SetRenderTarget(" + std::to_string(renderTarget.id) + ")");
        }

        void SetDepthStencil(GPUTextureHandle depthStencil) override
        {
            commands.push_back("SetDepthStencil(" + std::to_string(depthStencil.id) + ")");
        }

        void SetTexture(uint32_t slot, GPUTextureHandle texture) override
        {
            commands.push_back("SetTexture(" + std::to_string(slot) + ", " + std::to_string(texture.id) + ")");
        }

        void Clear(float r, float g, float b, float a) override
        {
            commands.push_back("Clear(" + std::to_string(r) + ", " + std::to_string(g) + ", " + 
                             std::to_string(b) + ", " + std::to_string(a) + ")");
        }

        void ClearDepth(float depth) override
        {
            commands.push_back("ClearDepth(" + std::to_string(depth) + ")");
        }

        void DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex) override
        {
            commands.push_back("DrawIndexed(" + std::to_string(indexCount) + ", " + 
                             std::to_string(startIndex) + ", " + std::to_string(baseVertex) + ")");
        }

        bool DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, uint32_t baseVertex, uint32_t baseInstance) override
        {
            commands.push_back("DrawIndexedInstanced(" + std::to_string(indexCount) + ", " + 
                             std::to_string(instanceCount) + ", " + std::to_string(startIndex) + ", " + 
                             std::to_string(baseVertex) + ", " + std::to_string(baseInstance) + ")");
            return true;
        }

        void Draw(uint32_t vertexCount, uint32_t startVertex) override
        {
            commands.push_back("Draw(" + std::to_string(vertexCount) + ", " + std::to_string(startVertex) + ")");
        }

        void ResourceBarrier(GPUTextureHandle resource, ResourceState srcState, ResourceState dstState) override
        {
            commands.push_back("ResourceBarrier(" + std::to_string(resource.id) + ", " +
                               std::to_string(static_cast<int>(srcState)) + "->" +
                               std::to_string(static_cast<int>(dstState)) + ")");
        }

        void BeginEvent(const char* name) override
        {
            commands.push_back("BeginEvent(" + std::string(name) + ")");
        }

        void EndEvent() override
        {
            commands.push_back("EndEvent()");
        }

        void Reset() override
        {
            commands.clear();
        }

        void Close() override
        {
            // Do nothing
        }

        void Finalize() override
        {
            // Do nothing
        }

        // MVP 3D Rendering extensions
        void SetViewport(uint32_t width, uint32_t height) override
        {
            commands.push_back("SetViewport(" + std::to_string(width) + ", " + std::to_string(height) + ")");
        }
        
        bool SetShader(uint32_t shaderId) override
        {
            commands.push_back("SetShader(" + std::to_string(shaderId) + ")");
            return true;
        }
        
        void SetUniformMat4(uint32_t location, const float* matrix) override
        {
            commands.push_back("SetUniformMat4(" + std::to_string(location) + ", <matrix>)");
        }
        
        void BindVertexArray(uint32_t vao) override
        {
            commands.push_back("BindVertexArray(" + std::to_string(vao) + ")");
        }
        
        void DrawArrays(uint32_t vertexCount, uint32_t startVertex = 0) override
        {
            commands.push_back("DrawArrays(" + std::to_string(vertexCount) + ", " + std::to_string(startVertex) + ")");
        }

        void DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex = 0, uint32_t baseInstance = 0) override
        {
            commands.push_back("DrawArraysInstanced(" + std::to_string(vertexCount) + ", " + std::to_string(instanceCount) + ", " + std::to_string(startVertex) + ", " + std::to_string(baseInstance) + ")");
        }

        // Test utilities
        const std::vector<std::string>& GetCommands() const { return commands; }
        size_t GetCommandCount() const { return commands.size(); }
        bool HasCommand(const std::string& command) const
        {
            return std::find(commands.begin(), commands.end(), command) != commands.end();
        }

    private:
        std::vector<std::string> commands;
    };

} // namespace Engine