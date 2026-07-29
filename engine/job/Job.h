#pragma once

#include <cstdint>
#include <atomic>
#include <vector>

namespace Engine
{
    // ========================================
    // Job Function Type
    // ========================================
    
    // Job 함수 시그니처
    // void* data: Job에 전달되는 사용자 데이터
    using JobFunction = void(*)(void*);

    // ========================================
    // Job Handle
    // ========================================
    
    // Job을 참조하기 위한 핸들
    // Generation을 사용하여 핸들 재사용 시 안전성 보장
    struct JobHandle
    {
        uint32_t id;         // Job의 고유 ID
        uint32_t generation; // 핸들 재사용 감지를 위한 세대 번호
        
        JobHandle()
            : id(0)
            , generation(0)
        {}
        
        JobHandle(uint32_t id, uint32_t generation)
            : id(id)
            , generation(generation)
        {}
        
        bool IsValid() const
        {
            return id != 0;
        }
        
        bool operator==(const JobHandle& other) const
        {
            return id == other.id && generation == other.generation;
        }
        
        bool operator!=(const JobHandle& other) const
        {
            return !(*this == other);
        }
    };

    // ========================================
    // Job Structure
    // ========================================
    
    // 병렬 실행 가능한 작업 단위
    // 
    // 수명주기:
    // 1. Dispatch() - Job 생성 및 큐에 추가
    // 2. Worker Thread - Job 실행
    // 3. Complete - 의존하는 Job의 카운터 감소
    // 4. Wait() - Job 완료 대기
    struct Job
    {
        // Job 실행 함수
        JobFunction function;
        
        // Job에 전달되는 데이터 (FrameAllocator에서 할당)
        void* data;
        
        // 의존성 관리
        // 이 Job이 실행되기 전에 완료되어야 하는 Job의 수
        std::atomic<uint32_t> unfinishedDependencies;
        
        // 이 Job에 의존하는 Job 목록
        // 이 Job이 완료되면 dependents의 카운터를 감소시킴
        std::vector<Job*> dependents;
        
        // Job 핸들
        JobHandle handle;
        
        // Job 상태
        std::atomic<bool> completed;
        
        Job()
            : function(nullptr)
            , data(nullptr)
            , unfinishedDependencies(0)
            , completed(false)
        {}
        
        // Job 실행
        void Execute()
        {
            if (function)
            {
                function(data);
            }
            completed.store(true, std::memory_order_release);
        }
        
        // Job 완료 여부 확인
        bool IsComplete() const
        {
            return completed.load(std::memory_order_acquire);
        }
    };

} // namespace Engine
