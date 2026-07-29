#include "WorkStealingDeque.h"
#include <algorithm>
#include <vector>
#include <utility>

namespace Engine
{
    WorkStealingDeque::WorkStealingDeque()
        : top(0)
        , bottom(0)
        , capacity(256) // 초기 용량
    {
        buffer.resize(capacity);
    }

    WorkStealingDeque::~WorkStealingDeque()
    {
    }

    WorkStealingDeque::WorkStealingDeque(WorkStealingDeque&& other) noexcept
        : top(other.top.load(std::memory_order_relaxed))
        , bottom(other.bottom.load(std::memory_order_relaxed))
        , buffer(std::move(other.buffer))
        , capacity(other.capacity)
    {
        // Reset the moved-from object
        other.top.store(0, std::memory_order_relaxed);
        other.bottom.store(0, std::memory_order_relaxed);
        other.capacity = 0;
    }

    WorkStealingDeque& WorkStealingDeque::operator=(WorkStealingDeque&& other) noexcept
    {
        if (this != &other)
        {
            top.store(other.top.load(std::memory_order_relaxed), std::memory_order_relaxed);
            bottom.store(other.bottom.load(std::memory_order_relaxed), std::memory_order_relaxed);
            buffer = std::move(other.buffer);
            capacity = other.capacity;

            // Reset the moved-from object
            other.top.store(0, std::memory_order_relaxed);
            other.bottom.store(0, std::memory_order_relaxed);
            other.capacity = 0;
        }
        return *this;
    }

    void WorkStealingDeque::Push(Job* job)
    {
        int64_t b = bottom.load(std::memory_order_relaxed);
        int64_t t = top.load(std::memory_order_acquire);

        // 버퍼가 가득 찼으면 크기 조정
        if (b - t >= static_cast<int64_t>(capacity))
        {
            Resize();
        }

        // Job을 버퍼에 추가
        buffer[b % capacity] = job;

        // Memory fence: 버퍼 쓰기가 완료된 후 bottom 증가
        // 다른 스레드가 Steal할 때 유효한 Job을 보도록 보장
        std::atomic_thread_fence(std::memory_order_release);
        bottom.store(b + 1, std::memory_order_relaxed);
    }

    Job* WorkStealingDeque::Pop()
    {
        // Chase-Lev 알고리즘: 로컬 스레드가 LIFO로 Pop
        // 1. bottom을 먼저 감소 (예약)
        int64_t b = bottom.load(std::memory_order_relaxed) - 1;
        bottom.store(b, std::memory_order_relaxed);

        // 2. Memory fence로 bottom 쓰기와 top 읽기 순서 보장
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t t = top.load(std::memory_order_relaxed);

        Job* job = nullptr;

        if (t <= b)
        {
            // 큐에 최소 1개 이상의 Job이 있음
            job = buffer[b % capacity];

            if (t == b)
            {
                // 마지막 Job - Steal과 경합 가능
                // CAS로 top을 증가시켜 경합 해결
                if (!top.compare_exchange_strong(t, t + 1,
                                                 std::memory_order_seq_cst,
                                                 std::memory_order_relaxed))
                {
                    // CAS 실패 - Steal이 먼저 가져감
                    job = nullptr;
                }

                // bottom 복원
                bottom.store(b + 1, std::memory_order_relaxed);
            }
        }
        else
        {
            // 큐가 비었음 - bottom 복원
            bottom.store(b + 1, std::memory_order_relaxed);
        }

        return job;
    }

    Job* WorkStealingDeque::Steal()
    {
        // Chase-Lev 알고리즘: 다른 스레드가 FIFO로 Steal
        // 1. top 읽기 (가장 오래된 Job)
        int64_t t = top.load(std::memory_order_acquire);
        
        // 2. Memory fence로 top 읽기와 bottom 읽기 순서 보장
        std::atomic_thread_fence(std::memory_order_seq_cst);
        int64_t b = bottom.load(std::memory_order_acquire);

        Job* job = nullptr;

        if (t < b)
        {
            // 큐에 최소 1개 이상의 Job이 있음
            job = buffer[t % capacity];

            // CAS로 top을 증가시켜 Job 획득
            // 다른 Steal과 경합 가능
            if (!top.compare_exchange_strong(t, t + 1,
                                             std::memory_order_seq_cst,
                                             std::memory_order_relaxed))
            {
                // CAS 실패 - 다른 스레드가 먼저 가져감
                return nullptr;
            }
        }

        return job;
    }

    bool WorkStealingDeque::IsEmpty() const
    {
        int64_t b = bottom.load(std::memory_order_relaxed);
        int64_t t = top.load(std::memory_order_relaxed);
        return b <= t;
    }

    size_t WorkStealingDeque::Size() const
    {
        int64_t b = bottom.load(std::memory_order_relaxed);
        int64_t t = top.load(std::memory_order_relaxed);
        int64_t size = b - t;
        return size > 0 ? static_cast<size_t>(size) : 0;
    }

    void WorkStealingDeque::Resize()
    {
        // 새 버퍼 크기 (2배)
        size_t newCapacity = capacity * 2;
        std::vector<Job*> newBuffer(newCapacity);

        // 기존 Job들을 새 버퍼로 복사
        int64_t b = bottom.load(std::memory_order_relaxed);
        int64_t t = top.load(std::memory_order_relaxed);

        for (int64_t i = t; i < b; ++i)
        {
            newBuffer[i % newCapacity] = buffer[i % capacity];
        }

        // 버퍼 교체
        buffer = std::move(newBuffer);
        capacity = newCapacity;
    }

} // namespace Engine
