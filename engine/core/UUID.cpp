#include "UUID.h"
#include <random>

namespace Engine {
    static std::random_device s_RandomDevice;
    static std::mt19937_64 s_Engine(s_RandomDevice());
    static std::uniform_int_distribution<uint64_t> s_UniformDistribution;

    UUID::UUID() : value(0) {}
    
    UUID::UUID(uint64_t value) : value(value) {}
    
    UUID UUID::Generate()
    {
        uint64_t uuid = s_UniformDistribution(s_Engine);
        // Ensure UUID is never 0, as 0 might be considered invalid in some contexts
        if (uuid == 0) {
            uuid = 1;
        }
        return UUID(uuid);
    }
    
    std::string UUID::ToString() const
    {
        std::stringstream ss;
        ss << std::hex << std::setfill('0') << std::setw(16) << value;
        return ss.str();
    }
}
