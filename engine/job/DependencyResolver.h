#pragma once

#include "Job.h"
#include <vector>
#include <mutex>

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
        // outReady: 설정이 끝난 시점에 기다릴 의존성이 없으면 true. 호출자는 이 값이 true일 때만 잡을 큐에 넣는다.
        //   true가 아니면 마지막 선행 잡을 끝낸 쪽(ResolveDependents)이 큐에 넣는다. 정확히 한 쪽만 넣는다.
        // 반환값: true = 성공, false = 순환 의존성 감지 또는 잘못된 핸들
        bool SetupDependencies(Job* job, const JobHandle* dependencies, uint32_t numDependencies, bool& outReady);

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

        // "선행 잡이 끝났나 확인 + dependents에 등록"(SetupDependencies)과 "dependents 순회"(ResolveDependents)를
        // 묶는다. 예전에는 락이 없어서 (1) 확인과 등록 사이에 선행 잡이 끝나면 후행 잡이 영영 깨어나지 않았고
        // (2) 워커 스레드가 dependents를 순회하는 동안 다른 스레드가 push_back했다.
        std::mutex graphMutex;

        // 순환 의존성 감지
        bool HasCircularDependency(const std::vector<Job*>& activeJobs) const;

        // DFS 기반 순환 의존성 감지
        bool DetectCycle(Job* job, std::vector<bool>& visited, std::vector<bool>& recStack,
                         const std::vector<Job*>& activeJobs) const;

        // 의존성 카운터 감소
        bool DecrementDependencyCounter(Job* job);   // true = 이 감소로 0이 됨
    };

} // namespace Engine
