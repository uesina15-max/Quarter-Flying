#pragma once

#include "EngineError.h"

namespace Engine
{
    // ========================================
    // Subsystem Base Interface
    // ========================================
    
    // Subsystem은 엔진의 독립적인 기능 모듈을 나타냅니다.
    // 예: Renderer, Physics, Audio 등
    // 
    // 수명주기:
    // 1. Initialize() - 서브시스템 초기화 (Result<void> 반환)
    // 2. Tick() - 매 프레임 업데이트
    // 3. LateTick() - 모든 Tick 이후 업데이트 (선택 사항)
    // 4. Shutdown() - 서브시스템 종료 (noexcept 보장 및 Best-Effort 리소스 해제)
    
    class Subsystem
    {
    public:
        virtual ~Subsystem() = default;

        // 서브시스템 초기화
        // Engine::Initialize()에서 등록 순서대로 호출됩니다.
        // 실패 시 에러가 담긴 Result를 반환하여 엔진 초기화를 중단하고 롤백합니다.
        virtual Result<void> Initialize() = 0;

        // 서브시스템 종료
        // Engine::Shutdown() 및 초기화 실패 시 롤백 과정에서 역순으로 호출됩니다.
        // 계약: 반드시 noexcept 및 Best-Effort(예외/실패를 외부로 던지지 않고 무조건 해제 진행)를 보장해야 합니다.
        virtual void Shutdown() noexcept = 0;

        // 매 프레임 업데이트
        // deltaTime: 이전 프레임으로부터 경과한 시간 (초 단위)
        virtual void Tick(float deltaTime) {}

        // 모든 Tick 이후 업데이트 (선택 사항)
        // 다른 서브시스템의 Tick이 완료된 후 실행됩니다.
        virtual void LateTick(float deltaTime) {}
    };

} // namespace Engine
