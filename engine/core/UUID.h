#pragma once

#include <cstdint>
#include <string>
#include <random>
#include <sstream>
#include <iomanip>

namespace Engine {

    // ========================================
    // UUID (Universally Unique Identifier)
    // ========================================

    class UUID
    {
    public:
        UUID();
        UUID(uint64_t value);
        
        // Generate a new random UUID
        static UUID Generate();
        
        // Get the raw 64-bit value
        uint64_t GetValue() const { return value; }
        operator uint64_t() const { return value; }
        
        // Convert to string representation
        std::string ToString() const;
        
        // Comparison operators
        bool operator==(const UUID& other) const { return value == other.value; }
        bool operator!=(const UUID& other) const { return value != other.value; }
        bool operator<(const UUID& other) const { return value < other.value; }
        
        // Check if UUID is valid (non-zero)
        bool IsValid() const { return value != 0; }

    private:
        uint64_t value;
    };

    // Legacy function for backward compatibility
    inline uint64_t GenerateUUID()
    {
        return ::Engine::UUID::Generate().GetValue();
    }

} // namespace Engine

// Hash specialization for UUID
namespace std
{
    template<>
    struct hash<::Engine::UUID>
    {
        size_t operator()(const ::Engine::UUID& uuid) const noexcept
        {
            return hash<uint64_t>{}(uuid.GetValue());
        }
    };
}
