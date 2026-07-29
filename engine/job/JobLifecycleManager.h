#pragma once

#include "Job.h"
#include <vector>
#include <mutex>
#include <atomic>

namespace Engine
{
    // ========================================
    // Job Lifecycle Manager
    // ========================================
    
    // Job의 전체 수명주기를 관리하는 컴포넌트
    // 
    // 책임:
    // - Job 풀 관리 (할당 및 재사용)
    // - 활성 Job 목록 관리
    // - Job 핸들 생성 (세대 번호 포함)
    // - Job 조회 (핸들 기반)
    // 
    // 스레드 안전성:
    // - Job 풀: mutex로 보호
    // - 활성 Job 목록: mutex로 보호
    // - 핸들 생성: atomic 연산 사용
    
    class JobLifecycleManager
    {
    public:
        JobLifecycleManager();
        ~JobLifecycleManager();

        // Job 생성 및 초기화
        // func: 실행할 함수
        // data: 함수에 전달할 데이터
        // numDependencies: 의존성 개수
        // 반환값: 생성된 Job 포인터
        Job* CreateJob(JobFunction func, void* data, uint32_t numDependencies);

        // Job 완료 처리 (풀에 반환)
        // job: 완료된 Job
        void RetireJob(Job* job);

        // Job 핸들로 Job 찾기 (스레드 안전)
        // handle: 찾을 Job의 핸들
        // 반환값: Job 포인터 (없으면 nullptr)
        Job* FindJob(JobHandle handle) const;

        // 활성 Job 목록 조회
        // 반환값: 활성 Job 목록 (읽기 전용)
        const std::vector<Job*>& GetActiveJobs() const;

        // 실패한 Job 정리
        // job: 정리할 Job
        void CleanupFailedJob(Job* job);

        // 모든 Job 정리 (종료 시)
        void Clear();

    private:
        // Job 할당 (풀에서 재사용 또는 새로 생성)
        Job* AllocateJob();

        // Job 핸들로 Job 찾기 (락 없이 - 이미 락을 잡은 상태에서 호출)
        Job* FindJobUnsafe(JobHandle handle) const;

        // Job 풀 (재사용을 위한 풀)
        std::vector<Job*> jobPool;
        mutable std::mutex jobPoolMutex;

        // 활성 Job 목록 (핸들로 조회)
        std::vector<Job*> activeJobs;
        mutable std::mutex activeJobsMutex;

        // Job 핸들 생성용
        std::atomic<uint32_t> nextJobID;
        std::atomic<uint32_t> nextGeneration;
    };

} // namespace Engine
