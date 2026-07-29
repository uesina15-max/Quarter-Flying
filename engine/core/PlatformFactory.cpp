#include "PlatformFactory.h"
#include <memory>

// Platform-specific includes
#include "../platform/GLFWPlatform.h"
#ifdef _WIN32
    #include "../platform/Win32Platform.h"
#endif

namespace Engine
{
    std::unique_ptr<IPlatform> PlatformFactory::CreatePlatform()
    {
        // Use GLFWPlatform for OpenGL context creation
        return std::make_unique<GLFWPlatform>();
    }

} // namespace Engine
