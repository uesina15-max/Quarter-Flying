#pragma once

#include "Job.h"
#include <cstdint>

namespace Engine
{
    // ========================================
    // ValidationLayer
    // ========================================
    
    // Job System 작업에 대한 입력 및 상태 검증을 담당하는 컴포넌트
    // 
    // 특징:
    // - 정적 메서드로 구성된 상태 없는 검증 레이어
    // - 구조화된 검증 결과 반환 (오류 메시지 포함)
    // - 비즈니스 로직과 검증 로직 분리
    // 
    // 사용 예시:
    // auto result = ValidationLayer::ValidateDispatch(func, initialized, running);
    // if (!result.isValid) {
    //     Logger::Log(LogLevel::Error, result.errorMessage);
    // }
    
    class ValidationLayer
    {
    public:
        // ========================================
        // Validation Result Structures
        // ========================================
        
        // Dispatch 검증 결과
        struct DispatchValidation
        {
            bool isValid;
            const char* errorMessage;
            
            DispatchValidation(bool valid, const char* message = nullptr)
                : isValid(valid)
                , errorMessage(message)
            {}
        };
        
        // 의존성 검증 결과
        struct DependencyValidation
        {
            bool isValid;
            const char* errorMessage;
            
            DependencyValidation(bool valid, const char* message = nullptr)
                : isValid(valid)
                , errorMessage(message)
            {}
        };
        
        // Wait 검증 결과
        struct WaitValidation
        {
            bool isValid;
            const char* errorMessage;
            
            WaitValidation(bool valid, const char* message = nullptr)
                : isValid(valid)
                , errorMessage(message)
            {}
        };
        
        // 초기화 검증 결과
        struct InitValidation
        {
            bool isValid;
            uint32_t adjustedThreadCount;
            const char* errorMessage;
            
            InitValidation(bool valid, uint32_t threadCount = 0, const char* message = nullptr)
                : isValid(valid)
                , adjustedThreadCount(threadCount)
                , errorMessage(message)
            {}
        };
        
        // ========================================
        // Validation Methods
        // ========================================
        
        // Dispatch 매개변수 검증
        // func: Job 함수 포인터
        // isInitialized: 시스템 초기화 여부
        // isRunning: 시스템 실행 여부
        // 반환값: 검증 결과
        static DispatchValidation ValidateDispatch(
            JobFunction func,
            bool isInitialized,
            bool isRunning
        );
        
        // 의존성 매개변수 검증
        // dependencies: 의존성 핸들 배열
        // numDependencies: 의존성 개수
        // 반환값: 검증 결과
        static DependencyValidation ValidateDependencies(
            const JobHandle* dependencies,
            uint32_t numDependencies
        );
        
        // Wait 매개변수 검증
        // handle: 대기할 Job 핸들
        // 반환값: 검증 결과
        static WaitValidation ValidateWait(JobHandle handle);
        
        // 초기화 매개변수 검증
        // requestedThreads: 요청된 스레드 수
        // alreadyInitialized: 이미 초기화되었는지 여부
        // 반환값: 검증 결과 (조정된 스레드 수 포함)
        static InitValidation ValidateInitialization(
            uint32_t requestedThreads,
            bool alreadyInitialized
        );
    };

} // namespace Engine
