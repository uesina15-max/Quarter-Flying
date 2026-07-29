#pragma once

#include "StringHash.h"
#include <string_view>
#include <string>

namespace Engine
{
    // ========================================
    // StringHashAdapter - Gradual Migration Layer
    // ========================================
    //
    // Responsibilities:
    // - Provide seamless transition between std::string and StringHash
    // - Support both legacy string-based and new hash-based APIs
    // - Enable zero-risk gradual migration across subsystems
    //
    // Usage Pattern:
    // - Phase 1: Use adapter to accept both string and hash inputs
    // - Phase 2: Gradually migrate subsystems to use hash internally
    // - Phase 3: Remove adapter once migration is complete

    class StringHashAdapter
    {
    public:
        // Construct from various input types
        constexpr StringHashAdapter() noexcept : m_Hash(0) {}
        constexpr StringHashAdapter(StringHash hash) noexcept : m_Hash(hash) {}
        constexpr StringHashAdapter(uint32_t hash) noexcept : m_Hash(hash) {}
        constexpr StringHashAdapter(const char* str) noexcept : m_Hash(str) {}
        constexpr StringHashAdapter(std::string_view str) noexcept : m_Hash(str) {}
        StringHashAdapter(const std::string& str) noexcept : m_Hash(str) {}

        // Implicit conversion to StringHash
        constexpr operator StringHash() const noexcept { return m_Hash; }
        constexpr uint32_t GetValue() const noexcept { return m_Hash.GetValue(); }

        // Comparison operators
        constexpr bool operator==(const StringHashAdapter& other) const noexcept 
        { 
            return m_Hash == other.m_Hash; 
        }
        constexpr bool operator!=(const StringHashAdapter& other) const noexcept 
        { 
            return m_Hash != other.m_Hash; 
        }
        constexpr bool operator<(const StringHashAdapter& other) const noexcept 
        { 
            return m_Hash < other.m_Hash; 
        }

        // String comparison (for legacy compatibility)
        bool operator==(std::string_view str) const noexcept
        {
            return m_Hash == StringHash(str);
        }

        // Get original string (debug/editor only)
#ifdef GE_DEBUG
        const char* GetDebugString() const
        {
            return HashCollisionRegistry::GetString(m_Hash);
        }
#endif

    private:
        StringHash m_Hash;
    };

    // Hash specialization for StringHashAdapter
}

namespace std
{
    template<>
    struct hash<Engine::StringHashAdapter>
    {
        size_t operator()(const Engine::StringHashAdapter& adapter) const noexcept
        {
            return static_cast<size_t>(adapter.GetValue());
        }
    };
}
