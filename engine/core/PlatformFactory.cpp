#include "PlatformFactory.h"
#include <memory>

// Platform-specific includes
#ifdef _WIN32
    #include "../platform/Win32Platform.h"
#endif

namespace Engine
{
    std::unique_ptr<IPlatform> PlatformFactory::CreatePlatform()
    {
#ifdef _WIN32
        return std::make_unique<Win32Platform>();
#else
        return nullptr;
#endif
    }

} // namespace Engine
