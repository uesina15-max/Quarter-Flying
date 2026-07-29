#pragma once

#include "Job.h"
#include <vector>

namespace Engine
{
    // Forward declaration
    class JobLifecycleManager;

    // ========================================
    // Dependency Resolver
    // ========================================
    
    // Job 의존성 관리 컴포넌트
    // 
    // 책임:
    // - 의존성 그래프 구축 및 유지
    // - 의존성 검증 (순환 의존성 감지)
    // - 의존성 카운터 관리
    // - 의존성 해결 (완료된 Job의 dependents 처리)
    // 
    // 스레드 안전성:
    // - 의존성 카운터: atomic 연산 사용
    // - 의존성 그래프: JobLifecycleManager의 락 사용
    
    class DependencyResolver
    {
    public:
        explicit DependencyResolver(JobLifecycleManager* lifecycleManager);

        // 의존성 설정
        // job: 의존성을 설정할 Job
        // dependencies: 의존하는 Job 핸들 배열
        // numDependencies: 의존성 개수
        // 반환값: true = 성공, false = 순환 의존성 감지
        bool SetupDependencies(Job* job, const JobHandle* dependencies, uint32_t numDependencies);

        // 완료된 Job의 dependents 해결
        // completedJob: 완료된 Job
        // 반환값: 실행 준비가 된 Job 목록
        std::vector<Job*> ResolveDependents(Job* completedJob);

        // 미완료 의존성 존재 여부 확인
        // job: 확인할 Job
        // 반환값: true = 미완료 의존성 존재, false = 모든 의존성 완료
        bool HasUnfinishedDependencies(Job* job) const;

    private:
        // JobLifecycleManager 참조
        JobLifecycleManager* lifecycleManager;

        // 순환 의존성 감지
        bool HasCircularDependency(const std::vector<Job*>& activeJobs) const;

        // DFS 기반 순환 의존성 감지
        bool DetectCycle(Job* job, std::vector<bool>& visited, std::vector<bool>& recStack,
                         const std::vector<Job*>& activeJobs) const;

        // 의존성 카운터 감소
        void DecrementDependencyCounter(Job* job);
    };

} // namespace Engine
