#pragma once

#include "../../core/Types.h"
#include <GL/glew.h>
#include <unordered_map>
#include <memory>
#include <vector>
#include <string>

namespace Engine
{
    // ========================================
    // Vertex Format Structures
    // ========================================

    enum class VertexAttributeType : uint8_t
    {
        Float,
        Float2,
        Float3,
        Float4,
        Int,
        Int2,
        Int3,
        Int4
    };

    struct VertexAttribute
    {
        VertexAttributeType type;
        uint32_t offset;
        bool normalized;
        
        VertexAttribute(VertexAttributeType t, uint32_t o, bool n = false)
            : type(t), offset(o), normalized(n) {}
    };

    struct VertexFormat
    {
        uint32_t stride;
        std::vector<VertexAttribute> attributes;
        
        VertexFormat() : stride(0) {}
        VertexFormat(uint32_t s, const std::vector<VertexAttribute>& attrs)
            : stride(s), attributes(attrs) {}
        
        // Common vertex formats
        static VertexFormat PositionUVNormal();
    };

    // ========================================
    // Texture Descriptor Structures
    // ========================================

    struct TextureDescriptor
    {
        uint32_t width;
        uint32_t height;
        TextureFormat format;
        TextureUsage usage;
        
        // Texture parameters
        GLenum minFilter;
        GLenum magFilter;
        GLenum wrapS;
        GLenum wrapT;
        
        TextureDescriptor(uint32_t w = 0, uint32_t h = 0, TextureFormat f = TextureFormat::RGBA8, TextureUsage u = TextureUsage::ShaderResource)
            : width(w), height(h), format(f), usage(u)
            , minFilter(GL_LINEAR), magFilter(GL_LINEAR)
            , wrapS(GL_CLAMP_TO_EDGE), wrapT(GL_CLAMP_TO_EDGE) {}
        
        static TextureDescriptor DefaultRenderTarget(uint32_t width, uint32_t height);
        static TextureDescriptor DefaultDepthStencil(uint32_t width, uint32_t height);
    };

    // ========================================
    // OpenGL Resource Types
    // ========================================

    struct OpenGLTexture
    {
        GLuint glId;
        uint32_t width;
        uint32_t height;
        TextureFormat format;
        TextureUsage usage;
        
        OpenGLTexture() : glId(0), width(0), height(0), format(TextureFormat::RGBA8), usage(TextureUsage::ShaderResource) {}
    };

    struct OpenGLFramebuffer
    {
        GLuint glId;
        GPUTextureHandle colorAttachment;
        GPUTextureHandle depthAttachment;
        uint32_t width;
        uint32_t height;
        
        OpenGLFramebuffer() : glId(0), colorAttachment(), depthAttachment(), width(0), height(0) {}
    };

    struct OpenGLVertexArray
    {
        GLuint glId;
        GLuint vertexBuffer;
        GLuint indexBuffer;
        uint32_t vertexCount;
        uint32_t indexCount;
        
        OpenGLVertexArray() : glId(0), vertexBuffer(0), indexBuffer(0), vertexCount(0), indexCount(0) {}
    };

    struct OpenGLShaderProgram
    {
        GLuint glId;
        std::string vertexSource;
        std::string fragmentSource;
        
        OpenGLShaderProgram() : glId(0) {}
    };

    // ========================================
    // OpenGL Resource Manager
    // ========================================

    class OpenGLResourceManager
    {
    public:
        OpenGLResourceManager();
        ~OpenGLResourceManager();

        // Texture management
        // NOTE: header previously declared CreateTexture(const TextureDescriptor&), but
        // OpenGLResourceManager.cpp defines and calls a 4-arg overload instead -- the
        // TextureDescriptor version was never actually implemented or called anywhere in
        // the codebase (confirmed via project-wide search, including engine/bindings/).
        // Declaration updated to match the real implementation.
        GPUTextureHandle CreateTexture(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage);
        void DestroyTexture(GPUTextureHandle handle);
        OpenGLTexture* GetTexture(GPUTextureHandle handle);
        const OpenGLTexture* GetTexture(GPUTextureHandle handle) const;

        // Framebuffer management
        GPUTextureHandle CreateFramebuffer(uint32_t width, uint32_t height, TextureFormat colorFormat, bool hasDepth);
        void DestroyFramebuffer(GPUTextureHandle handle);
        OpenGLFramebuffer* GetFramebuffer(GPUTextureHandle handle);
        const OpenGLFramebuffer* GetFramebuffer(GPUTextureHandle handle) const;

        // Vertex Array management
        GPUVertexArrayHandle CreateVertexArray(const void* vertexData, uint32_t vertexCount, const VertexFormat& format,
                                               const void* indexData, uint32_t indexCount);
        void DestroyVertexArray(GPUVertexArrayHandle handle);
        OpenGLVertexArray* GetVertexArray(GPUVertexArrayHandle handle);
        const OpenGLVertexArray* GetVertexArray(GPUVertexArrayHandle handle) const;

        // Shader program management
        uint32_t CreateShaderProgram(const char* vertexSource, const char* fragmentSource);
        void DestroyShaderProgram(uint32_t shaderId);
        OpenGLShaderProgram* GetShaderProgram(uint32_t shaderId);
        const OpenGLShaderProgram* GetShaderProgram(uint32_t shaderId) const;

        // Cleanup
        void DestroyAllResources();

    private:
        // Resource storage
        std::unordered_map<GPUTextureHandle, OpenGLTexture> textures;
        std::unordered_map<GPUTextureHandle, OpenGLFramebuffer> framebuffers;
        std::unordered_map<GPUVertexArrayHandle, OpenGLVertexArray> vertexArrays;
        std::unordered_map<uint32_t, OpenGLShaderProgram> shaderPrograms;

        // ID generation
        GPUTextureHandle nextTextureHandle;
        GPUTextureHandle nextFramebufferHandle;
        GPUVertexArrayHandle nextVertexArrayHandle;
        uint32_t nextShaderId;

        // Helper methods
        GLuint ConvertTextureFormat(TextureFormat format);
        GLenum ConvertTextureUsage(TextureUsage usage);
    };

} // namespace Engine
