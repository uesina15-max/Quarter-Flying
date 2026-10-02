#pragma once

#include "Job.h"
#include <cstdint>
#include <deque>
#include <mutex>

namespace Engine
{
    /// <summary>
    /// 워커별 잡 큐. 소유 워커는 뒤쪽에서 LIFO로 Push/Pop하고, 다른 워커는 앞쪽에서 FIFO로 Steal한다.
    ///
    /// 뮤텍스로 보호하는 std::deque다. 예전에는 락 없는 Chase-Lev 구현이었는데, 코드 검토로 확인한 결함이 있었다.
    ///  1) Resize()가 buffer 벡터를 통째로 교체하는 동안 다른 워커의 Steal()이 buffer[t % capacity]를 읽었다
    ///     (해제된 메모리, 또는 buffer와 capacity가 어긋난 상태). 잘못된 잡을 돌려줄 수 있었다.
    ///  2) 버퍼 슬롯이 원자적이지 않았다.
    ///  3) JobScheduler가 여러 스레드에서 같은 deque에 Push했다(소유자 전용 규칙 위반, JobScheduler::EnqueueJob 주석).
    /// 참고: 의존성 체인 스트레스 테스트에서 보인 잡 유실의 직접 원인은 DependencyResolver의 순환 검사 오탐이었다
    /// (그 주석 참고). 위 결함과 그 증상의 연관은 따로 분리해 확인하지 않았다 - 실제 경쟁이라서 함께 고쳤다.
    /// 락 없는 버전을 크기 조정까지 정확하게 만드는 것(옛 버퍼 수명 관리 등)보다, 이 엔진의 잡 규모에서는 락이
    /// 정확하고 충분히 빠르다. 인터페이스는 그대로다.
    /// </summary>
    class WorkStealingDeque
    {
    public:
        WorkStealingDeque() = default;
        ~WorkStealingDeque() = default;

        WorkStealingDeque(const WorkStealingDeque&) = delete;
        WorkStealingDeque& operator=(const WorkStealingDeque&) = delete;

        // std::vector<WorkStealingDeque>에 담기 위해 필요하다. 뮤텍스는 옮기지 않고 내용만 옮긴다
        // (스케줄러가 워커를 시작하기 전에만 일어난다).
        WorkStealingDeque(WorkStealingDeque&& other) noexcept;
        WorkStealingDeque& operator=(WorkStealingDeque&& other) noexcept;

        void Push(Job* job);   // 소유 워커: 뒤에 추가
        Job* Pop();            // 소유 워커: 뒤에서 꺼냄 (LIFO), 비었으면 nullptr
        Job* Steal();          // 다른 워커: 앞에서 꺼냄 (FIFO), 비었으면 nullptr

        bool IsEmpty() const;
        size_t Size() const;

    private:
        mutable std::mutex mutex;
        std::deque<Job*> jobs;
    };
}
