#pragma once

#include "Job.h"
#include <atomic>
#include <vector>
#include <cstdint>

namespace Engine
{
    // ========================================
    // Work Stealing Deque
    // ========================================
    
    // Lock-free work stealing deque
    // 
    // 동작 방식:
    // - Push/Pop: 로컬 스레드가 자신의 큐 끝(bottom)에서 LIFO로 작업
    // - Steal: 다른 스레드가 큐 앞(top)에서 FIFO로 작업 훔침
    // 
    // 캐시 친화성:
    // - top과 bottom을 64바이트(캐시 라인) 단위로 정렬
    // - False sharing 방지
    // 
    // 참고: Chase-Lev work stealing deque 알고리즘 기반
    
    class WorkStealingDeque
    {
    public:
        WorkStealingDeque();
        ~WorkStealingDeque();

        // Delete copy constructor and assignment (atomic members are not copyable)
        WorkStealingDeque(const WorkStealingDeque&) = delete;
        WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

        // Move constructor and assignment
        WorkStealingDeque(WorkStealingDeque&& other) noexcept;
        WorkStealingDeque& operator=(WorkStealingDeque&& other) noexcept;

        // 로컬 스레드가 큐 끝에 Job 추가 (LIFO)
        void Push(Job* job);

        // 로컬 스레드가 큐 끝에서 Job 가져오기 (LIFO)
        // 반환값: Job 포인터 또는 nullptr (큐가 비었을 때)
        Job* Pop();

        // 다른 스레드가 큐 앞에서 Job 훔치기 (FIFO)
        // 반환값: Job 포인터 또는 nullptr (큐가 비었거나 경합 발생)
        Job* Steal();

        // 큐가 비었는지 확인
        bool IsEmpty() const;

        // 큐의 대략적인 크기 (정확하지 않을 수 있음)
        size_t Size() const;

    private:
        // 캐시 라인 정렬 (64바이트)
        // False sharing 방지를 위해 top과 bottom을 분리
        alignas(64) std::atomic<int64_t> top;
        alignas(64) std::atomic<int64_t> bottom;

        // Job 포인터 배열
        std::vector<Job*> buffer;
        
        // 버퍼 크기 (2의 거듭제곱)
        size_t capacity;

        // 버퍼 크기 조정
        void Resize();
    };

} // namespace Engine
