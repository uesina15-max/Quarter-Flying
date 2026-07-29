#include "OpenGLResourceManager.h"
#include "../../core/logging/Logger.h"
#include <GL/glew.h>
#include <cstring>

namespace Engine
{
    // ========================================
    // Vertex Format Implementation
    // ========================================

    VertexFormat VertexFormat::PositionUVNormal()
    {
        return VertexFormat(
            32, // stride: 3(float) + 2(float) + 3(float) = 8 * 4 = 32 bytes
            {
                VertexAttribute(VertexAttributeType::Float3, 0),   // Position
                VertexAttribute(VertexAttributeType::Float2, 12),  // UV
                VertexAttribute(VertexAttributeType::Float3, 20)   // Normal
            }
        );
    }

    // ========================================
    // Texture Descriptor Implementation
    // ========================================

    TextureDescriptor TextureDescriptor::DefaultRenderTarget(uint32_t width, uint32_t height)
    {
        TextureDescriptor desc(width, height, TextureFormat::RGBA8, TextureUsage::RenderTarget);
        desc.minFilter = GL_LINEAR;
        desc.magFilter = GL_LINEAR;
        desc.wrapS = GL_CLAMP_TO_EDGE;
        desc.wrapT = GL_CLAMP_TO_EDGE;
        return desc;
    }

    TextureDescriptor TextureDescriptor::DefaultDepthStencil(uint32_t width, uint32_t height)
    {
        TextureDescriptor desc(width, height, TextureFormat::Depth24Stencil8, TextureUsage::DepthStencil);
        desc.minFilter = GL_NEAREST;
        desc.magFilter = GL_NEAREST;
        desc.wrapS = GL_CLAMP_TO_EDGE;
        desc.wrapT = GL_CLAMP_TO_EDGE;
        return desc;
    }

    // ========================================
    // OpenGL Resource Manager
    // ========================================

    OpenGLResourceManager::OpenGLResourceManager()
        : nextTextureHandle(1, 0)
        , nextFramebufferHandle(1, 0)
        , nextVertexArrayHandle(1, 0)
        , nextShaderId(1)
    {
        Logger::Log(LogLevel::Info, "OpenGLResourceManager - Initialized");
    }

    OpenGLResourceManager::~OpenGLResourceManager()
    {
        DestroyAllResources();
    }

    GLuint OpenGLResourceManager::ConvertTextureFormat(TextureFormat format)
    {
        switch (format)
        {
            case TextureFormat::RGBA8:           return GL_RGBA8;
            case TextureFormat::RGBA16F:         return GL_RGBA16F;
            case TextureFormat::RGBA32F:         return GL_RGBA32F;
            case TextureFormat::Depth24Stencil8: return GL_DEPTH24_STENCIL8;
            case TextureFormat::Depth32F:        return GL_DEPTH_COMPONENT32F;
            default:                             return GL_RGBA8;
        }
    }

    GLenum OpenGLResourceManager::ConvertTextureUsage(TextureUsage usage)
    {
        switch (usage)
        {
            case TextureUsage::ShaderResource: return GL_TEXTURE_2D;
            case TextureUsage::RenderTarget:   return GL_TEXTURE_2D;
            case TextureUsage::DepthStencil:   return GL_TEXTURE_2D;
            default:                           return GL_TEXTURE_2D;
        }
    }

    // ========================================
    // Texture Management
    // ========================================

    GPUTextureHandle OpenGLResourceManager::CreateTexture(uint32_t width, uint32_t height, TextureFormat format, TextureUsage usage)
    {
        GLuint glId;
        glGenTextures(1, &glId);
        
        GLenum glFormat = ConvertTextureFormat(format);
        GLenum glUsage = ConvertTextureUsage(usage);
        
        glBindTexture(glUsage, glId);
        
        // Determine internal format and data format
        GLenum internalFormat = GL_RGBA8;
        GLenum dataFormat = GL_RGBA;
        GLenum dataType = GL_UNSIGNED_BYTE;
        
        switch (format)
        {
            case TextureFormat::RGBA8:
                internalFormat = GL_RGBA8;
                dataFormat = GL_RGBA;
                dataType = GL_UNSIGNED_BYTE;
                break;
            case TextureFormat::RGBA16F:
                internalFormat = GL_RGBA16F;
                dataFormat = GL_RGBA;
                dataType = GL_HALF_FLOAT;
                break;
            case TextureFormat::RGBA32F:
                internalFormat = GL_RGBA32F;
                dataFormat = GL_RGBA;
                dataType = GL_FLOAT;
                break;
            case TextureFormat::Depth24Stencil8:
                internalFormat = GL_DEPTH24_STENCIL8;
                dataFormat = GL_DEPTH_STENCIL;
                dataType = GL_UNSIGNED_INT_24_8;
                break;
            case TextureFormat::Depth32F:
                internalFormat = GL_DEPTH_COMPONENT32F;
                dataFormat = GL_DEPTH_COMPONENT;
                dataType = GL_FLOAT;
                break;
        }
        
        // Allocate texture storage
        glTexImage2D(glUsage, 0, internalFormat, width, height, 0, dataFormat, dataType, nullptr);
        
        // Set texture parameters
        glTexParameteri(glUsage, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(glUsage, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(glUsage, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(glUsage, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        glBindTexture(glUsage, 0);
        
        // Create texture record
        GPUTextureHandle handle = nextTextureHandle;
        nextTextureHandle = GPUTextureHandle(nextTextureHandle.id + 1, nextTextureHandle.generation);
        
        OpenGLTexture texture;
        texture.glId = glId;
        texture.width = width;
        texture.height = height;
        texture.format = format;
        texture.usage = usage;
        
        textures[handle] = texture;
        
        Logger::Log(LogLevel::Info, ("OpenGLResourceManager::CreateTexture - Created texture " + std::to_string(handle.id) + 
                                    " (" + std::to_string(width) + "x" + std::to_string(height) + ")").c_str());
        
        return handle;
    }

    void OpenGLResourceManager::DestroyTexture(GPUTextureHandle handle)
    {
        auto it = textures.find(handle);
        if (it != textures.end())
        {
            glDeleteTextures(1, &it->second.glId);
            textures.erase(it);
            Logger::Log(LogLevel::Info, ("OpenGLResourceManager::DestroyTexture - Destroyed texture " + std::to_string(handle.id)).c_str());
        }
    }

    OpenGLTexture* OpenGLResourceManager::GetTexture(GPUTextureHandle handle)
    {
        auto it = textures.find(handle);
        return (it != textures.end()) ? &it->second : nullptr;
    }

    const OpenGLTexture* OpenGLResourceManager::GetTexture(GPUTextureHandle handle) const
    {
        auto it = textures.find(handle);
        return (it != textures.end()) ? &it->second : nullptr;
    }

    // ========================================
    // Framebuffer Management
    // ========================================

    GPUTextureHandle OpenGLResourceManager::CreateFramebuffer(uint32_t width, uint32_t height, TextureFormat colorFormat, bool hasDepth)
    {
        // Create color texture
        GPUTextureHandle colorTexture = CreateTexture(width, height, colorFormat, TextureUsage::RenderTarget);
        OpenGLTexture* colorTex = GetTexture(colorTexture);
        
        if (!colorTex)
        {
            Logger::Log(LogLevel::Error, "OpenGLResourceManager::CreateFramebuffer - Failed to create color texture");
            return GPUTextureHandle();
        }
        
        // Create depth texture if needed
        GPUTextureHandle depthTexture;
        if (hasDepth)
        {
            depthTexture = CreateTexture(width, height, TextureFormat::Depth24Stencil8, TextureUsage::DepthStencil);
            OpenGLTexture* depthTex = GetTexture(depthTexture);
            if (!depthTex)
            {
                Logger::Log(LogLevel::Error, "OpenGLResourceManager::CreateFramebuffer - Failed to create depth texture");
                DestroyTexture(colorTexture);
                return GPUTextureHandle();
            }
        }
        
        // Create framebuffer
        GLuint fboId;
        glGenFramebuffers(1, &fboId);
        glBindFramebuffer(GL_FRAMEBUFFER, fboId);
        
        // Attach color texture
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex->glId, 0);
        
        // Attach depth texture if needed
        if (hasDepth)
        {
            OpenGLTexture* depthTex = GetTexture(depthTexture);
            // Use GL_DEPTH_STENCIL_ATTACHMENT for combined depth-stencil formats
            if (depthTex->format == TextureFormat::Depth24Stencil8)
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, depthTex->glId, 0);
            }
            else
            {
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex->glId, 0);
            }
        }
        
        // Check framebuffer status
        GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if (status != GL_FRAMEBUFFER_COMPLETE)
        {
            Logger::Log(LogLevel::Error, ("OpenGLResourceManager::CreateFramebuffer - Framebuffer incomplete: " + std::to_string(status)).c_str());
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &fboId);
            DestroyTexture(colorTexture);
            if (hasDepth) DestroyTexture(depthTexture);
            return GPUTextureHandle();
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        
        // Create framebuffer record
        GPUTextureHandle handle = nextFramebufferHandle;
        nextFramebufferHandle = GPUTextureHandle(nextFramebufferHandle.id + 1, nextFramebufferHandle.generation);
        
        OpenGLFramebuffer framebuffer;
        framebuffer.glId = fboId;
        framebuffer.colorAttachment = colorTexture;
        framebuffer.depthAttachment = depthTexture;
        framebuffer.width = width;
        framebuffer.height = height;
        
        framebuffers[handle] = framebuffer;
        
        Logger::Log(LogLevel::Info, ("OpenGLResourceManager::CreateFramebuffer - Created framebuffer " + std::to_string(handle.id) + 
                                    " (" + std::to_string(width) + "x" + std::to_string(height) + ")").c_str());
        
        return handle;
    }

    void OpenGLResourceManager::DestroyFramebuffer(GPUTextureHandle handle)
    {
        auto it = framebuffers.find(handle);
        if (it != framebuffers.end())
        {
            // Destroy attached textures
            DestroyTexture(it->second.colorAttachment);
            if (it->second.depthAttachment.IsValid())
            {
                DestroyTexture(it->second.depthAttachment);
            }
            
            glDeleteFramebuffers(1, &it->second.glId);
            framebuffers.erase(it);
            Logger::Log(LogLevel::Info, ("OpenGLResourceManager::DestroyFramebuffer - Destroyed framebuffer " + std::to_string(handle.id)).c_str());
        }
    }

    OpenGLFramebuffer* OpenGLResourceManager::GetFramebuffer(GPUTextureHandle handle)
    {
        auto it = framebuffers.find(handle);
        return (it != framebuffers.end()) ? &it->second : nullptr;
    }

    const OpenGLFramebuffer* OpenGLResourceManager::GetFramebuffer(GPUTextureHandle handle) const
    {
        auto it = framebuffers.find(handle);
        return (it != framebuffers.end()) ? &it->second : nullptr;
    }

    // ========================================
    // Vertex Array Management
    // ========================================

    GPUVertexArrayHandle OpenGLResourceManager::CreateVertexArray(const void* vertexData, uint32_t vertexCount, const VertexFormat& format,
                                                               const void* indexData, uint32_t indexCount)
    {
        GLuint vao, vbo, ibo;
        
        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        
        // Create and bind vertex buffer
        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertexCount * format.stride, vertexData, GL_STATIC_DRAW);
        
        // Setup vertex attributes based on format
        for (uint32_t i = 0; i < format.attributes.size(); ++i)
        {
            const VertexAttribute& attr = format.attributes[i];
            
            GLint componentCount = 0;
            GLenum componentType = GL_FLOAT;
            
            switch (attr.type)
            {
                case VertexAttributeType::Float:
                    componentCount = 1;
                    componentType = GL_FLOAT;
                    break;
                case VertexAttributeType::Float2:
                    componentCount = 2;
                    componentType = GL_FLOAT;
                    break;
                case VertexAttributeType::Float3:
                    componentCount = 3;
                    componentType = GL_FLOAT;
                    break;
                case VertexAttributeType::Float4:
                    componentCount = 4;
                    componentType = GL_FLOAT;
                    break;
                case VertexAttributeType::Int:
                    componentCount = 1;
                    componentType = GL_INT;
                    break;
                case VertexAttributeType::Int2:
                    componentCount = 2;
                    componentType = GL_INT;
                    break;
                case VertexAttributeType::Int3:
                    componentCount = 3;
                    componentType = GL_INT;
                    break;
                case VertexAttributeType::Int4:
                    componentCount = 4;
                    componentType = GL_INT;
                    break;
            }
            
            glEnableVertexAttribArray(i);
            if (componentType == GL_INT)
            {
                glVertexAttribIPointer(i, componentCount, componentType, format.stride, reinterpret_cast<void*>(attr.offset));
            }
            else
            {
                glVertexAttribPointer(i, componentCount, componentType, attr.normalized, format.stride, reinterpret_cast<void*>(attr.offset));
            }
        }
        
        // Create and bind index buffer if provided
        if (indexData && indexCount > 0)
        {
            glGenBuffers(1, &ibo);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indexCount * sizeof(uint32_t), indexData, GL_STATIC_DRAW);
        }
        else
        {
            ibo = 0;
        }
        
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        
        // Create vertex array record
        GPUVertexArrayHandle handle = nextVertexArrayHandle;
        nextVertexArrayHandle = GPUVertexArrayHandle(nextVertexArrayHandle.id + 1, nextVertexArrayHandle.generation);
        
        OpenGLVertexArray vaoData;
        vaoData.glId = vao;
        vaoData.vertexBuffer = vbo;
        vaoData.indexBuffer = ibo;
        vaoData.vertexCount = vertexCount;
        vaoData.indexCount = indexCount;
        
        vertexArrays[handle] = vaoData;
        
        Logger::Log(LogLevel::Info, ("OpenGLResourceManager::CreateVertexArray - Created VAO " + std::to_string(handle.id) + 
                                    " with " + std::to_string(vertexCount) + " vertices, " + 
                                    std::to_string(format.attributes.size()) + " attributes").c_str());
        
        return handle;
    }

    void OpenGLResourceManager::DestroyVertexArray(GPUVertexArrayHandle handle)
    {
        auto it = vertexArrays.find(handle);
        if (it != vertexArrays.end())
        {
            glDeleteVertexArrays(1, &it->second.glId);
            glDeleteBuffers(1, &it->second.vertexBuffer);
            if (it->second.indexBuffer != 0)
            {
                glDeleteBuffers(1, &it->second.indexBuffer);
            }
            vertexArrays.erase(it);
            Logger::Log(LogLevel::Info, ("OpenGLResourceManager::DestroyVertexArray - Destroyed VAO " + std::to_string(handle.id)).c_str());
        }
    }

    OpenGLVertexArray* OpenGLResourceManager::GetVertexArray(GPUVertexArrayHandle handle)
    {
        auto it = vertexArrays.find(handle);
        return (it != vertexArrays.end()) ? &it->second : nullptr;
    }

    const OpenGLVertexArray* OpenGLResourceManager::GetVertexArray(GPUVertexArrayHandle handle) const
    {
        auto it = vertexArrays.find(handle);
        return (it != vertexArrays.end()) ? &it->second : nullptr;
    }

    // ========================================
    // Shader Program Management
    // ========================================

    uint32_t OpenGLResourceManager::CreateShaderProgram(const char* vertexSource, const char* fragmentSource)
    {
        // Compile vertex shader
        GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertexShader, 1, &vertexSource, nullptr);
        glCompileShader(vertexShader);
        
        // Check vertex shader compilation
        GLint success;
        glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(vertexShader, 512, nullptr, infoLog);
            Logger::Log(LogLevel::Error, ("OpenGLResourceManager::CreateShaderProgram - Vertex shader compilation failed: " + std::string(infoLog)).c_str());
            glDeleteShader(vertexShader);
            return 0;
        }
        
        // Compile fragment shader
        GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragmentShader, 1, &fragmentSource, nullptr);
        glCompileShader(fragmentShader);
        
        // Check fragment shader compilation
        glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetShaderInfoLog(fragmentShader, 512, nullptr, infoLog);
            Logger::Log(LogLevel::Error, ("OpenGLResourceManager::CreateShaderProgram - Fragment shader compilation failed: " + std::string(infoLog)).c_str());
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            return 0;
        }
        
        // Link shader program
        GLuint program = glCreateProgram();
        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);
        
        // Check linking
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success)
        {
            char infoLog[512];
            glGetProgramInfoLog(program, 512, nullptr, infoLog);
            Logger::Log(LogLevel::Error, ("OpenGLResourceManager::CreateShaderProgram - Shader program linking failed: " + std::string(infoLog)).c_str());
            glDeleteShader(vertexShader);
            glDeleteShader(fragmentShader);
            glDeleteProgram(program);
            return 0;
        }
        
        // Clean up individual shaders
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        
        // Create shader program record
        uint32_t shaderId = nextShaderId++;
        
        OpenGLShaderProgram shaderProgram;
        shaderProgram.glId = program;
        shaderProgram.vertexSource = vertexSource ? vertexSource : "";
        shaderProgram.fragmentSource = fragmentSource ? fragmentSource : "";
        
        shaderPrograms[shaderId] = shaderProgram;
        
        Logger::Log(LogLevel::Info, ("OpenGLResourceManager::CreateShaderProgram - Created shader program " + std::to_string(shaderId)).c_str());
        
        return shaderId;
    }

    void OpenGLResourceManager::DestroyShaderProgram(uint32_t shaderId)
    {
        auto it = shaderPrograms.find(shaderId);
        if (it != shaderPrograms.end())
        {
            glDeleteProgram(it->second.glId);
            shaderPrograms.erase(it);
            Logger::Log(LogLevel::Info, ("OpenGLResourceManager::DestroyShaderProgram - Destroyed shader program " + std::to_string(shaderId)).c_str());
        }
    }

    OpenGLShaderProgram* OpenGLResourceManager::GetShaderProgram(uint32_t shaderId)
    {
        auto it = shaderPrograms.find(shaderId);
        return (it != shaderPrograms.end()) ? &it->second : nullptr;
    }

    const OpenGLShaderProgram* OpenGLResourceManager::GetShaderProgram(uint32_t shaderId) const
    {
        auto it = shaderPrograms.find(shaderId);
        return (it != shaderPrograms.end()) ? &it->second : nullptr;
    }

    // ========================================
    // Cleanup
    // ========================================

    void OpenGLResourceManager::DestroyAllResources()
    {
        Logger::Log(LogLevel::Info, "OpenGLResourceManager::DestroyAllResources - Destroying all resources");
        
        // Destroy all framebuffers first (they depend on textures)
        for (auto& pair : framebuffers)
        {
            glDeleteFramebuffers(1, &pair.second.glId);
        }
        framebuffers.clear();
        
        // Destroy all vertex arrays (they depend on buffers)
        for (auto& pair : vertexArrays)
        {
            glDeleteVertexArrays(1, &pair.second.glId);
            glDeleteBuffers(1, &pair.second.vertexBuffer);
            if (pair.second.indexBuffer != 0)
            {
                glDeleteBuffers(1, &pair.second.indexBuffer);
            }
        }
        vertexArrays.clear();
        
        // Destroy all shader programs
        for (auto& pair : shaderPrograms)
        {
            glDeleteProgram(pair.second.glId);
        }
        shaderPrograms.clear();
        
        // Destroy all textures last (they are independent)
        for (auto& pair : textures)
        {
            glDeleteTextures(1, &pair.second.glId);
        }
        textures.clear();
        
        Logger::Log(LogLevel::Info, "OpenGLResourceManager::DestroyAllResources - All resources destroyed");
    }

} // namespace Engine
