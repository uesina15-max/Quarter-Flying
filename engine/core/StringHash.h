#pragma once

#include <cstdint>
#include <string_view>
#include <string>
#include <functional>
#include <unordered_map>
#include <cassert>

namespace Engine
{
    // ========================================
    // StringHash (32-bit FNV-1a Compile-time Hash)
    // ========================================
    //
    // Responsibilities:
    // - Provide compile-time (constexpr) hashing of string literals
    // - Provide L1 cache-friendly O(1) integer comparison for symbols and names
    // - Serve as key in L-value L1 lookups (unordered_map, etc.)
    //
    // Thread Safety:
    // - Immutable value type (safe for concurrent read across threads)

    class StringHash
    {
    public:
        constexpr StringHash() noexcept : m_Hash(0) {}
        
        constexpr StringHash(uint32_t hash) noexcept : m_Hash(hash) {}

        constexpr StringHash(const char* str) noexcept
            : m_Hash(CalculateFNV1a(str))
        {
        }

        constexpr StringHash(std::string_view str) noexcept
            : m_Hash(CalculateFNV1a(str.data(), str.length()))
        {
        }

        StringHash(const std::string& str) noexcept
            : m_Hash(CalculateFNV1a(str.data(), str.length()))
        {
        }

        constexpr uint32_t GetValue() const noexcept { return m_Hash; }
        constexpr explicit operator uint32_t() const noexcept { return m_Hash; }
        constexpr bool IsValid() const noexcept { return m_Hash != 0; }

        constexpr bool operator==(const StringHash& other) const noexcept { return m_Hash == other.m_Hash; }
        constexpr bool operator!=(const StringHash& other) const noexcept { return m_Hash != other.m_Hash; }
        constexpr bool operator<(const StringHash& other) const noexcept { return m_Hash < other.m_Hash; }
        constexpr bool operator>(const StringHash& other) const noexcept { return m_Hash > other.m_Hash; }
        constexpr bool operator<=(const StringHash& other) const noexcept { return m_Hash <= other.m_Hash; }
        constexpr bool operator>=(const StringHash& other) const noexcept { return m_Hash >= other.m_Hash; }

        // Compile-time FNV-1a calculation
        static constexpr uint32_t CalculateFNV1a(const char* str) noexcept
        {
            if (!str || *str == '\0') return 0;
            
            uint32_t hash = 2166136261u; // FNV offset basis
            while (*str)
            {
                hash ^= static_cast<uint8_t>(*str++);
                hash *= 16777619u;       // FNV prime
            }
            return hash;
        }

        static constexpr uint32_t CalculateFNV1a(const char* str, size_t len) noexcept
        {
            if (!str || len == 0) return 0;
            
            uint32_t hash = 2166136261u; // FNV offset basis
            for (size_t i = 0; i < len; ++i)
            {
                hash ^= static_cast<uint8_t>(str[i]);
                hash *= 16777619u;       // FNV prime
            }
            return hash;
        }

    private:
        uint32_t m_Hash;
    };

    namespace Literals
    {
        constexpr StringHash operator""_hash(const char* str, size_t len) noexcept
        {
            return StringHash(StringHash::CalculateFNV1a(str, len));
        }
    }

    // ========================================
    // Hash Collision Registry (Debug/Editor builds only)
    // ========================================
    //
    // Responsibilities:
    // - Detect hash collisions at runtime in debug/editor builds
    // - Assert immediately if different strings produce the same hash
    // - Zero overhead in release builds (completely compiled out)
    //
    // Thread Safety:
    // - Read-only after registration (safe for concurrent read)

#ifdef GE_DEBUG
    class HashCollisionRegistry
    {
    public:
        static void Register(const char* str, StringHash hash)
        {
            auto it = s_Registry.find(hash);
            if (it != s_Registry.end())
            {
                // Collision detected: different string, same hash
                if (std::strcmp(it->second, str) != 0)
                {
                    assert(false && "StringHash collision detected! Different strings produced the same hash.");
                }
            }
            else
            {
                s_Registry[hash] = str;
            }
        }

        static const char* GetString(StringHash hash)
        {
            auto it = s_Registry.find(hash);
            return (it != s_Registry.end()) ? it->second : nullptr;
        }

        static void Clear() { s_Registry.clear(); }

    private:
        static std::unordered_map<StringHash, const char*> s_Registry;
    };
#endif

} // namespace Engine

// Hash specialization for Engine::StringHash
namespace std
{
    template<>
    struct hash<Engine::StringHash>
    {
        size_t operator()(const Engine::StringHash& hash) const noexcept
        {
            return static_cast<size_t>(hash.GetValue());
        }
    };
}
