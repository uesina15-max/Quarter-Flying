#pragma once

#include "../platform/IPlatform.h"
#include <memory>

namespace Engine
{
    // ========================================
    // Platform Factory
    // ========================================
    
    // 플랫폼별 구현체를 생성하는 팩토리 클래스
    // 컴파일 타임에 적절한 플랫폼 구현을 선택합니다.
    
    class PlatformFactory
    {
    public:
        // 현재 플랫폼에 맞는 IPlatform 구현체를 생성합니다.
        static std::unique_ptr<IPlatform> CreatePlatform();
    };

} // namespace Engine
