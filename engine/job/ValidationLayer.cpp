#include "ValidationLayer.h"
#include "../core/logging/Logger.h"
#include <thread>

namespace Engine
{
    // ========================================
    // Dispatch Validation
    // ========================================
    
    ValidationLayer::DispatchValidation ValidationLayer::ValidateDispatch(
        JobFunction func,
        bool isInitialized,
        bool isRunning)
    {
        // Requirement 5.1: 함수 포인터가 null인지 검증
        if (func == nullptr)
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Error, "ValidationLayer: Dispatch failed - Job function pointer is null");
            return DispatchValidation(false, "Job function pointer is null");
        }
        
        // Requirement 5.3: 시스템이 초기화되지 않았는지 검증
        if (!isInitialized)
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Error, "ValidationLayer: Dispatch failed - JobSystem is not initialized");
            return DispatchValidation(false, "JobSystem is not initialized");
        }
        
        // 시스템이 실행 중이 아닌지 검증
        if (!isRunning)
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Error, "ValidationLayer: Dispatch failed - JobSystem is not running");
            return DispatchValidation(false, "JobSystem is not running");
        }
        
        return DispatchValidation(true);
    }
    
    // ========================================
    // Dependency Validation
    // ========================================
    
    ValidationLayer::DependencyValidation ValidationLayer::ValidateDependencies(
        const JobHandle* dependencies,
        uint32_t numDependencies)
    {
        // 의존성이 없으면 검증 통과
        if (numDependencies == 0)
        {
            return DependencyValidation(true);
        }
        
        // 의존성 배열이 null인지 검증
        if (dependencies == nullptr)
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Error, "ValidationLayer: Dependency validation failed - Dependency array is null but numDependencies = %u", numDependencies);
            return DependencyValidation(false, "Dependency array is null but numDependencies > 0");
        }
        
        // Requirement 5.2: 각 의존성 핸들이 유효한지 검증
        for (uint32_t i = 0; i < numDependencies; ++i)
        {
            if (!dependencies[i].IsValid())
            {
                // Requirement 6.2: Log validation failure with context
                Logger::Log(LogLevel::Error, "ValidationLayer: Dependency validation failed - Invalid handle at index %u (id=%u, gen=%u)", 
                           i, dependencies[i].id, dependencies[i].generation);
                return DependencyValidation(false, "Invalid dependency handle detected");
            }
        }
        
        return DependencyValidation(true);
    }
    
    // ========================================
    // Wait Validation
    // ========================================
    
    ValidationLayer::WaitValidation ValidationLayer::ValidateWait(JobHandle handle)
    {
        // Requirement 5.5: Job 핸들이 유효한지 검증
        if (!handle.IsValid())
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Warning, "ValidationLayer: Wait validation failed - Invalid job handle (id=%u, gen=%u)", 
                       handle.id, handle.generation);
            return WaitValidation(false, "Invalid job handle");
        }
        
        return WaitValidation(true);
    }
    
    // ========================================
    // Initialization Validation
    // ========================================
    
    ValidationLayer::InitValidation ValidationLayer::ValidateInitialization(
        uint32_t requestedThreads,
        bool alreadyInitialized)
    {
        // 이미 초기화되었는지 검증
        if (alreadyInitialized)
        {
            // Requirement 6.2: Log validation failure with context
            Logger::Log(LogLevel::Warning, "ValidationLayer: Initialization failed - JobSystem is already initialized");
            return InitValidation(false, 0, "JobSystem is already initialized");
        }
        
        // Requirement 5.4: 스레드 수 검증 및 조정
        uint32_t hardwareThreads = std::thread::hardware_concurrency();
        uint32_t adjustedThreadCount = requestedThreads;
        
        // 0이면 하드웨어 스레드 수 - 1 사용
        if (requestedThreads == 0)
        {
            adjustedThreadCount = (hardwareThreads > 1) ? (hardwareThreads - 1) : 1;
            Logger::Log(LogLevel::Info, "ValidationLayer: Thread count auto-adjusted from 0 to %u (hardware threads: %u)", 
                       adjustedThreadCount, hardwareThreads);
        }
        // 하드웨어 스레드 수를 초과하면 조정
        else if (requestedThreads > hardwareThreads)
        {
            adjustedThreadCount = hardwareThreads;
            Logger::Log(LogLevel::Warning, "ValidationLayer: Thread count adjusted from %u to %u (exceeds hardware threads)", 
                       requestedThreads, hardwareThreads);
        }
        
        // 최소 1개의 스레드는 필요
        if (adjustedThreadCount == 0)
        {
            adjustedThreadCount = 1;
            Logger::Log(LogLevel::Warning, "ValidationLayer: Thread count adjusted to minimum value of 1");
        }
        
        return InitValidation(true, adjustedThreadCount);
    }

} // namespace Engine
