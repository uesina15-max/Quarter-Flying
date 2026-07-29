#include <gtest/gtest.h>
#include <type_traits>
#include "../core/Types.h"
#include "../renderer/RenderGraph.h"

namespace Engine
{
    // ========================================
    // Handle Type Contract Verification (Static Assert & Type Traits)
    // ========================================

    // Verify TextureHandle vs GPUTextureHandle vs RGTextureHandle domain separation
    static_assert(!std::is_convertible_v<RGTextureHandle, GPUTextureHandle>, "RGTextureHandle must not implicitly convert to GPUTextureHandle");
    static_assert(!std::is_convertible_v<GPUTextureHandle, RGTextureHandle>, "GPUTextureHandle must not implicitly convert to RGTextureHandle");
    static_assert(!std::is_convertible_v<TextureHandle, GPUTextureHandle>, "TextureHandle must not implicitly convert to GPUTextureHandle");
    static_assert(!std::is_convertible_v<GPUTextureHandle, TextureHandle>, "GPUTextureHandle must not implicitly convert to TextureHandle");
    static_assert(!std::is_convertible_v<TextureHandle, RGTextureHandle>, "TextureHandle must not implicitly convert to RGTextureHandle");
    static_assert(!std::is_convertible_v<RGTextureHandle, TextureHandle>, "RGTextureHandle must not implicitly convert to TextureHandle");

    // Verify BufferHandle vs GPUBufferHandle domain separation
    static_assert(!std::is_convertible_v<BufferHandle, GPUBufferHandle>, "BufferHandle must not implicitly convert to GPUBufferHandle");
    static_assert(!std::is_convertible_v<GPUBufferHandle, BufferHandle>, "GPUBufferHandle must not implicitly convert to BufferHandle");

    // Verify identical types still convert to themselves
    static_assert(std::is_convertible_v<RGTextureHandle, RGTextureHandle>, "RGTextureHandle must convert to itself");
    static_assert(std::is_convertible_v<GPUTextureHandle, GPUTextureHandle>, "GPUTextureHandle must convert to itself");
    static_assert(std::is_convertible_v<TextureHandle, TextureHandle>, "TextureHandle must convert to itself");
    static_assert(std::is_convertible_v<BufferHandle, BufferHandle>, "BufferHandle must convert to itself");
    static_assert(std::is_convertible_v<GPUBufferHandle, GPUBufferHandle>, "GPUBufferHandle must convert to itself");

    TEST(HandleTypeContractTest, DomainSeparationTraits)
    {
        EXPECT_FALSE((std::is_convertible_v<RGTextureHandle, GPUTextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<GPUTextureHandle, RGTextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<TextureHandle, GPUTextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<GPUTextureHandle, TextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<TextureHandle, RGTextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<RGTextureHandle, TextureHandle>));
        EXPECT_FALSE((std::is_convertible_v<BufferHandle, GPUBufferHandle>));
        EXPECT_FALSE((std::is_convertible_v<GPUBufferHandle, BufferHandle>));
    }
} // namespace Engine
