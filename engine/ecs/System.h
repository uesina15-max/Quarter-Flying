#pragma once

#include <vector>
#include <typeinfo>

namespace Engine
{
    // Forward declarations
    class ECSRegistry;
    
    // ComponentArray 전방 선언 (청크 처리용)
    template<typename T>
    class TypedComponentArray;

    // ========================================
    // System Base Interface
    // ========================================
    
    // System은 ECS에서 Component를 처리하는 로직을 담당합니다.
    // 
    // 설계 원칙:
    // - 순수 로직만 포함 (데이터는 Component에)
    // - ECSRegistry를 통해 Entity와 Component에 접근
    // - 병렬 실행을 고려한 설계
    // - 상태를 가지지 않음 (Stateless)
    // 
    // 수명주기:
    // 1. Initialize() - System 초기화 (한 번)
    // 2. Update() - 매 프레임 실행
    // 3. Shutdown() - System 종료 (한 번)
    // 
    // 병렬 실행 고려사항:
    // - Update 메서드는 여러 스레드에서 동시에 호출될 수 있음
    // - System 내부 상태를 가지면 안됨 (Thread-safe하지 않음)
    // - Component 데이터만 읽기/쓰기 해야 함
    
    class System
    {
    public:
        virtual ~System() = default;

        // ========================================
        // System Lifecycle
        // ========================================
        
        // System 초기화
        // World 생성 시 한 번 호출됩니다.
        // registry: ECS Registry 참조
        virtual void Initialize(ECSRegistry& registry) {}

        // System 종료
        // World 파괴 시 한 번 호출됩니다.
        // registry: ECS Registry 참조
        virtual void Shutdown(ECSRegistry& registry) {}

        // ========================================
        // PIE Lifecycle (Phase 3)
        // ========================================
        
        // 에디터 Play 모드 시작 시 호출
        virtual void OnStart(ECSRegistry& registry) {}

        // 에디터 Stop (롤백 전/후) 시 호출
        virtual void OnStop(ECSRegistry& registry) {}

        // ========================================
        // System Update
        // ========================================
        
        // System 업데이트 (매 프레임)
        // 
        // 주의사항:
        // - 이 메서드는 여러 스레드에서 동시에 호출될 수 있습니다
        // - System 내부 상태를 변경하면 안됩니다
        // - Component 데이터만 읽기/쓰기 해야 합니다
        // 
        // registry: ECS Registry 참조 (Entity/Component 접근용)
        // deltaTime: 이전 프레임으로부터 경과한 시간 (초 단위)
        virtual void Update(ECSRegistry& registry, float deltaTime) = 0;

        // ========================================
        // System Metadata
        // ========================================
        
        // System 이름 반환 (디버깅 및 로깅용)
        // 기본값: 클래스 이름 (typeid 사용)
        virtual const char* GetName() const { return "System"; }

        // System 우선순위 반환 (낮을수록 먼저 실행)
        // 기본값: 0 (보통 우선순위)
        virtual int GetPriority() const { return 0; }

        // ========================================
        // Parallel Execution Support
        // ========================================
        
        // 병렬 실행 가능 여부 반환
        // true: 다른 System과 병렬 실행 가능
        // false: 순차 실행 필요
        // 기본값: true (대부분의 System은 병렬 실행 가능)
        virtual bool CanRunInParallel() const { return true; }

        // 읽기 전용 Component 타입 목록 반환
        // 병렬 실행 스케줄링에 사용됩니다.
        // 같은 Component를 읽기만 하는 System들은 병렬 실행 가능합니다.
        // 
        // 반환값: Component 타입 해시 목록
        // 기본값: 빈 목록 (하위 클래스에서 오버라이드 필요)
        virtual std::vector<size_t> GetReadComponentTypes() const { return {}; }

        // 쓰기 Component 타입 목록 반환
        // 병렬 실행 스케줄링에 사용됩니다.
        // 같은 Component를 쓰는 System들은 순차 실행됩니다.
        // 
        // 반환값: Component 타입 해시 목록
        // 기본값: 빈 목록 (하위 클래스에서 오버라이드 필요)
        virtual std::vector<size_t> GetWriteComponentTypes() const { return {}; }

        // ========================================
        // Chunk-based Parallel Processing Support
        // ========================================
        
        // 청크 기반 병렬 처리 지원 여부
        // true: Component 배열을 청크로 나누어 병렬 처리 가능
        // false: 전체 Entity 집합을 한 번에 처리해야 함
        // 기본값: false (안전한 기본값)
        virtual bool SupportsChunkedProcessing() const { return false; }

        // 청크 기반 업데이트 (선택 사항)
        // SupportsChunkedProcessing()이 true일 때만 호출됩니다.
        // 
        // registry: ECS Registry 참조
        // deltaTime: 프레임 시간
        // chunkStart: 처리할 Entity 범위 시작 인덱스
        // chunkSize: 처리할 Entity 개수
        virtual void UpdateChunk(ECSRegistry& registry, float deltaTime, 
                               size_t chunkStart, size_t chunkSize) {}

        // 최적 청크 크기 반환 (청크 기반 처리용)
        // 기본값: 1000 (경험적 값)
        virtual size_t GetOptimalChunkSize() const { return 1000; }

        // ========================================
        // Advanced Chunk Processing Support
        // ========================================
        
        // 동적 청크 크기 계산 (Component 개수와 워커 스레드 수 고려)
        // totalComponents: 처리할 전체 Component 개수
        // workerThreadCount: 사용 가능한 워커 스레드 수
        // 반환값: 최적화된 청크 크기
        virtual size_t CalculateDynamicChunkSize(size_t totalComponents, size_t workerThreadCount) const
        {
            if (totalComponents == 0) return 0;
            size_t targetChunkCount = workerThreadCount * 3;
            size_t calculatedChunkSize = totalComponents / (targetChunkCount > 0 ? targetChunkCount : 1);
            if (calculatedChunkSize < 64) calculatedChunkSize = 64;
            if (calculatedChunkSize > 4096) calculatedChunkSize = 4096;
            return calculatedChunkSize;
        }

        // 청크 처리 전 준비 작업 (선택 사항)
        // 청크 기반 처리 시작 전에 한 번 호출됩니다.
        virtual void PrepareChunkedProcessing(ECSRegistry& registry, float deltaTime) {}

        // 청크 처리 후 정리 작업 (선택 사항)
        // 모든 청크 처리 완료 후 한 번 호출됩니다.
        virtual void FinalizeChunkedProcessing(ECSRegistry& registry, float deltaTime) {}

        // 청크 간 동기화가 필요한지 여부
        // true: 각 청크 처리 후 동기화 필요
        // false: 모든 청크를 독립적으로 처리 가능
        virtual bool RequiresChunkSynchronization() const { return false; }

        // System이 특정 Component 타입에 대해 읽기 전용인지 확인
        // 동시성 제어 최적화에 사용됩니다.
        virtual bool IsReadOnlyForComponent(size_t componentTypeHash) const
        {
            auto writeTypes = GetWriteComponentTypes();
            return std::find(writeTypes.begin(), writeTypes.end(), componentTypeHash) == writeTypes.end();
        }

        // System이 특정 Component 타입을 사용하는지 확인
        virtual bool UsesComponent(size_t componentTypeHash) const
        {
            auto readTypes = GetReadComponentTypes();
            auto writeTypes = GetWriteComponentTypes();
            
            return std::find(readTypes.begin(), readTypes.end(), componentTypeHash) != readTypes.end() ||
                   std::find(writeTypes.begin(), writeTypes.end(), componentTypeHash) != writeTypes.end();
        }
    };

} // namespace Engine
