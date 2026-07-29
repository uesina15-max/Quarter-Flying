#pragma once

#include "Job.h"
#include "ValidationLayer.h"
#include "JobLifecycleManager.h"
#include "DependencyResolver.h"
#include "JobScheduler.h"
#include "../core/memory/FrameAllocator.h"
#include "../core/logging/Logger.h"
#include <memory>
#include <atomic>
#include <utility>

namespace Engine
{
    // ========================================
    // Job System
    // ========================================
    
    // 멀티스레드 작업 스케줄링 시스템
    // 
    // 특징:
    // - Work Stealing으로 부하 분산
    // - Lock-free 큐 사용
    // - FrameAllocator에서 Job 메모리 할당
    // - 의존성 그래프 지원 (작업 6에서 구현)
    // 
    // 사용 예시:
    // JobHandle handle = jobSystem->Dispatch(MyFunction, myData);
    // jobSystem->Wait(handle);
    
    class JobSystem
    {
    public:
        JobSystem();
        ~JobSystem();

        // JobSystem 초기화
        // numThreads: 워커 스레드 수 (0 = 하드웨어 스레드 수 - 1)
        void Initialize(uint32_t numThreads = 0);

        // JobSystem 종료
        void Shutdown();

        // Job 디스패치 (큐에 추가)
        // func: 실행할 함수
        // data: 함수에 전달할 데이터
        // dependencies: 의존하는 Job 목록
        // numDependencies: 의존성 개수
        // 반환값: Job 핸들
        JobHandle Dispatch(
            JobFunction func,
            void* data,
            const JobHandle* dependencies = nullptr,
            uint32_t numDependencies = 0
        );

        // Job 완료 대기
        // handle: 대기할 Job의 핸들
        void Wait(JobHandle handle);

        // Job 완료 여부 확인
        // handle: 확인할 Job의 핸들
        // 반환값: true = 완료, false = 진행 중
        bool IsComplete(JobHandle handle);

        // FrameAllocator 설정
        void SetFrameAllocator(FrameAllocator* allocator);

        // 워커 스레드 수 조회
        uint32_t GetWorkerThreadCount() const;

        // Job 데이터 할당 헬퍼
        // T: 데이터 타입
        // args: 생성자 인자
        // 반환값: 할당된 데이터 포인터
        template<typename T, typename... Args>
        T* AllocateJobData(Args&&... args);

    private:
        // ========================================
        // Component Instances
        // ========================================
        
        // Job 수명주기 관리 컴포넌트
        std::unique_ptr<JobLifecycleManager> lifecycleManager;
        
        // 의존성 해결 컴포넌트
        std::unique_ptr<DependencyResolver> dependencyResolver;
        
        // Job 스케줄러 컴포넌트
        std::unique_ptr<JobScheduler> scheduler;

        // ========================================
        // System State
        // ========================================
        
        // 시스템 초기화 여부
        std::atomic<bool> initialized;
        
        // FrameAllocator (Job 데이터 할당용)
        FrameAllocator* frameAllocator;

        // ========================================
        // Callback Handlers
        // ========================================
        
        // Job 완료 콜백 핸들러
        void OnJobComplete(Job* job);
    };

    // ========================================
    // Template Implementation
    // ========================================

    template<typename T, typename... Args>
    T* JobSystem::AllocateJobData(Args&&... args)
    {
        if (!frameAllocator)
        {
            // FrameAllocator가 없으면 일반 힙 할당
            // 경고: 이 경우 Job 완료 후 메모리가 자동 해제되지 않으므로 누수가 발생할 수 있습니다!
            // 사용자가 명시적으로 메모리 소유권을 관리해야 합니다.
            Logger::Log(LogLevel::Warning, "AllocateJobData: Fallback to heap allocation. Memory leak possible if not manually freed.");
            return new T(std::forward<Args>(args)...);
        }

        // FrameAllocator에서 메모리 할당
        void* memory = frameAllocator->Allocate(sizeof(T), alignof(T));
        
        // Placement new로 객체 생성
        return new (memory) T(std::forward<Args>(args)...);
    }

} // namespace Engine
