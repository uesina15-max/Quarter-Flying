#pragma once

#include <cstdint>
#include <cmath>
#include <functional>

namespace Engine
{
    // ========================================
    // Math Types
    // ========================================

    struct Vec3
    {
        float x, y, z;

        Vec3() : x(0.0f), y(0.0f), z(0.0f) {}
        Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

        Vec3 operator+(const Vec3& other) const
        {
            return Vec3(x + other.x, y + other.y, z + other.z);
        }

        Vec3 operator-(const Vec3& other) const
        {
            return Vec3(x - other.x, y - other.y, z - other.z);
        }

        Vec3 operator*(float scalar) const
        {
            return Vec3(x * scalar, y * scalar, z * scalar);
        }

        float Dot(const Vec3& other) const
        {
            return x * other.x + y * other.y + z * other.z;
        }

        Vec3 Cross(const Vec3& other) const
        {
            return Vec3(
                y * other.z - z * other.y,
                z * other.x - x * other.z,
                x * other.y - y * other.x
            );
        }

        float Length() const
        {
            return std::sqrt(x * x + y * y + z * z);
        }

        Vec3 Normalized() const
        {
            float len = Length();
            if (len > 0.0f)
                return Vec3(x / len, y / len, z / len);
            return Vec3(0.0f, 0.0f, 0.0f);
        }
    };

    struct Quaternion
    {
        float x, y, z, w;

        Quaternion() : x(0.0f), y(0.0f), z(0.0f), w(1.0f) {}
        Quaternion(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

        Quaternion operator*(const Quaternion& other) const
        {
            return Quaternion(
                w * other.x + x * other.w + y * other.z - z * other.y,
                w * other.y - x * other.z + y * other.w + z * other.x,
                w * other.z + x * other.y - y * other.x + z * other.w,
                w * other.w - x * other.x - y * other.y - z * other.z
            );
        }

        float Length() const
        {
            return std::sqrt(x * x + y * y + z * z + w * w);
        }

        Quaternion Normalized() const
        {
            float len = Length();
            if (len > 0.0f)
                return Quaternion(x / len, y / len, z / len, w / len);
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }

        static Quaternion Identity()
        {
            return Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
        }
    };

    // ========================================
    // Handle Types
    // ========================================

    template<typename Tag>
    struct Handle
    {
        uint32_t id;
        uint32_t generation;

        Handle() : id(0), generation(0) {}
        Handle(uint32_t id, uint32_t generation) : id(id), generation(generation) {}

        bool IsValid() const { return id != 0; }

        bool operator==(const Handle& other) const
        {
            return id == other.id && generation == other.generation;
        }

        bool operator!=(const Handle& other) const
        {
            return !(*this == other);
        }
    };

    // Handle type tags
    struct WindowHandleTag {};
    struct MeshHandleTag {};
    struct MaterialHandleTag {};
    struct TextureHandleTag {};
    struct BufferHandleTag {};
    struct GPUTextureHandleTag {};
    struct GPUBufferHandleTag {};
    struct GPUVertexArrayHandleTag {};

    // ========================================
    // Handle Resource Domains & Specific Handle Types
    // ========================================
    //
    // ARCHITECTURAL INVARIANT (Resource Domain Separation):
    // Each handle represents a strictly distinct resource domain:
    // - TextureHandle / BufferHandle: Asset/Runtime Logical Reference (e.g., material-bound asset)
    // - RGTextureHandle (in RenderGraph.h): RenderGraph Virtual Resource (frame graph compilation)
    // - GPUTextureHandle / GPUBufferHandle: Physical Backend Allocation (GPU backend texture/buffer ID)
    //
    // Conversions between domains must occur explicitly at designated bridge points
    // (e.g., RenderGraph physical resource materialization). Implicit conversions are prevented at compile-time.
    //
    // Note: Naming of TextureHandle / BufferHandle is recorded as a tech debt candidate
    // for future refinement (e.g., LogicalTextureHandle / AssetTextureHandle).

    using WindowHandle = Handle<WindowHandleTag>;
    using MeshHandle = Handle<MeshHandleTag>;
    using MaterialHandle = Handle<MaterialHandleTag>;
    using TextureHandle = Handle<TextureHandleTag>;       // Asset/Runtime Logical Reference
    using BufferHandle = Handle<BufferHandleTag>;         // Asset/Runtime Logical Reference

    // ========================================
    // GPU Resource Types
    // ========================================

    enum class TextureFormat : uint8_t
    {
        RGBA8,
        RGBA16F,
        RGBA32F,
        Depth24Stencil8,
        Depth32F
    };

    enum class TextureUsage : uint8_t
    {
        RenderTarget = 1 << 0,
        DepthStencil = 1 << 1,
        ShaderResource = 1 << 2,
        UnorderedAccess = 1 << 3
    };

    using GPUTextureHandle = Handle<GPUTextureHandleTag>; // Physical Backend Allocation
    using GPUBufferHandle = Handle<GPUBufferHandleTag>;   // Physical Backend Allocation
    using GPUVertexArrayHandle = Handle<GPUVertexArrayHandleTag>; // Vertex Array Object

} // namespace Engine

// Hash specialization for Handle types
namespace std
{
    template<typename Tag>
    struct hash<Engine::Handle<Tag>>
    {
        size_t operator()(const Engine::Handle<Tag>& handle) const noexcept
        {
            // Combine id and generation using a simple hash function
            return hash<uint32_t>{}(handle.id) ^ (hash<uint32_t>{}(handle.generation) << 1);
        }
    };
}
