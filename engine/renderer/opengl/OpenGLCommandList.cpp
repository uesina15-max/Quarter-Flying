#include "OpenGLCommandList.h"
#include "../../core/logging/Logger.h"
#include <GL/glew.h>

namespace Engine
{
    OpenGLCommandList::OpenGLCommandList()
        : currentRenderTarget()
        , currentDepthStencil()
        , currentShader(0)
        , currentVAO(0)
        , isOpen(false)
        , isClosed(false)
    {
        resourceManager = std::make_unique<OpenGLResourceManager>();
        Reset();
        Logger::Log(LogLevel::Info, "OpenGLCommandList - Initialized with resource manager");
    }

    OpenGLCommandList::~OpenGLCommandList()
    {
        if (isOpen)
        {
            Close();
        }
        resourceManager.reset();
    }
    void OpenGLCommandList::SetRenderTarget(GPUTextureHandle renderTarget)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetRenderTarget - Command list is not open");
            return;
        }

        currentRenderTarget = renderTarget;

        if (renderTarget.id == 0)
        {
            // Bind default framebuffer
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            Logger::Log(LogLevel::Trace, "OpenGLCommandList::SetRenderTarget - Bound default framebuffer");
        }
        else
        {
            // Bind framebuffer from resource manager
            OpenGLFramebuffer* framebuffer = resourceManager->GetFramebuffer(renderTarget);
            if (framebuffer)
            {
                glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->glId);
                Logger::Log(LogLevel::Trace, ("OpenGLCommandList::SetRenderTarget - Bound framebuffer " + std::to_string(renderTarget.id)).c_str());
            }
            else
            {
                Logger::Log(LogLevel::Error, ("OpenGLCommandList::SetRenderTarget - Invalid framebuffer handle: " + std::to_string(renderTarget.id)).c_str());
            }
        }
    }

    void OpenGLCommandList::SetDepthStencil(GPUTextureHandle depthStencil)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetDepthStencil - Command list is not open");
            return;
        }

        currentDepthStencil = depthStencil;

        // Depth stencil is typically attached to the framebuffer, so this is mainly for tracking
        // In a more advanced implementation, this would handle separate depth stencil buffers
        Logger::Log(LogLevel::Trace, ("OpenGLCommandList::SetDepthStencil - Set depth stencil handle " + std::to_string(depthStencil.id)).c_str());
    }

    void OpenGLCommandList::SetTexture(uint32_t slot, GPUTextureHandle texture)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetTexture - Command list is not open");
            return;
        }

        glActiveTexture(GL_TEXTURE0 + slot);

        if (texture.id == 0)
        {
            glBindTexture(GL_TEXTURE_2D, 0);
        }
        else
        {
            OpenGLTexture* glTexture = resourceManager->GetTexture(texture);
            if (glTexture)
            {
                glBindTexture(GL_TEXTURE_2D, glTexture->glId);
                Logger::Log(LogLevel::Trace, ("OpenGLCommandList::SetTexture - Bound texture " + std::to_string(texture.id) + " to slot " + std::to_string(slot)).c_str());
            }
            else
            {
                Logger::Log(LogLevel::Error, ("OpenGLCommandList::SetTexture - Invalid texture handle: " + std::to_string(texture.id)).c_str());
                glBindTexture(GL_TEXTURE_2D, 0);
            }
        }
    }

    void OpenGLCommandList::Clear(float r, float g, float b, float a)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::Clear - Command list is not open");
            return;
        }

        glClearColor(r, g, b, a);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLCommandList::ClearDepth(float depth)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::ClearDepth - Command list is not open");
            return;
        }

        glClearDepth(depth);
        glClear(GL_DEPTH_BUFFER_BIT);
    }

    void OpenGLCommandList::DrawIndexed(uint32_t indexCount, uint32_t startIndex, uint32_t baseVertex)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::DrawIndexed - Command list is not open");
            return;
        }

        GLint boundElementBuffer = 0;  // DrawIndexedInstanced 주석 참고 - 같은 크래시 경로
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementBuffer);
        if (boundElementBuffer == 0)
        {
            Logger::Log(LogLevel::Error, "OpenGLCommandList::DrawIndexed - no element (index) buffer bound to the current VAO; skipping draw");
            return;
        }

        glDrawElementsBaseVertex(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)(startIndex * sizeof(uint32_t)), baseVertex);
    }

    bool OpenGLCommandList::DrawIndexedInstanced(uint32_t indexCount, uint32_t instanceCount, uint32_t startIndex, uint32_t baseVertex, uint32_t baseInstance)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::DrawIndexedInstanced - Command list is not open");
            return false;
        }

        // 인덱스 draw는 현재 VAO에 인덱스 버퍼(EBO)가 바인딩돼 있어야 한다. 없으면 GL은 마지막 인자를
        // 클라이언트 메모리 포인터로 해석해서 (void*)(startIndex*4) 주소를 읽고 access violation으로 죽는다.
        // RendererInstancingTest.SubmitIndexedInstancedBatchSucceeds가 바로 이 경로에서
        // "SEH exception with code 0xc0000005 thrown in the test body"로 실패하고 있었다.
        GLint boundElementBuffer = 0;
        glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &boundElementBuffer);
        if (boundElementBuffer == 0)
        {
            Logger::Log(LogLevel::Error, "OpenGLCommandList::DrawIndexedInstanced - no element (index) buffer bound to the current VAO; skipping draw");
            return false;
        }

        glDrawElementsInstancedBaseVertexBaseInstance(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, (void*)(startIndex * sizeof(uint32_t)), instanceCount, baseVertex, baseInstance);
        return true;
    }

    void OpenGLCommandList::Draw(uint32_t vertexCount, uint32_t startVertex)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::Draw - Command list is not open");
            return;
        }

        glDrawArrays(GL_TRIANGLES, startVertex, vertexCount);
    }

    void OpenGLCommandList::ResourceBarrier(GPUTextureHandle resource, ResourceState /*srcState*/, ResourceState /*dstState*/)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::ResourceBarrier - Command list is not open");
            return;
        }

        // OpenGL handles most synchronization automatically via implicit synchronization
        // For advanced use cases with compute shaders or multiple contexts, we would use glMemoryBarrier
        // For now, this is a no-op but logged for debugging
        Logger::Log(LogLevel::Trace, ("OpenGLCommandList::ResourceBarrier - Barrier for resource " + std::to_string(resource.id)).c_str());
    }

    void OpenGLCommandList::BeginEvent(const char* name)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::BeginEvent - Command list is not open");
            return;
        }

        // Use OpenGL debug markers if available (GL_KHR_debug extension)
        if (glPushDebugGroup)
        {
            glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, name);
        }
    }

    void OpenGLCommandList::EndEvent()
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::EndEvent - Command list is not open");
            return;
        }

        // Use OpenGL debug markers if available
        if (glPopDebugGroup)
        {
            glPopDebugGroup();
        }
    }

    void OpenGLCommandList::Reset()
    {
        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Reset - Resetting command list state");
        
        // Reset state tracking
        currentRenderTarget = GPUTextureHandle();
        currentDepthStencil = GPUTextureHandle();
        currentShader = 0;
        currentVAO = 0;
        
        // Reset command list state
        isOpen = true;
        isClosed = false;
        
        // Reset OpenGL state to defaults
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glUseProgram(0);
        glBindVertexArray(0);
        glBindTexture(GL_TEXTURE_2D, 0);
        
        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Reset - Command list reset complete");
    }

    void OpenGLCommandList::Close()
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::Close - Command list is already closed");
            return;
        }

        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Close - Closing command list");
        
        // Flush any pending OpenGL commands
        glFlush();
        
        isOpen = false;
        isClosed = true;
        
        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Close - Command list closed");
    }

    void OpenGLCommandList::Finalize()
    {
        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Finalize - Finalizing command list");
        
        if (isOpen)
        {
            Close();
        }
        
        // Finalize only closes the command list, does not reset state
        // Reset should be called explicitly for the next frame
        Logger::Log(LogLevel::Trace, "OpenGLCommandList::Finalize - Command list finalized");
    }

    void OpenGLCommandList::SetViewport(uint32_t width, uint32_t height)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetViewport - Command list is not open");
            return;
        }

        glViewport(0, 0, width, height);
    }
    
    bool OpenGLCommandList::SetShader(uint32_t shaderId)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetShader - Command list is not open");
            return false;
        }

        currentShader = shaderId;
        
        if (shaderId == 0)
        {
            glUseProgram(0);
            return true;
        }
        else
        {
            OpenGLShaderProgram* shader = resourceManager->GetShaderProgram(shaderId);
            if (shader)
            {
                glUseProgram(shader->glId);
                Logger::Log(LogLevel::Trace, ("OpenGLCommandList::SetShader - Bound shader program " + std::to_string(shaderId)).c_str());
                return true;
            }
            else
            {
                // 예전에는 여기서 로그만 남기고 호출자에게 알릴 방법이 없어서, Renderer::Submit*가 그대로
                // draw를 발행했다(프로그램 0 상태로 그려짐 - 아무 것도 안 보이거나, 인덱스 draw면 크래시).
                Logger::Log(LogLevel::Error, ("OpenGLCommandList::SetShader - Invalid shader ID: " + std::to_string(shaderId)).c_str());
                glUseProgram(0);
                currentShader = 0;
                return false;
            }
        }
    }
    
    void OpenGLCommandList::SetUniformMat4(uint32_t location, const float* matrix)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::SetUniformMat4 - Command list is not open");
            return;
        }

        // location can be retrieved via glGetUniformLocation
        glUniformMatrix4fv(location, 1, GL_FALSE, matrix);
    }
    
    void OpenGLCommandList::BindVertexArray(uint32_t vao)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::BindVertexArray - Command list is not open");
            return;
        }

        currentVAO = vao;
        
        if (vao == 0)
        {
            glBindVertexArray(0);
        }
        else
        {
            OpenGLVertexArray* vertexArray = resourceManager->GetVertexArray(GPUVertexArrayHandle(vao, 0));
            if (vertexArray)
            {
                glBindVertexArray(vertexArray->glId);
                Logger::Log(LogLevel::Trace, ("OpenGLCommandList::BindVertexArray - Bound VAO " + std::to_string(vao)).c_str());
            }
            else
            {
                Logger::Log(LogLevel::Error, ("OpenGLCommandList::BindVertexArray - Invalid VAO handle: " + std::to_string(vao)).c_str());
                glBindVertexArray(0);
            }
        }
    }
    
    void OpenGLCommandList::DrawArrays(uint32_t vertexCount, uint32_t startVertex)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::DrawArrays - Command list is not open");
            return;
        }

        glDrawArrays(GL_TRIANGLES, startVertex, vertexCount);
    }

    void OpenGLCommandList::DrawArraysInstanced(uint32_t vertexCount, uint32_t instanceCount, uint32_t startVertex, uint32_t baseInstance)
    {
        if (!isOpen)
        {
            Logger::Log(LogLevel::Warning, "OpenGLCommandList::DrawArraysInstanced - Command list is not open");
            return;
        }

        glDrawArraysInstancedBaseInstance(GL_TRIANGLES, startVertex, vertexCount, instanceCount, baseInstance);
    }
}
