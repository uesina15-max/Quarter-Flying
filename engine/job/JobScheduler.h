#pragma once

#include "Job.h"
#include "WorkStealingDeque.h"
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

namespace Engine
{
    // ========================================
    // Job Scheduler
    // ========================================
    
    // 워커 스레드와 작업 큐를 관리하는 컴포넌트
    // 
    // 책임:
    // - 워커 스레드 생성 및 수명주기 관리
    // - Job을 적절한 큐에 추가
    // - Work Stealing 조정
    // - 워커 스레드 동기화 (wake-up/sleep)
    // - 활성 워커 수 추적
    // 
    // 스레드 안전성:
    // - running: atomic 플래그
    // - activeWorkers: atomic 카운터
    // - wakeCondition: mutex로 보호
    
    class JobScheduler
    {
    public:
        JobScheduler();
        ~JobScheduler();

        // 워커 스레드 초기화
        // numThreads: 생성할 워커 스레드 수
        // Requirement 3.1: 워커 스레드 생성 및 수명주기 관리
        void Initialize(uint32_t numThreads);

        // 모든 워커 스레드 종료
        // Requirement 3.1: 워커 스레드 수명주기 관리
        void Shutdown();

        // Job을 실행 큐에 추가
        // job: 추가할 Job
        // Requirement 3.2: Job을 적절한 큐에 추가
        void EnqueueJob(Job* job);

        // Job 완료 여부 확인
        // job: 확인할 Job
        // 반환값: true = 완료, false = 진행 중
        bool IsJobComplete(Job* job) const;

        // Job 완료 대기 (busy-wait with yielding)
        // job: 대기할 Job
        void WaitForJob(Job* job);

        // 스케줄러 실행 여부 확인
        // 반환값: true = 실행 중, false = 중지됨
        bool IsRunning() const;

        // 워커 스레드 수 조회
        // 반환값: 워커 스레드 수
        uint32_t GetWorkerThreadCount() const;

        // Job 완료 콜백 설정
        // callback: Job 완료 시 호출될 함수
        void SetJobCompletionCallback(std::function<void(Job*)> callback);

    private:
        // 워커 스레드 메인 루프
        // threadIndex: 스레드 인덱스
        // Requirement 3.1: 워커 스레드 관리
        void WorkerThreadMain(uint32_t threadIndex);

        // Job 획득 (로컬 큐 또는 Steal)
        // threadIndex: 현재 스레드 인덱스
        // 반환값: Job 포인터 (없으면 nullptr)
        // Requirement 3.3: Work Stealing 조정
        Job* AcquireJob(uint32_t threadIndex);

        // 다른 스레드에서 Job 훔치기
        // thiefIndex: 훔치는 스레드 인덱스
        // 반환값: Job 포인터 (없으면 nullptr)
        // Requirement 3.3: Work Stealing 조정
        Job* StealJob(uint32_t thiefIndex);

        // 작업 대기
        // Requirement 3.4: 워커 스레드 wake-up 및 sleep 관리
        void WaitForWork();

        // 스레드 수 결정
        // requestedThreads: 요청된 스레드 수 (0 = 자동)
        // 반환값: 실제 생성할 스레드 수
        uint32_t DetermineThreadCount(uint32_t requestedThreads) const;

        // 워커 스레드
        std::vector<std::thread> workers;

        // 각 워커 스레드의 로컬 큐
        // Requirement 3.3: Work Stealing을 위한 큐 관리
        std::vector<WorkStealingDeque> localQueues;

        // 시스템 상태
        std::atomic<bool> running;

        // 활성 워커 수
        // Requirement 3.5: 활성 워커 수 추적
        std::atomic<uint32_t> activeWorkers;

        // 워커 스레드 동기화
        std::mutex wakeMutex;
        std::condition_variable wakeCondition;

        // Job 완료 콜백
        std::function<void(Job*)> completionCallback;
    };

} // namespace Engine
