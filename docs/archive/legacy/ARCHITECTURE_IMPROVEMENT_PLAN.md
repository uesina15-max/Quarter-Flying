# 최종 아키텍처 개선 계획서

**작성일**: 2026-07-30  
**기준**: 아키텍처 검증 체크리스트 점검 결과 및 리뷰 반영본  
**목표**: 문서와 실제 구조의 정합성 확보, Python/엔진 경계 확립, Undo/Redo 가능한 편집 인프라 완성

---

## 3차 보강 검토 반영 완료 (2026-07-31)

### 주요 수정 사항
1. **동기/비동기 계약 모순 해결**: EditorAPI는 동기 계약만 지원하며, 비동기 호출 필요 시 별도 API 제공
2. **pybind11 예외 변환 레이어 추가**: std::expected를 Python 예외로 변환하는 UnwrapOrThrow 래퍼 구현
3. **Create/Destroy 서브트리 지원**: 계층 구조에서 서브트리 전체를 원자적으로 처리하는 DestroyEntityCommand 설계
4. **RemoveComponent 필드 스냅샷**: Reflection 활용한 필드 값 스냅샷 및 비대칭 설계 명시
5. **Redo 스택 클리어 시맨틱**: Undo 이후 새 Command 실행 시 redoStack_ 즉시 비우는 표준 규약 추가
6. **World/Scene reload 훅**: CommandManager::Clear() 호출로 Undo/Redo 메뉴 비활성화
7. **Handle 재사용 정책**: 오래된 redo 항목과 충돌 시 동작 테스트 케이스 추가
8. **Merge Session 명시적 API**: BeginMergeSession()/EndMergeSession() API 및 포커스 상실 안전장치
9. **View 스냅샷 의미론**: TransformComponentView의 스냅샷 특성 문서화
10. **Selection/Undo 분리**: Selection을 UI 상태로 명의하고 Undo 스택에서 분리

---

## 1. 종합 결론

**결론 한 줄**: 현재 구조는 엔진 코어의 방향은 건강하지만, 편집 변경이 `Editor API → Command/Transaction` 경로로 강제되지 않아 아키텍처 정합성이 깨지고 있으므로, 이를 중심으로 단계적 재구성이 필요하다.

현재 상태는 **6/9 합격(67%)**이며, 핵심 미해결 영역은 다음 세 가지다.

| 영역 | 현재 상태 | 핵심 문제 | 목표 상태 |
|---|---|---|---|
| Command/Transaction | ❌ 불합격 | 편집 변경의 작업 단위 기록 부재 | 모든 편집 작업의 Undo/Redo 가능 |
| Python 경계 | ⚠️ 부분 합격 | Python이 ECS 내부를 직접 수정 가능 | Python은 Editor API만 통해 수정 |
| 데이터 소유권 | ⚠️ 부분 합격 | Handle/수명주기/유효성 경계 불명확 | 엔진 소유권 고정 + stale handle 방어 |

이번 최종안의 핵심은 단순히 기능을 추가하는 것이 아니라, 아래 구조를 강제하는 데 있다.

> **모든 편집 변경은 반드시 `Editor API → Command → CommandManager` 경로를 통과한다.**

이 원칙이 지켜지면 체크리스트의 `Command/Transaction`, `Python 경계`, `데이터 소유권` 항목이 함께 개선된다.

---

## 2. 아키텍처 목표와 설계 원칙

이번 개선은 기능 추가가 아니라 **편집 아키텍처의 재정렬**이다.  
이 문서에서 적용하는 최종 원칙은 다음과 같다.

### 2.1 단일 수정 경로 원칙
엔진 상태를 바꾸는 모든 편집 행위는 `Editor API`를 통해서만 수행한다.  
Python, UI, 툴 스크립트, 자동화 로직 모두 예외 없이 같은 경로를 사용한다.

### 2.2 명령 기반 편집 원칙
편집은 즉시 상태를 바꾸는 호출이 아니라, **실행·취소 가능한 명령**으로 표현한다.  
Undo/Redo는 부가 기능이 아니라 편집 시스템의 기본 계약으로 취급한다.

### 2.3 엔진 소유권 유지 원칙
Entity/Component의 실제 수명주기와 메모리 소유권은 항상 엔진에 있다.  
Python은 데이터를 소유하지 않고, ID/Handle과 읽기 전용 스냅샷만 다룬다.

### 2.4 범용화의 단계적 도입 원칙
초기에는 `set_property(path, json)` 같은 범용 API보다, `move_entity`, `rotate_entity`, `add_component` 같은 **typed API**를 우선 구현한다.  
범용 reflection 기반 속성 수정은 2차 단계에서 도입한다.

### 2.5 실패 가능성의 명시 원칙
트랜잭션 원자성을 보장하려면 명령 인터페이스부터 실패를 표현해야 한다.  
따라서 `void Execute()` 중심 설계는 지양하고, `std::expected<void, EngineError>` 기반으로 통일한다.

---

## 3. 우선순위별 최종 로드맵

| 우선순위 | 항목 | 목표 | 예상 난이도 | 비고 |
|---|---|---|---|---|
| 🔴 P0 | Command/Transaction 기반 인프라 | Undo/Redo 가능한 편집 기록 체계 구축 | 높음 | 최우선 |
| 🔴 P0 | Editor API 레이어 도입 | 편집 변경 경로 단일화 | 높음 | Command와 동시 진행 |
| 🟡 P1 | Python 바인딩 경계 강화 | 직접 메모리 수정 차단 | 중간 | P0 완료 직후 |
| 🟡 P1 | 핵심 Python 도구 마이그레이션 | 기존 스크립트 경로 전환 | 중간 | 점진적 |
| 🟢 P2 | 데이터 소유권 명확화 | Handle/Generation 기반 안전성 확보 | 중간 | 경계 강화 후 적용 |
| 🟢 P2 | 범용 Property API | Reflection 기반 속성 수정 확장 | 중간~높음 | typed API 안정화 후 |
| 🟢 P3 | 레거시 정리 및 프로젝트 구조 최적화 | 문서/빌드 구조 단순화 | 낮음 | time-box로 제한 |
| 🔵 P3 | Command 직렬화/복원 | 세션 저장/복원 확장 | 중간 | 초기 범위에서 제외 |

---

## 4. 이론상 구조와 현재 구조의 차이

| 항목 | 바람직한 구조 | 현재 구조 | 개선 방향 |
|---|---|---|---|
| 편집 요청 진입점 | Editor API 단일 진입 | Python/UI가 직접 ECS 접근 가능 | 모든 쓰기 경로를 Editor API로 집중 |
| 상태 변경 단위 | ICommand 기반 기록 | 함수 호출이 즉시 상태 변경 | Command로 캡슐화 |
| Undo/Redo | CommandManager가 관리 | 메뉴만 있고 실제 인프라 없음 | undo/redo stack 구현 |
| 복합 작업 | Transaction 단위 | 다중 작업이 분산 호출 | atomic transaction 도입 |
| Python 바인딩 | 읽기 전용 + 명시적 수정 API | `def_readwrite`로 직접 수정 노출 | 읽기 전용 스냅샷으로 축소 |
| Entity 유효성 | Handle + generation 검증 | 단순 ID 접근 가능성 | stale handle 방어 추가 |
| 범용 속성 수정 | reflection 기반 fallback | 직접 필드 접근 | typed API 우선 후 확장 |

---

## 5. 상세 개선 계획

---

## 5.1 P0-1: Command/Transaction 시스템 구현

### 문제 정의
현재는 Undo/Redo 메뉴가 존재하더라도 실제 편집 변경이 명령 객체로 기록되지 않기 때문에, 구조적으로 되돌리기가 불가능하다.  
또한 복합 작업을 하나의 작업 단위로 묶는 트랜잭션 개념이 없어, 에디터 동작의 일관성이 깨질 수 있다.

### 최종 설계 방향
기존 초안의 `void Execute()/Undo()/Redo()` 구조는 실패 처리와 원자성 보장에 약하므로, 다음처럼 수정한다.

### 핵심 인터페이스

```cpp
// engine/core/ICommand.h
#pragma once
#include <expected>
#include <string>
#include "EngineError.h"

namespace Engine {

class ICommand {
public:
    virtual ~ICommand() = default;

    // 실제 적용
    virtual std::expected<void, EngineError> Apply() = 0;

    // 적용 취소
    virtual std::expected<void, EngineError> Undo() = 0;

    // Command 병합 가능 여부
    virtual bool CanMergeWith(const ICommand& other) const { return false; }

    // 연속 조작 병합
    virtual std::expected<void, EngineError> MergeWith(const ICommand& other) {
        return std::unexpected(EngineError::NotMergeable());
    }

    virtual std::string GetName() const = 0;
};

} // namespace Engine
```

이 구조의 장점은 명확하다.  
`Redo()`를 별도 강제하지 않고, 재적용은 `Apply()`를 다시 호출하는 방식으로 단순화할 수 있다.  
또한 실패 가능성이 인터페이스에 드러나므로 Transaction의 원자성 구현이 가능해진다.

---

### Transaction 설계

Transaction은 단순 Composite가 아니라, **커밋/취소/부분 실패 롤백 정책**을 가진 편집 단위여야 한다.

```cpp
// engine/core/Transaction.h
#pragma once
#include "ICommand.h"
#include <memory>
#include <vector>

namespace Engine {

class Transaction : public ICommand {
public:
    explicit Transaction(std::string name);

    void AddCommand(std::unique_ptr<ICommand> command);
    bool Empty() const;
    size_t Size() const;

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    size_t appliedCount_ = 0;
};

} // namespace Engine
```

### 트랜잭션 정책
이번 최종안에서는 정책을 다음처럼 명확히 한다.

| 정책 항목 | 최종 결정 |
|---|---|
| 원자성 | 기본적으로 atomic |
| 중간 실패 | 이미 적용된 command를 역순 Undo하여 rollback |
| 빈 트랜잭션 | undo stack에 넣지 않음 |
| 중첩 트랜잭션 | 1차 구현에서는 금지 또는 flatten 중 하나를 명시적으로 택함 |
| 권장안 | 1차 구현은 **중첩 금지**, 이후 필요 시 flatten 지원 |

중첩 트랜잭션은 초기에 허용하면 디버깅 난도가 급상승하므로, 1차 구현에서는 금지하고 에러를 반환하는 편이 안전하다.

---

### CommandManager 설계

초기 구현은 singleton으로 시작할 수 있으나, 최종 구조는 향후 `EditorSession` 또는 `EditorContext` 소유로 이전 가능하게 설계한다.

```cpp
// engine/core/CommandManager.h
#pragma once
#include "ICommand.h"
#include <expected>
#include <memory>
#include <vector>

namespace Engine {

class CommandManager {
public:
    static CommandManager& GetInstance(); // 초기 구현용
    
    // 향후 EditorSession/EditorContext 소유로 이전 가능하게 설계
    // 현재는 싱글톤으로 메인 스레드 전용으로 운용

    std::expected<void, EngineError> Execute(std::unique_ptr<ICommand> command);
    std::expected<void, EngineError> Undo();
    std::expected<void, EngineError> Redo();

    // Undo 이후 새 Command를 Execute하면 redoStack_이 즉시 비워짐 (표준 규약)

    bool CanUndo() const;
    bool CanRedo() const;

    void Clear();

    void SetMaxUndoDepth(size_t depth);
    size_t GetMaxUndoDepth() const;

private:
    CommandManager() = default;

    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    size_t maxUndoDepth_ = 100;
    
    // 스레드 안전성: 메인 스레드 전용으로 설계
    // Python 호출은 GIL 하에서 엔진 메인 스레드와 동기화 필요
};

} // namespace Engine
```

### 커맨드 병합 정책
드래그 이동, 슬라이더 변경, 연속 입력 같은 작업은 개별 명령을 모두 스택에 넣지 말고 **병합(coalescing)** 해야 한다.

**병합 실행 위치**: CommandManager::Execute()에서 undo 스택 top과 비교하여 병합 여부 판단

**시간 윈도우 및 병합 세션**: 병합을 순수 타입/엔티티 기준으로만 판단하면 "물체를 옮기고 → 다른 작업을 하고 → 한참 뒤에 같은 물체를 다시 옮긴" 경우까지 병합될 위험이 있으므로, 드래그 시작~끝과 같은 명시적 "병합 세션" 개념을 함께 사용

**명시적 Merge Session API**:
```cpp
class CommandManager {
public:
    void BeginMergeSession();
    void EndMergeSession();
    
private:
    bool inMergeSession_ = false;
};
```

`Execute()`는 `inMergeSession_ == true`일 때만 undo 스택 top과의 `CanMergeWith()`를 시도한다. UI 쪽에서는 기즈모 드래그의 mouse-down/up, 인스펙터 슬라이더의 drag-start/end에 각각 `BeginMergeSession()`/`EndMergeSession()`을 건다. 포커스 상실 이벤트에도 세션을 강제 종료하는 안전장치 필요.

따라서 병합 정책은 단순 `bool strategy`가 아니라, 각 Command가 직접 병합 가능성과 병합 로직을 가진다.

예시:
- `MoveEntityCommand(A, pos1)` 이후 `MoveEntityCommand(A, pos2)`가 연속으로 오면 병합
- 최종적으로 undo 시 한 번에 원래 위치로 복귀

**MergeWith 구현 시 유의사항**: 병합된 커맨드의 undo 기준값(예: MoveEntityCommand의 oldPosition)은 최초 병합 대상의 값을 유지해야 하며, 매 병합마다 갱신하면 안 됨

---

### Create/Destroy 커맨드의 Handle 안정성 (핵심 설계)

**문제**: Redo()를 없애고 Apply() 재호출로 대체한 설계는 깔끔하지만, CreateEntityCommand의 경우 핵심 문제를 만듭니다.

```cpp
Transaction:
  1. CreateEntityCommand → handle H 반환
  2. AddComponentCommand(H)
  3. MoveEntityCommand(H, pos)
```

이 트랜잭션을 Undo했다가 다시 Redo(=Apply() 재호출)하면, CreateEntityCommand::Apply()가 원래와 동일한 H(같은 id, 같은 generation)를 재생성해야 합니다. 그렇지 않으면 트랜잭션 내부의 2번·3번 커맨드가 들고 있는 H가 stale handle이 되어 Redo 자체가 실패합니다.

**해결 방안**: CreateEntityCommand가 최초 Apply()에서 발급받은 handle을 멤버로 저장해두고, 이후 재적용 시에는 registry에 "이 id/generation으로 복원해달라"고 요청

```cpp
// engine/ecs/ECSRegistry.h (확장)
class ECSRegistry {
public:
    // Handle 복원 API
    std::expected<void, EngineError> RestoreEntity(EntityHandle handle);
    
    // 기존 유효성 검사
    bool IsValid(EntityHandle handle) const;
};
```

**CreateEntityCommand 구현 예시**:
```cpp
class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(const std::string& name);
    
    std::expected<void, EngineError> Apply() override {
        if (originalHandle.IsValid()) {
            // Handle이 이미 있으면 복원 모드
            return registry->RestoreEntity(originalHandle);
        } else {
            // 최초 생성 모드
            auto result = registry->CreateEntity(name);
            if (result) {
                originalHandle = *result;
                return {};
            }
            return std::unexpected(EngineError::EntityCreationFailed());
        }
    }
    
    std::expected<void, EngineError> Undo() override {
        return registry->DestroyEntity(originalHandle);
    }
    
private:
    std::string name;
    EntityHandle originalHandle;  // Apply() 시 발급받은 handle 저장
    ECSRegistry* registry;
};
```

**DestroyEntityCommand 구현 예시 (서브트리 지원)**:
```cpp
class DestroyEntityCommand : public ICommand {
public:
    DestroyEntityCommand(EntityHandle entity) : root_(entity), registry_(ECSRegistry::GetInstance()) {}
    
    std::expected<void, EngineError> Apply() override {
        // 계층 구조 고려: 서브트리 전체 스냅샷 캡처
        if (snapshot_.empty()) {
            snapshot_ = CaptureSubtreeSnapshot(root_);
        }
        
        // 리프 -> 루트 순서로 삭제 (자식 먼저)
        for (auto it = snapshot_.rbegin(); it != snapshot_.rend(); ++it) {
            auto result = registry_->DestroyEntity(it->handle);
            if (!result) return std::unexpected(result.error());
        }
        return {};
    }
    
    std::expected<void, EngineError> Undo() override {
        // 루트 -> 리프 순서로 복원 (부모 먼저 존재해야 함)
        for (auto& node : snapshot_) {
            auto restoreResult = registry_->RestoreEntity(node.handle);
            if (!restoreResult) return std::unexpected(restoreResult.error());
            registry_->RestoreComponents(node.handle, node.componentSnapshots);
            registry_->SetParent(node.handle, node.parentHandle, node.siblingIndex);
        }
        return {};
    }
    
private:
    EntityHandle root_;
    std::vector<SubtreeNodeSnapshot> snapshot_;
    ECSRegistry* registry_;
};
```

**서브트리 스냅샷 구조**:
```cpp
struct SubtreeNodeSnapshot {
    EntityHandle handle;
    EntityHandle parentHandle;
    int siblingIndex;
    std::vector<SerializedComponent> componentSnapshots; // 엔티티는 여러 컴포넌트를 가질 수 있음
};
```

**핵심 원칙**: 
- Undo→Redo 사이클에서 handle이 동일하게 유지되어야 함
- 계층 구조에서는 서브트리 전체를 원자적으로 처리
- 부모-자식 관계와 sibling 순서도 복원

**RemoveComponentCommand 구현 예시 (필드 스냅샷 지원)**:
```cpp
class RemoveComponentCommand : public ICommand {
public:
    RemoveComponentCommand(EntityHandle entity, std::string componentType);

    std::expected<void, EngineError> Apply() override {
        // 제거 직전 상태를 Reflection으로 직렬화해 보관
        auto snapshot = Reflection::Serialize(entity_, componentType_);
        if (!snapshot) return std::unexpected(snapshot.error());
        snapshot_ = *snapshot;
        return registry_->RemoveComponent(entity_, componentType_);
    }

    std::expected<void, EngineError> Undo() override {
        auto addResult = registry_->AddComponent(entity_, componentType_);
        if (!addResult) return std::unexpected(addResult.error());
        return Reflection::Deserialize(entity_, componentType_, snapshot_);
    }

private:
    EntityHandle entity_;
    std::string componentType_;
    SerializedComponent snapshot_; // Apply() 시점에 캡처
    ECSRegistry* registry_;
};
```

**비대칭 설계 명시**:
- `RemoveComponentCommand`: 필드 값 스냅샷 필요 (Reflection 활용)
- `AddComponentCommand`: 스냅샷 불필요 (기본값으로 생성, Undo는 제거만)

---

### 구현 대상 Command 우선순위
P0에서는 범위를 좁혀 아래 항목부터 구현한다.

| 1차 대상 | 이유 |
|---|---|
| MoveEntityCommand | 가장 대표적인 편집 작업 |
| RotateEntityCommand | Transform 계열 공통성 확보 |
| ScaleEntityCommand | Transform 작업 완결 |
| CreateEntityCommand | 수명주기 작업 시작점 |
| DestroyEntityCommand | 삭제/복원 검증 필요 |
| AddComponentCommand | 구성 변경 필수 |
| RemoveComponentCommand | 편집 완결성 확보 |

`SetPropertyCommand`와 범용 reflection 명령은 1차 성공 후 추가한다.

---

### P0 범위에서 제외할 항목
초기 일정 안정성을 위해 아래는 P0 핵심 목표에서 제외한다.

| 제외 항목 | 이유 |
|---|---|
| Command 직렬화/역직렬화 | 기능은 유용하지만 초기 구현 난이도 급상승 |
| 세션 저장/복원 연계 | Undo 인프라 안정화 이후 검토 |
| 중첩 트랜잭션 완전 지원 | 초기에는 정책 단순화 우선 |
| 범용 Property Path 수정 | typed API 안정화 후 확장 |

---

### 검증 기준
- [ ] 단일 Command의 Apply/Undo 정상 동작
- [ ] Transaction이 여러 Command를 하나의 undo 단위로 묶음
- [ ] Transaction 중간 실패 시 rollback 수행
- [ ] 빈 Transaction은 스택에 쌓이지 않음
- [ ] 연속 이동 작업 병합 가능
- [ ] Ctrl+Z / Ctrl+Y가 실제 상태 변경과 연결됨
- [ ] undo/redo stack depth 제한 동작
- [ ] 실패 시 EngineError가 상위 계층까지 전달됨
- [ ] **Undo→Redo 사이클에서 handle이 동일하게 유지되는가** (Create/Destroy 핵심)

---

## 5.2 P0-2: Editor API 레이어 도입

### 문제 정의
현재 구조에서는 Python이 ECS와 Component 내부 필드를 직접 수정할 수 있어, 편집 행위가 Command 시스템 바깥으로 새고 있다.  
이 상태에서는 Undo/Redo를 도입해도 우회 경로 때문에 일관성이 보장되지 않는다.

### 목표
Editor API를 편집의 공식 인터페이스로 정의하고, **모든 쓰기 작업이 내부적으로 Command로 전환**되도록 한다.

---

### 최종 인터페이스 방향

```cpp
// engine/editor/EditorAPI.h
#pragma once
#include <expected>
#include <string>
#include "EngineError.h"
#include "EntityHandle.h"
#include "Types.h"

namespace Engine::Editor {

class EditorAPI {
public:
    static EditorAPI& GetInstance();
    
    // 향후 EditorSession/EditorContext 소유로 이전 가능하게 설계
    // 현재는 싱글톤으로 메인 스레드 전용으로 운용
    // 멀티 씬/멀티 문서 편집 고려
    
    // 스레드 안전성: 현재는 동기 계약만 지원
    // - Python 스크립트/UI 이벤트는 엔진 메인 루프와 같은 스레드에서 실행
    // - GIL은 엔진이 Python 콜백을 재진입 호출할 때의 재귀 잠금으로만 다룸
    // - 백그라운드 스레드에서 필요한 경우 별도 비동기 API 제공 (예: CreateEntityAsync)
    // - 동기 계약 유지: 호출 즉시 결과값 반환 (메인 스레드에서만 사용 가능)

    // Transaction lifecycle
    std::expected<void, EngineError> BeginTransaction(const std::string& name);
    std::expected<void, EngineError> CommitTransaction();
    std::expected<void, EngineError> CancelTransaction();
    bool IsInTransaction() const;

    // Typed editing APIs - 1차 핵심 범위
    std::expected<void, EngineError> MoveEntity(EntityHandle entity, const Vec3& position);
    std::expected<void, EngineError> RotateEntity(EntityHandle entity, const Vec3& rotation);
    std::expected<void, EngineError> ScaleEntity(EntityHandle entity, const Vec3& scale);

    std::expected<EntityHandle, EngineError> CreateEntity(const std::string& name);
    std::expected<void, EngineError> DestroyEntity(EntityHandle entity);

    std::expected<void, EngineError> AddComponent(EntityHandle entity, const std::string& componentType);
    std::expected<void, EngineError> RemoveComponent(EntityHandle entity, const std::string& componentType);

    // 2차 확장 범위
    std::expected<void, EngineError> SetProperty(
        EntityHandle entity,
        const std::string& propertyPath,
        const nlohmann::json& value
    );

private:
    EditorAPI() = default;
    
    // 비동기 API (필요 시 별도 구현)
    // template<typename T>
    // std::future<std::expected<T, EngineError>> DispatchAsync(
    //     std::function<std::expected<T, EngineError>()> fn);
};

} // namespace Engine::Editor
```

---

### Transaction API 수정 원칙
기존 `BeginTransaction()` / `EndTransaction()`만 두는 방식은 실패 시 커밋/취소가 모호하다.  
따라서 `EndTransaction()`은 폐기하고 아래로 분리한다.

- `CommitTransaction()`
- `CancelTransaction()`

또한 Python에서는 가능한 경우 다음과 같은 context manager 패턴을 제공하는 것이 가장 안전하다.

```python
with editor.transaction("Create and Configure Entity"):
    entity = editor.create_entity("NewObject")
    editor.add_component(entity, "TransformComponent")
    editor.move_entity(entity, Vec3(0, 0, 0))
```

이 패턴의 장점은 명확하다.  
정상 종료 시 commit, 예외 발생 시 cancel/rollback을 강제할 수 있다.

---

### typed API 우선 전략
초기부터 `set_property(path, json)` 중심으로 가면 reflection, 타입 체크, 에러 메시지, nested write-back 문제 때문에 구현 복잡도가 급상승한다.  
따라서 1차는 다음처럼 **명시적 API 중심**으로 간다.

| 1차 제공 API | 목적 |
|---|---|
| `move_entity` | 위치 이동 |
| `rotate_entity` | 회전 수정 |
| `scale_entity` | 스케일 수정 |
| `create_entity` | 엔티티 생성 |
| `destroy_entity` | 엔티티 삭제 |
| `add_component` | 컴포넌트 추가 |
| `remove_component` | 컴포넌트 제거 |

그 후 빈도가 높은 속성부터 typed setter를 늘리고, 마지막에 범용 `set_property()`를 fallback으로 추가한다.

---

### Editor API의 책임
Editor API는 단순 래퍼가 아니라 다음을 책임진다.

| 책임 | 설명 |
|---|---|
| 입력 검증 | 유효하지 않은 Entity, 잘못된 타입, 존재하지 않는 Component 차단 |
| Command 생성 | 편집 요청을 구체적인 Command로 전환 |
| Transaction 연결 | 현재 transaction 존재 시 묶어서 저장 |
| 에러 표준화 | Python/UI에서 일관된 오류 처리 가능 |
| 수정 경로 일원화 | direct ECS mutation 차단 |

---

### 검증 기준
- [ ] Python과 UI가 동일한 Editor API를 사용
- [ ] Editor API의 쓰기 작업이 모두 Command로 변환됨
- [ ] Commit/Cancel semantics가 분명함
- [ ] 예외 발생 시 트랜잭션 rollback 동작
- [ ] typed API만으로 핵심 편집 시나리오 처리 가능
- [ ] Undo/Redo가 Editor API 경유 작업에 대해 일관되게 동작

---

## 5.3 P1: Python 바인딩 경계 강화

### 문제 정의
현재 `def_readwrite` 기반 바인딩은 Python이 엔진 내부 구조를 너무 많이 알게 하고, 메모리 직접 수정을 허용한다.  
이 구조는 빠르게 만들기엔 편하지만 아키텍처 경계를 무너뜨린다.

### 목표
Python은 엔진 내부 객체를 “직접 수정”하지 않고, “조회”와 “편집 요청”을 분리해서 다루도록 한다.

---

### 최종 방향

#### 1) 쓰기 가능한 Component 필드 직접 노출 제거
```cpp
// 변경 전
py::class_<TransformComponent>(m, "TransformComponent")
    .def_readwrite("position", &TransformComponent::position)
    .def_readwrite("rotation", &TransformComponent::rotation)
    .def_readwrite("scale", &TransformComponent::scale);

// 변경 후
py::class_<TransformComponentView>(m, "TransformComponentView")
    .def_property_readonly("position", &TransformComponentView::GetPosition)
    .def_property_readonly("rotation", &TransformComponentView::GetRotation)
    .def_property_readonly("scale", &TransformComponentView::GetScale);
```

여기서 중요한 건 단순 readonly가 아니라, **가능하면 snapshot/value copy 기반 조회 객체**로 바꾸는 것이다.

**View 스냅샷 수명 의미론**: `TransformComponentView`는 조회 시점의 스냅샷이며 이후 변경이 자동 반영되지 않는다. Python 코드에서 이를 참조처럼 오해하면 안 됨. API 문서에 명시 필요.

---

#### 2) 수정은 Editor API만 허용
```python
# 금지
entity.transform.position.x = 100

# 허용
editor.move_entity(entity, Vec3(100, 0, 0))
```

---

#### 3) 내부용 바인딩은 분리
테스트 또는 디버깅이 필요하다면 내부 전용 바인딩을 별도로 두되, 일반 배포 경로와 분리한다.

```cpp
#ifdef ENABLE_INTERNAL_BINDINGS
// test/debug only
#endif
```

---

#### 4) Python 접근 계층 정리
Python 측에서는 다음 세 층만 남긴다.

| 계층 | 허용 여부 | 역할 |
|---|---|---|
| World/Entity 조회 | 허용 | 탐색, 선택, 상태 조회 |
| Component 읽기 | 제한적 허용 | 읽기 전용 snapshot |
| 상태 수정 | Editor API만 허용 | 실제 편집 작업 |

---

### 마이그레이션 방침
모든 Python 코드를 한 번에 교체하지 않고, 편집 기능이 실제로 많이 쓰이는 경로부터 옮긴다.

1. Transform 편집
2. Entity 생성/삭제
3. Component 추가/제거
4. Inspector 계열 속성 수정
5. 자동화 도구 및 스크립트

---

### 검증 기준
- [ ] Python에서 Component 필드 직접 수정 불가
- [ ] readonly 조회는 가능
- [ ] 수정은 Editor API만 통해 가능
- [ ] 기존 편집 기능의 핵심 경로가 정상 동작
- [ ] 내부 테스트용 바인딩과 일반 바인딩이 분리됨
- [ ] **스레드 안전성: Python GIL 하에서 엔진 메인 스레드와 동기화**

---

## 5.4 P2: 데이터 소유권 명확화

### 문제 정의
지금은 엔진이 실제 데이터를 소유하지만, Python 바인딩이 직접 메모리 접근을 허용하면서 소유권 경계가 흐려져 있다.  
이 상태에서는 stale pointer/invalid handle 문제를 구조적으로 막기 어렵다.

### 목표
Python은 엔티티를 객체가 아니라 **Handle**로 다루고, 유효성 검사는 항상 엔진 registry가 수행한다.

---

### 최종 설계 방향

#### EntityHandle 도입
```cpp
struct EntityHandle {
    EntityID id;
    uint32_t generation;
};
```

핵심은 `EntityHandle`이 스스로 유효성을 증명하지 않는다는 점이다.  
검증은 반드시 registry가 수행한다.

```cpp
class EntityRegistry {
public:
    bool IsValid(EntityHandle handle) const;
    
    // Handle 복원 API (Create/Destroy Command용)
    std::expected<void, EngineError> RestoreEntity(EntityHandle handle);
};
```

즉, 아래 구조를 원칙으로 한다.

| 요소 | 책임 |
|---|---|
| Handle | ID + generation 보관 |
| Registry | 유효성 판단 |
| Editor API | 사용 전 검증 및 오류 변환 |
| Python | handle 전달만 수행 |

---

### stale handle 방어 시나리오
다음 경우를 명시적으로 방어 대상에 포함한다.

| 시나리오 | 기대 동작 |
|---|---|
| 삭제된 Entity를 Python이 계속 보유 | `StaleEntityHandle` 반환 |
| World reload 후 예전 handle 사용 | `StaleEntityHandle` 반환 |
| World/Scene 로드 직후 Undo/Redo 메뉴 | CommandManager::Clear() 호출로 비활성화 |
| Undo/Redo로 객체 수명주기 변경 | generation mismatch 감지 |
| 다중 선택 중 일부 삭제 후 편집 | invalid handle 개별 보고 |

---

### 오류 코드 확장
```cpp
enum class ErrorCode {
    InvalidEntityID,
    StaleEntityHandle,
    MissingComponent,
    DuplicateComponent,
    InvalidPropertyPath,
    TypeMismatch,
    TransactionAlreadyOpen,
    NoOpenTransaction,
    NotMergeable,
    InternalCommandFailure
};
```

---

### 검증 기준
- [ ] Python은 Entity 포인터를 직접 소유하지 않음
- [ ] 모든 편집 API가 handle 유효성 검사를 수행
- [ ] 삭제된 Entity 접근 시 `StaleEntityHandle` 발생
- [ ] generation 기반 stale handle 탐지 동작
- [ ] 엔진이 데이터 수명주기를 일관되게 관리
- [ ] **스레드 안전성: CommandManager/EditorAPI 메인 스레드 전용**
- [ ] **스레드 안전성: Python GIL 하에서 엔진 메인 스레드와 동기화**

---

## 5.5 P2: 범용 Property API 도입

### 도입 시점
이 항목은 P0가 아니라 P2다.  
Undo/Redo와 Editor API의 typed 경로가 안정화된 뒤 도입한다.

### 이유
`set_property(entity, "transform.position", value)`는 매우 강력하지만, 다음 요구 사항이 선행되어야 한다.

- reflection metadata
- property path resolver
- JSON ↔ 엔진 타입 변환
- 타입 검증
- nested property write-back
- 에러 메시지 표준화

따라서 이 기능은 “초기 성공 조건”이 아니라 “확장성 확보”로 위치를 조정한다.

---

## 5.6 P3: 레거시 정리 및 프로젝트 구조 최적화

### 원칙
이 작업은 필요하지만 P0 착수를 지연시키면 안 된다.  
따라서 **time-box 2~3일** 내로 마무리하는 전제로 수행한다.

### 범위

#### 문서 정리
- 오래된 레거시 보고서는 `docs/archive/legacy/`로 이동
- 현재 상태 문서는 본 개선 계획서 하나를 기준 문서로 사용
- 완료 보고 성격 문서는 active 문서군에서 분리

#### 빌드 아티팩트 정리
- `.gitignore` 최신화
- 대형 결과 파일 제거
- 중복 빌드 디렉토리 정리
- 단일 빌드 디렉토리 전략 수립
- **Git 히스토리 정리 여부 결정** (baseline_test_results.txt가 이미 커밋된 경우)
  - 저장소 용량 감소 필요 시: `git filter-repo` 또는 BFG Repo-Cleaner 적용
  - 용량 문제 없으면 워킹 트리 정리만으로 충분
  - P3 time-box 안에서 적용 여부 결정

#### 복구 스크립트 처리
- 계속 필요하면 `scripts/archive/`로 이동
- 필요 없으면 제거
- “현재 활성 경로”와 “복구 보관물”을 구분

---

### .gitignore 권장안
```gitignore
# Build directories
build/
engine/build/
engine/build_*/

# Test outputs
baseline_test_results.txt
*_test_results.txt
```

---

### 검증 기준
- [ ] 레거시 문서가 archive로 이동
- [ ] 불필요한 빌드 아티팩트 제거
- [ ] `.gitignore` 반영 완료
- [ ] 단일 빌드 디렉토리 운영 원칙 정리
- [ ] Git 히스토리 정리 적용 여부 결정 및 완료
- [ ] P0 착수를 지연시키지 않음

---

## 6. 구현 단계

## Phase 0: 프로젝트 구조 정리 (2~3일, time-box)
이 단계의 목적은 청소 자체가 아니라, P0 작업을 방해하는 잡음을 제거하는 것이다.

진행 항목은 다음으로 제한한다.

1. 레거시 문서 archive 이동  
2. `.gitignore` 갱신  
3. 대형 테스트 결과 파일 제거  
4. 중복 빌드 디렉토리 정책 정리  

이 단계는 길게 끌지 않는다.

---

## Phase 1: 최소 수직 슬라이스 완성 (2주)
이 단계에서 가장 중요한 목표는 “설계가 실제로 작동하는지”를 검증하는 것이다.

완성 대상은 아래와 같다.

- `MoveEntityCommand`
- `CommandManager`
- `EditorAPI.move_entity`
- Python 또는 UI에서 이동 후 Undo/Redo

즉, 넓게 만들기보다 **하나를 끝까지 관통**한다.

### 성공 조건
- entity 이동이 command로 기록됨
- undo/redo 동작
- direct mutation 없이 편집 가능

---

## Phase 2: 핵심 편집 인프라 확장 (2~3주)
1차 슬라이스가 검증되면 편집 범위를 확장한다.

대상은 다음과 같다.

- Rotate/Scale command
- Create/Destroy entity command
- Add/Remove component command
- Transaction apply/rollback
- Commit/Cancel API

### 성공 조건
- 복합 편집 작업이 하나의 undo 단위로 묶임
- 실패 시 rollback 동작
- 스택 정합성 유지

---

## Phase 3: Python 경계 강화 및 마이그레이션 (2~3주)
이 단계에서는 바인딩과 기존 스크립트 경로를 정리한다.

진행 내용은 다음과 같다.

- `def_readwrite` 제거 또는 readonly화
- Python 편집 코드의 Editor API 전환
- Inspector/툴 스크립트 마이그레이션
- direct mutation runtime 차단 확인

### 성공 조건
- 편집 변경이 모두 공식 경로를 탐
- 기존 주요 기능 회귀 없음
- Python이 제2의 엔진 계층이 되지 않음

---

## Phase 4: 소유권 강화 및 범용화 (1~2주)
이 단계에서는 안전성과 확장성을 높인다.

대상은 다음과 같다.

- EntityHandle + generation 검증
- stale handle 오류 처리
- 자주 쓰는 typed setter 보강
- 필요 시 `set_property(path, json)` 도입 시작

---

## Phase 5: 통합·최적화·문서화 (1주)
마지막 단계에서는 다음을 정리한다.

- 전체 회귀 테스트
- command merge/coalescing 튜닝
- 문서 업데이트
- 체크리스트 재검증

---

## 7. 일정 산정

### 권장 일정
| 구분 | 기간 |
|---|---|
| 최소 동작 가능한 핵심 슬라이스 | 4~6주 |
| 전체 마이그레이션 + 안정화 포함 | 8~12주 |

기존 초안의 7~10주는 가능은 하지만 다소 낙관적이다.  
특히 범용 property API, 바인딩 마이그레이션, stale handle 방어까지 포함하면 **8~12주**가 더 현실적이다.

---

## 8. 최종 검증 기준

## 기능 관점
- [ ] Ctrl+Z / Ctrl+Y가 실제 편집 동작과 연결됨
- [ ] 복합 작업이 하나의 undo 단위로 처리됨
- [ ] 연속 이동/드래그가 병합됨
- [ ] 예외 발생 시 rollback 보장

## 경계 관점
- [ ] Python에서 Component 직접 수정 불가
- [ ] 쓰기 작업은 Editor API만 사용
- [ ] 엔진이 데이터 소유권을 유지
- [ ] stale handle 접근이 안전하게 차단됨

## 구조 관점
- [ ] Editor API가 편집의 단일 진입점이 됨
- [ ] CommandManager가 히스토리를 일관되게 관리
- [ ] 레거시 문서/빌드 구조가 혼동을 주지 않음
- [ ] 문서와 실제 구현이 일치함

---

## 9. 아키텍처 재검증 목표

| 점검 항목 | 현재 상태 | 목표 상태 |
|---|---|---|
| Engine 공유 | ✅ 합격 | ✅ 유지 |
| 프로세스 구조 | ✅ 합격 | ✅ 유지 |
| World 관리 | ✅ 합격 | ✅ 유지 |
| 데이터 소유권 | ⚠️ 부분 합격 | ✅ 합격 |
| Reflection | ✅ 합격 | ✅ 유지 |
| Serialization | ✅ 합격 | ✅ 유지 |
| Command/Transaction | ❌ 불합격 | ✅ 합격 |
| 생산성 | ✅ 합격 | ✅ 유지 |
| Python 경계 | ⚠️ 부분 합격 | ✅ 합격 |

---

## 10. 최종 결론

이번 개선의 핵심은 기능 추가가 아니라 **편집 아키텍처의 통제권 회수**다.  
지금 문제는 Undo/Redo가 없다는 사실 자체보다, 상태 변경 경로가 분산되어 있다는 데 있다.

따라서 최종 목표는 아래 한 문장으로 요약된다.

> **엔진 상태를 바꾸는 모든 편집은 Editor API를 통해 Command로 기록되고, 필요 시 Transaction으로 묶이며, Python은 그 경계를 넘지 못한다.**

이 원칙이 구현되면 다음 효과를 기대할 수 있다.

- Undo/Redo가 구조적으로 가능해진다
- Python과 엔진의 책임 경계가 명확해진다
- 데이터 소유권과 수명주기가 안정된다
- 아키텍처 문서와 실제 구현의 정합성이 회복된다
- 장기 유지보수 비용이 크게 낮아진다

---






















2
#  아키텍처 개선 계획서

**작성일**: 2026-07-30  
**기준**: 아키텍처 검증 체크리스트 점검 결과 및 리뷰 반영본  
**목표**: 문서와 실제 구조의 정합성 확보, Python/엔진 경계 확립, Undo/Redo 가능한 편집 인프라 완성

---

## 1. 종합 결론

**결론 한 줄**: 현재 구조는 엔진 코어의 방향은 건강하지만, 편집 변경이 `Editor API → Command/Transaction` 경로로 강제되지 않아 아키텍처 정합성이 깨지고 있으므로, 이를 중심으로 단계적 재구성이 필요하다.

현재 상태는 **6/9 합격(67%)**이며, 핵심 미해결 영역은 다음 세 가지다.

| 영역 | 현재 상태 | 핵심 문제 | 목표 상태 |
|---|---|---|---|
| Command/Transaction | ❌ 불합격 | 편집 변경의 작업 단위 기록 부재 | 모든 편집 작업의 Undo/Redo 가능 |
| Python 경계 | ⚠️ 부분 합격 | Python이 ECS 내부를 직접 수정 가능 | Python은 Editor API만 통해 수정 |
| 데이터 소유권 | ⚠️ 부분 합격 | Handle/수명주기/유효성 경계 불명확 | 엔진 소유권 고정 + stale handle 방어 |

이번 최종안의 핵심은 단순히 기능을 추가하는 것이 아니라, 아래 구조를 강제하는 데 있다.

> **모든 편집 변경은 반드시 `Editor API → Command → CommandManager` 경로를 통과한다.**

이 원칙이 지켜지면 체크리스트의 `Command/Transaction`, `Python 경계`, `데이터 소유권` 항목이 함께 개선된다.

---

## 2. 아키텍처 목표와 설계 원칙

이번 개선은 기능 추가가 아니라 **편집 아키텍처의 재정렬**이다.  
이 문서에서 적용하는 최종 원칙은 다음과 같다.

### 2.1 단일 수정 경로 원칙
엔진 상태를 바꾸는 모든 편집 행위는 `Editor API`를 통해서만 수행한다.  
Python, UI, 툴 스크립트, 자동화 로직 모두 예외 없이 같은 경로를 사용한다.

### 2.2 명령 기반 편집 원칙
편집은 즉시 상태를 바꾸는 호출이 아니라, **실행·취소 가능한 명령**으로 표현한다.  
Undo/Redo는 부가 기능이 아니라 편집 시스템의 기본 계약으로 취급한다.

### 2.3 엔진 소유권 유지 원칙
Entity/Component의 실제 수명주기와 메모리 소유권은 항상 엔진에 있다.  
Python은 데이터를 소유하지 않고, ID/Handle과 읽기 전용 스냅샷만 다룬다.

### 2.4 범용화의 단계적 도입 원칙
초기에는 `set_property(path, json)` 같은 범용 API보다, `move_entity`, `rotate_entity`, `add_component` 같은 **typed API**를 우선 구현한다.  
범용 reflection 기반 속성 수정은 2차 단계에서 도입한다.

### 2.5 실패 가능성의 명시 원칙
트랜잭션 원자성을 보장하려면 명령 인터페이스부터 실패를 표현해야 한다.  
따라서 `void Execute()` 중심 설계는 지양하고, `std::expected<void, EngineError>` 기반으로 통일한다.

---

## 3. 우선순위별 최종 로드맵

| 우선순위 | 항목 | 목표 | 예상 난이도 | 비고 |
|---|---|---|---|---|
| 🔴 P0 | Command/Transaction 기반 인프라 | Undo/Redo 가능한 편집 기록 체계 구축 | 높음 | 최우선 |
| 🔴 P0 | Editor API 레이어 도입 | 편집 변경 경로 단일화 | 높음 | Command와 동시 진행 |
| 🟡 P1 | Python 바인딩 경계 강화 | 직접 메모리 수정 차단 | 중간 | P0 완료 직후 |
| 🟡 P1 | 핵심 Python 도구 마이그레이션 | 기존 스크립트 경로 전환 | 중간 | 점진적 |
| 🟢 P2 | 데이터 소유권 명확화 | Handle/Generation 기반 안전성 확보 | 중간 | 경계 강화 후 적용 |
| 🟢 P2 | 범용 Property API | Reflection 기반 속성 수정 확장 | 중간~높음 | typed API 안정화 후 |
| 🟢 P3 | 레거시 정리 및 프로젝트 구조 최적화 | 문서/빌드 구조 단순화 | 낮음 | time-box로 제한 |
| 🔵 P3 | Command 직렬화/복원 | 세션 저장/복원 확장 | 중간 | 초기 범위에서 제외 |

---

## 4. 이론상 구조와 현재 구조의 차이

| 항목 | 바람직한 구조 | 현재 구조 | 개선 방향 |
|---|---|---|---|
| 편집 요청 진입점 | Editor API 단일 진입 | Python/UI가 직접 ECS 접근 가능 | 모든 쓰기 경로를 Editor API로 집중 |
| 상태 변경 단위 | ICommand 기반 기록 | 함수 호출이 즉시 상태 변경 | Command로 캡슐화 |
| Undo/Redo | CommandManager가 관리 | 메뉴만 있고 실제 인프라 없음 | undo/redo stack 구현 |
| 복합 작업 | Transaction 단위 | 다중 작업이 분산 호출 | atomic transaction 도입 |
| Python 바인딩 | 읽기 전용 + 명시적 수정 API | `def_readwrite`로 직접 수정 노출 | 읽기 전용 스냅샷으로 축소 |
| Entity 유효성 | Handle + generation 검증 | 단순 ID 접근 가능성 | stale handle 방어 추가 |
| 범용 속성 수정 | reflection 기반 fallback | 직접 필드 접근 | typed API 우선 후 확장 |

---

## 5. 상세 개선 계획

---

## 5.1 P0-1: Command/Transaction 시스템 구현

### 문제 정의
현재는 Undo/Redo 메뉴가 존재하더라도 실제 편집 변경이 명령 객체로 기록되지 않기 때문에, 구조적으로 되돌리기가 불가능하다.  
또한 복합 작업을 하나의 작업 단위로 묶는 트랜잭션 개념이 없어, 에디터 동작의 일관성이 깨질 수 있다.

### 최종 설계 방향
기존 초안의 `void Execute()/Undo()/Redo()` 구조는 실패 처리와 원자성 보장에 약하므로, 다음처럼 수정한다.

### 핵심 인터페이스

```cpp
// engine/core/ICommand.h
#pragma once
#include <expected>
#include <string>
#include "EngineError.h"

namespace Engine {

class ICommand {
public:
    virtual ~ICommand() = default;

    // 실제 적용
    virtual std::expected<void, EngineError> Apply() = 0;

    // 적용 취소
    virtual std::expected<void, EngineError> Undo() = 0;

    // Command 병합 가능 여부
    virtual bool CanMergeWith(const ICommand& other) const { return false; }

    // 연속 조작 병합
    virtual std::expected<void, EngineError> MergeWith(const ICommand& other) {
        return std::unexpected(EngineError::NotMergeable());
    }

    virtual std::string GetName() const = 0;
};

} // namespace Engine
```

이 구조의 장점은 명확하다.  
`Redo()`를 별도 강제하지 않고, 재적용은 `Apply()`를 다시 호출하는 방식으로 단순화할 수 있다.  
또한 실패 가능성이 인터페이스에 드러나므로 Transaction의 원자성 구현이 가능해진다.

---

### Transaction 설계

Transaction은 단순 Composite가 아니라, **커밋/취소/부분 실패 롤백 정책**을 가진 편집 단위여야 한다.

```cpp
// engine/core/Transaction.h
#pragma once
#include "ICommand.h"
#include <memory>
#include <vector>

namespace Engine {

class Transaction : public ICommand {
public:
    explicit Transaction(std::string name);

    void AddCommand(std::unique_ptr<ICommand> command);
    bool Empty() const;
    size_t Size() const;

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;

    std::string GetName() const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    size_t appliedCount_ = 0;
};

} // namespace Engine
```

### 트랜잭션 정책
이번 최종안에서는 정책을 다음처럼 명확히 한다.

| 정책 항목 | 최종 결정 |
|---|---|
| 원자성 | 기본적으로 atomic |
| 중간 실패 | 이미 적용된 command를 역순 Undo하여 rollback |
| 빈 트랜잭션 | undo stack에 넣지 않음 |
| 중첩 트랜잭션 | 1차 구현에서는 금지 또는 flatten 중 하나를 명시적으로 택함 |
| 권장안 | 1차 구현은 **중첩 금지**, 이후 필요 시 flatten 지원 |

중첩 트랜잭션은 초기에 허용하면 디버깅 난도가 급상승하므로, 1차 구현에서는 금지하고 에러를 반환하는 편이 안전하다.

---

### CommandManager 설계

초기 구현은 singleton으로 시작할 수 있으나, 최종 구조는 향후 `EditorSession` 또는 `EditorContext` 소유로 이전 가능하게 설계한다.

```cpp
// engine/core/CommandManager.h
#pragma once
#include "ICommand.h"
#include <expected>
#include <memory>
#include <vector>

namespace Engine {

class CommandManager {
public:
    static CommandManager& GetInstance(); // 초기 구현용
    
    // 향후 EditorSession/EditorContext 소유로 이전 가능하게 설계
    // 현재는 싱글톤으로 메인 스레드 전용으로 운용

    std::expected<void, EngineError> Execute(std::unique_ptr<ICommand> command);
    std::expected<void, EngineError> Undo();
    std::expected<void, EngineError> Redo();

    bool CanUndo() const;
    bool CanRedo() const;

    void Clear();

    void SetMaxUndoDepth(size_t depth);
    size_t GetMaxUndoDepth() const;

private:
    CommandManager() = default;

    std::vector<std::unique_ptr<ICommand>> undoStack_;
    std::vector<std::unique_ptr<ICommand>> redoStack_;
    size_t maxUndoDepth_ = 100;
    
    // 스레드 안전성: 메인 스레드 전용으로 설계
    // Python 호출은 GIL 하에서 엔진 메인 스레드와 동기화 필요
};

} // namespace Engine
```

### 커맨드 병합 정책
드래그 이동, 슬라이더 변경, 연속 입력 같은 작업은 개별 명령을 모두 스택에 넣지 말고 **병합(coalescing)** 해야 한다.

**병합 실행 위치**: CommandManager::Execute()에서 undo 스택 top과 비교하여 병합 여부 판단

**시간 윈도우 및 병합 세션**: 병합을 순수 타입/엔티티 기준으로만 판단하면 "물체를 옮기고 → 다른 작업을 하고 → 한참 뒤에 같은 물체를 다시 옮긴" 경우까지 병합될 위험이 있으므로, 드래그 시작~끝과 같은 명시적 "병합 세션" 개념을 함께 사용

**명시적 Merge Session API**:
```cpp
class CommandManager {
public:
    void BeginMergeSession();
    void EndMergeSession();
    
private:
    bool inMergeSession_ = false;
};
```

`Execute()`는 `inMergeSession_ == true`일 때만 undo 스택 top과의 `CanMergeWith()`를 시도한다. UI 쪽에서는 기즈모 드래그의 mouse-down/up, 인스펙터 슬라이더의 drag-start/end에 각각 `BeginMergeSession()`/`EndMergeSession()`을 건다. 포커스 상실 이벤트에도 세션을 강제 종료하는 안전장치 필요.

따라서 병합 정책은 단순 `bool strategy`가 아니라, 각 Command가 직접 병합 가능성과 병합 로직을 가진다.

예시:
- `MoveEntityCommand(A, pos1)` 이후 `MoveEntityCommand(A, pos2)`가 연속으로 오면 병합
- 최종적으로 undo 시 한 번에 원래 위치로 복귀

**MergeWith 구현 시 유의사항**: 병합된 커맨드의 undo 기준값(예: MoveEntityCommand의 oldPosition)은 최초 병합 대상의 값을 유지해야 하며, 매 병합마다 갱신하면 안 됨

---

### Create/Destroy 커맨드의 Handle 안정성 (핵심 설계)

**문제**: Redo()를 없애고 Apply() 재호출로 대체한 설계는 깔끔하지만, CreateEntityCommand의 경우 핵심 문제를 만듭니다.

```cpp
Transaction:
  1. CreateEntityCommand → handle H 반환
  2. AddComponentCommand(H)
  3. MoveEntityCommand(H, pos)
```

이 트랜잭션을 Undo했다가 다시 Redo(=Apply() 재호출)하면, CreateEntityCommand::Apply()가 원래와 동일한 H(같은 id, 같은 generation)를 재생성해야 합니다. 그렇지 않으면 트랜잭션 내부의 2번·3번 커맨드가 들고 있는 H가 stale handle이 되어 Redo 자체가 실패합니다.

**해결 방안**: CreateEntityCommand가 최초 Apply()에서 발급받은 handle을 멤버로 저장해두고, 이후 재적용 시에는 registry에 "이 id/generation으로 복원해달라"고 요청

```cpp
// engine/ecs/ECSRegistry.h (확장)
class ECSRegistry {
public:
    // Handle 복원 API
    std::expected<void, EngineError> RestoreEntity(EntityHandle handle);
    
    // 기존 유효성 검사
    bool IsValid(EntityHandle handle) const;
};
```

**CreateEntityCommand 구현 예시**:
```cpp
class CreateEntityCommand : public ICommand {
public:
    CreateEntityCommand(const std::string& name);
    
    std::expected<void, EngineError> Apply() override {
        if (originalHandle.IsValid()) {
            // Handle이 이미 있으면 복원 모드
            return registry->RestoreEntity(originalHandle);
        } else {
            // 최초 생성 모드
            auto result = registry->CreateEntity(name);
            if (result) {
                originalHandle = *result;
                return {};
            }
            return std::unexpected(EngineError::EntityCreationFailed());
        }
    }
    
    std::expected<void, EngineError> Undo() override {
        return registry->DestroyEntity(originalHandle);
    }
    
private:
    std::string name;
    EntityHandle originalHandle;  // Apply() 시 발급받은 handle 저장
    ECSRegistry* registry;
};
```

**DestroyEntityCommand 구현 예시**:
```cpp
class DestroyEntityCommand : public ICommand {
public:
    DestroyEntityCommand(EntityHandle entity);
    
    std::expected<void, EngineError> Apply() override {
        // Destroy 시 상태 저장
        destroyedHandle = entity;
        // entity 삭제
        return registry->DestroyEntity(entity);
    }
    
    std::expected<void, EngineError> Undo() override {
        // 삭제된 entity 복원
        return registry->RestoreEntity(destroyedHandle);
    }
    
private:
    EntityHandle destroyedHandle;
    ECSRegistry* registry;
};
```

**핵심 원칙**: Undo→Redo 사이클에서 handle이 동일하게 유지되어야 함

---

### 구현 대상 Command 우선순위
P0에서는 범위를 좁혀 아래 항목부터 구현한다.

| 1차 대상 | 이유 |
|---|---|
| MoveEntityCommand | 가장 대표적인 편집 작업 |
| RotateEntityCommand | Transform 계열 공통성 확보 |
| ScaleEntityCommand | Transform 작업 완결 |
| CreateEntityCommand | 수명주기 작업 시작점 |
| DestroyEntityCommand | 삭제/복원 검증 필요 |
| AddComponentCommand | 구성 변경 필수 |
| RemoveComponentCommand | 편집 완결성 확보 |

`SetPropertyCommand`와 범용 reflection 명령은 1차 성공 후 추가한다.

---

### P0 범위에서 제외할 항목
초기 일정 안정성을 위해 아래는 P0 핵심 목표에서 제외한다.

| 제외 항목 | 이유 |
|---|---|
| Command 직렬화/역직렬화 | 기능은 유용하지만 초기 구현 난이도 급상승 |
| 세션 저장/복원 연계 | Undo 인프라 안정화 이후 검토 |
| 중첩 트랜잭션 완전 지원 | 초기에는 정책 단순화 우선 |
| 범용 Property Path 수정 | typed API 안정화 후 확장 |

---

### 검증 기준
- [ ] 단일 Command의 Apply/Undo 정상 동작
- [ ] Transaction이 여러 Command를 하나의 undo 단위로 묶음
- [ ] Transaction 중간 실패 시 rollback 수행
- [ ] 빈 Transaction은 스택에 쌓이지 않음
- [ ] 연속 이동 작업 병합 가능
- [ ] Ctrl+Z / Ctrl+Y가 실제 상태 변경과 연결됨
- [ ] undo/redo stack depth 제한 동작
- [ ] 실패 시 EngineError가 상위 계층까지 전달됨
- [ ] **Undo→Redo 사이클에서 handle이 동일하게 유지되는가** (Create/Destroy 핵심)

---

## 5.2 P0-2: Editor API 레이어 도입

### 문제 정의
현재 구조에서는 Python이 ECS와 Component 내부 필드를 직접 수정할 수 있어, 편집 행위가 Command 시스템 바깥으로 새고 있다.  
이 상태에서는 Undo/Redo를 도입해도 우회 경로 때문에 일관성이 보장되지 않는다.

### 목표
Editor API를 편집의 공식 인터페이스로 정의하고, **모든 쓰기 작업이 내부적으로 Command로 전환**되도록 한다.

---

### 최종 인터페이스 방향

```cpp
// engine/editor/EditorAPI.h
#pragma once
#include <expected>
#include <string>
#include "EngineError.h"
#include "EntityHandle.h"
#include "Types.h"

namespace Engine::Editor {

class EditorAPI {
public:
    static EditorAPI& GetInstance();
    
    // 향후 EditorSession/EditorContext 소유로 이전 가능하게 설계
    // 현재는 싱글톤으로 메인 스레드 전용으로 운용
    // 멀티 씬/멀티 문서 편집 고려

    // Transaction lifecycle
    std::expected<void, EngineError> BeginTransaction(const std::string& name);
    std::expected<void, EngineError> CommitTransaction();
    std::expected<void, EngineError> CancelTransaction();
    bool IsInTransaction() const;

    // Typed editing APIs - 1차 핵심 범위
    std::expected<void, EngineError> MoveEntity(EntityHandle entity, const Vec3& position);
    std::expected<void, EngineError> RotateEntity(EntityHandle entity, const Vec3& rotation);
    std::expected<void, EngineError> ScaleEntity(EntityHandle entity, const Vec3& scale);

    std::expected<EntityHandle, EngineError> CreateEntity(const std::string& name);
    std::expected<void, EngineError> DestroyEntity(EntityHandle entity);

    std::expected<void, EngineError> AddComponent(EntityHandle entity, const std::string& componentType);
    std::expected<void, EngineError> RemoveComponent(EntityHandle entity, const std::string& componentType);

    // 2차 확장 범위
    std::expected<void, EngineError> SetProperty(
        EntityHandle entity,
        const std::string& propertyPath,
        const nlohmann::json& value
    );

private:
    EditorAPI() = default;
};

} // namespace Engine::Editor
```

---

### Transaction API 수정 원칙
기존 `BeginTransaction()` / `EndTransaction()`만 두는 방식은 실패 시 커밋/취소가 모호하다.  
따라서 `EndTransaction()`은 폐기하고 아래로 분리한다.

- `CommitTransaction()`
- `CancelTransaction()`

또한 Python에서는 가능한 경우 다음과 같은 context manager 패턴을 제공하는 것이 가장 안전하다.

```python
with editor.transaction("Create and Configure Entity"):
    entity = editor.create_entity("NewObject")
    editor.add_component(entity, "TransformComponent")
    editor.move_entity(entity, Vec3(0, 0, 0))
```

이 패턴의 장점은 명확하다.  
정상 종료 시 commit, 예외 발생 시 cancel/rollback을 강제할 수 있다.

---

### typed API 우선 전략
초기부터 `set_property(path, json)` 중심으로 가면 reflection, 타입 체크, 에러 메시지, nested write-back 문제 때문에 구현 복잡도가 급상승한다.  
따라서 1차는 다음처럼 **명시적 API 중심**으로 간다.

| 1차 제공 API | 목적 |
|---|---|
| `move_entity` | 위치 이동 |
| `rotate_entity` | 회전 수정 |
| `scale_entity` | 스케일 수정 |
| `create_entity` | 엔티티 생성 |
| `destroy_entity` | 엔티티 삭제 |
| `add_component` | 컴포넌트 추가 |
| `remove_component` | 컴포넌트 제거 |

그 후 빈도가 높은 속성부터 typed setter를 늘리고, 마지막에 범용 `set_property()`를 fallback으로 추가한다.

---

### Editor API의 책임
Editor API는 단순 래퍼가 아니라 다음을 책임진다.

| 책임 | 설명 |
|---|---|
| 입력 검증 | 유효하지 않은 Entity, 잘못된 타입, 존재하지 않는 Component 차단 |
| Command 생성 | 편집 요청을 구체적인 Command로 전환 |
| Transaction 연결 | 현재 transaction 존재 시 묶어서 저장 |
| 에러 표준화 | Python/UI에서 일관된 오류 처리 가능 |
| 수정 경로 일원화 | direct ECS mutation 차단 |

---

### 검증 기준
- [ ] Python과 UI가 동일한 Editor API를 사용
- [ ] Editor API의 쓰기 작업이 모두 Command로 변환됨
- [ ] Commit/Cancel semantics가 분명함
- [ ] 예외 발생 시 트랜잭션 rollback 동작
- [ ] typed API만으로 핵심 편집 시나리오 처리 가능
- [ ] Undo/Redo가 Editor API 경유 작업에 대해 일관되게 동작

---

## 5.3 P1: Python 바인딩 경계 강화

### 문제 정의
현재 `def_readwrite` 기반 바인딩은 Python이 엔진 내부 구조를 너무 많이 알게 하고, 메모리 직접 수정을 허용한다.  
이 구조는 빠르게 만들기엔 편하지만 아키텍처 경계를 무너뜨린다.

### 목표
Python은 엔진 내부 객체를 “직접 수정”하지 않고, “조회”와 “편집 요청”을 분리해서 다루도록 한다.

---

### 최종 방향

#### 1) 쓰기 가능한 Component 필드 직접 노출 제거
```cpp
// 변경 전
py::class_<TransformComponent>(m, "TransformComponent")
    .def_readwrite("position", &TransformComponent::position)
    .def_readwrite("rotation", &TransformComponent::rotation)
    .def_readwrite("scale", &TransformComponent::scale);

// 변경 후
py::class_<TransformComponentView>(m, "TransformComponentView")
    .def_property_readonly("position", &TransformComponentView::GetPosition)
    .def_property_readonly("rotation", &TransformComponentView::GetRotation)
    .def_property_readonly("scale", &TransformComponentView::GetScale);
```

여기서 중요한 건 단순 readonly가 아니라, **가능하면 snapshot/value copy 기반 조회 객체**로 바꾸는 것이다.

**View 스냅샷 수명 의미론**: `TransformComponentView`는 조회 시점의 스냅샷이며 이후 변경이 자동 반영되지 않는다. Python 코드에서 이를 참조처럼 오해하면 안 됨. API 문서에 명시 필요.

---

#### 2) 수정은 Editor API만 허용
```python
# 금지
entity.transform.position.x = 100

# 허용
editor.move_entity(entity, Vec3(100, 0, 0))
```

---

#### 3) 내부용 바인딩은 분리
테스트 또는 디버깅이 필요하다면 내부 전용 바인딩을 별도로 두되, 일반 배포 경로와 분리한다.

```cpp
#ifdef ENABLE_INTERNAL_BINDINGS
// test/debug only
#endif
```

---

#### 4) Python 접근 계층 정리
Python 측에서는 다음 세 층만 남긴다.

| 계층 | 허용 여부 | 역할 |
|---|---|---|
| World/Entity 조회 | 허용 | 탐색, 선택, 상태 조회 |
| Component 읽기 | 제한적 허용 | 읽기 전용 snapshot |
| 상태 수정 | Editor API만 허용 | 실제 편집 작업 |

---

### 마이그레이션 방침
모든 Python 코드를 한 번에 교체하지 않고, 편집 기능이 실제로 많이 쓰이는 경로부터 옮긴다.

1. Transform 편집
2. Entity 생성/삭제
3. Component 추가/제거
4. Inspector 계열 속성 수정
5. 자동화 도구 및 스크립트

---

### 검증 기준
- [ ] Python에서 Component 필드 직접 수정 불가
- [ ] readonly 조회는 가능
- [ ] 수정은 Editor API만 통해 가능
- [ ] 기존 편집 기능의 핵심 경로가 정상 동작
- [ ] 내부 테스트용 바인딩과 일반 바인딩이 분리됨
- [ ] **스레드 안전성: Python GIL 하에서 엔진 메인 스레드와 동기화**

---

## 5.4 P2: 데이터 소유권 명확화

### 문제 정의
지금은 엔진이 실제 데이터를 소유하지만, Python 바인딩이 직접 메모리 접근을 허용하면서 소유권 경계가 흐려져 있다.  
이 상태에서는 stale pointer/invalid handle 문제를 구조적으로 막기 어렵다.

### 목표
Python은 엔티티를 객체가 아니라 **Handle**로 다루고, 유효성 검사는 항상 엔진 registry가 수행한다.

---

### 최종 설계 방향

#### EntityHandle 도입
```cpp
struct EntityHandle {
    EntityID id;
    uint32_t generation;
};
```

핵심은 `EntityHandle`이 스스로 유효성을 증명하지 않는다는 점이다.  
검증은 반드시 registry가 수행한다.

```cpp
class EntityRegistry {
public:
    bool IsValid(EntityHandle handle) const;
    
    // Handle 복원 API (Create/Destroy Command용)
    std::expected<void, EngineError> RestoreEntity(EntityHandle handle);
};
```

즉, 아래 구조를 원칙으로 한다.

| 요소 | 책임 |
|---|---|
| Handle | ID + generation 보관 |
| Registry | 유효성 판단 |
| Editor API | 사용 전 검증 및 오류 변환 |
| Python | handle 전달만 수행 |

---

### stale handle 방어 시나리오
다음 경우를 명시적으로 방어 대상에 포함한다.

| 시나리오 | 기대 동작 |
|---|---|
| 삭제된 Entity를 Python이 계속 보유 | `StaleEntityHandle` 반환 |
| World reload 후 예전 handle 사용 | `StaleEntityHandle` 반환 |
| World/Scene 로드 직후 Undo/Redo 메뉴 | CommandManager::Clear() 호출로 비활성화 |
| Undo/Redo로 객체 수명주기 변경 | generation mismatch 감지 |
| 다중 선택 중 일부 삭제 후 편집 | invalid handle 개별 보고 |

---

### 오류 코드 확장
```cpp
enum class ErrorCode {
    InvalidEntityID,
    StaleEntityHandle,
    MissingComponent,
    DuplicateComponent,
    InvalidPropertyPath,
    TypeMismatch,
    TransactionAlreadyOpen,
    NoOpenTransaction,
    NotMergeable,
    InternalCommandFailure
};
```

---

### 검증 기준
- [ ] Python은 Entity 포인터를 직접 소유하지 않음
- [ ] 모든 편집 API가 handle 유효성 검사를 수행
- [ ] 삭제된 Entity 접근 시 `StaleEntityHandle` 발생
- [ ] generation 기반 stale handle 탐지 동작
- [ ] 엔진이 데이터 수명주기를 일관되게 관리
- [ ] **스레드 안전성: CommandManager/EditorAPI 메인 스레드 전용**
- [ ] **스레드 안전성: Python GIL 하에서 엔진 메인 스레드와 동기화**

---

## 5.5 P2: 범용 Property API 도입

### 도입 시점
이 항목은 P0가 아니라 P2다.  
Undo/Redo와 Editor API의 typed 경로가 안정화된 뒤 도입한다.

### 이유
`set_property(entity, "transform.position", value)`는 매우 강력하지만, 다음 요구 사항이 선행되어야 한다.

- reflection metadata
- property path resolver
- JSON ↔ 엔진 타입 변환
- 타입 검증
- nested property write-back
- 에러 메시지 표준화

따라서 이 기능은 “초기 성공 조건”이 아니라 “확장성 확보”로 위치를 조정한다.

---

## 5.6 P3: 레거시 정리 및 프로젝트 구조 최적화

### 원칙
이 작업은 필요하지만 P0 착수를 지연시키면 안 된다.  
따라서 **time-box 2~3일** 내로 마무리하는 전제로 수행한다.

### 범위

#### 문서 정리
- 오래된 레거시 보고서는 `docs/archive/legacy/`로 이동
- 현재 상태 문서는 본 개선 계획서 하나를 기준 문서로 사용
- 완료 보고 성격 문서는 active 문서군에서 분리

#### 빌드 아티팩트 정리
- `.gitignore` 최신화
- 대형 결과 파일 제거
- 중복 빌드 디렉토리 정리
- 단일 빌드 디렉토리 전략 수립
- **Git 히스토리 정리 여부 결정** (baseline_test_results.txt가 이미 커밋된 경우)
  - 저장소 용량 감소 필요 시: `git filter-repo` 또는 BFG Repo-Cleaner 적용
  - 용량 문제 없으면 워킹 트리 정리만으로 충분
  - P3 time-box 안에서 적용 여부 결정

#### 복구 스크립트 처리
- 계속 필요하면 `scripts/archive/`로 이동
- 필요 없으면 제거
- “현재 활성 경로”와 “복구 보관물”을 구분

---

### .gitignore 권장안
```gitignore
# Build directories
build/
engine/build/
engine/build_*/

# Test outputs
baseline_test_results.txt
*_test_results.txt
```

---

### 검증 기준
- [ ] 레거시 문서가 archive로 이동
- [ ] 불필요한 빌드 아티팩트 제거
- [ ] `.gitignore` 반영 완료
- [ ] 단일 빌드 디렉토리 운영 원칙 정리
- [ ] Git 히스토리 정리 적용 여부 결정 및 완료
- [ ] P0 착수를 지연시키지 않음

---

## 6. 구현 단계

## Phase 0: 프로젝트 구조 정리 (2~3일, time-box)
이 단계의 목적은 청소 자체가 아니라, P0 작업을 방해하는 잡음을 제거하는 것이다.

진행 항목은 다음으로 제한한다.

1. 레거시 문서 archive 이동  
2. `.gitignore` 갱신  
3. 대형 테스트 결과 파일 제거  
4. 중복 빌드 디렉토리 정책 정리  

이 단계는 길게 끌지 않는다.

---

## Phase 1: 최소 수직 슬라이스 완성 (2주)
이 단계에서 가장 중요한 목표는 “설계가 실제로 작동하는지”를 검증하는 것이다.

완성 대상은 아래와 같다.

- `MoveEntityCommand`
- `CommandManager`
- `EditorAPI.move_entity`
- Python 또는 UI에서 이동 후 Undo/Redo

즉, 넓게 만들기보다 **하나를 끝까지 관통**한다.

### 성공 조건
- entity 이동이 command로 기록됨
- undo/redo 동작
- direct mutation 없이 편집 가능

---

## Phase 2: 핵심 편집 인프라 확장 (2~3주)
1차 슬라이스가 검증되면 편집 범위를 확장한다.

대상은 다음과 같다.

- Rotate/Scale command
- Create/Destroy entity command
- Add/Remove component command
- Transaction apply/rollback
- Commit/Cancel API

### 성공 조건
- 복합 편집 작업이 하나의 undo 단위로 묶임
- 실패 시 rollback 동작
- 스택 정합성 유지

---

## Phase 3: Python 경계 강화 및 마이그레이션 (2~3주)
이 단계에서는 바인딩과 기존 스크립트 경로를 정리한다.

진행 내용은 다음과 같다.

- `def_readwrite` 제거 또는 readonly화
- Python 편집 코드의 Editor API 전환
- Inspector/툴 스크립트 마이그레이션
- direct mutation runtime 차단 확인

### 성공 조건
- 편집 변경이 모두 공식 경로를 탐
- 기존 주요 기능 회귀 없음
- Python이 제2의 엔진 계층이 되지 않음

---

## Phase 4: 소유권 강화 및 범용화 (1~2주)
이 단계에서는 안전성과 확장성을 높인다.

대상은 다음과 같다.

- EntityHandle + generation 검증
- stale handle 오류 처리
- 자주 쓰는 typed setter 보강
- 필요 시 `set_property(path, json)` 도입 시작

---

## Phase 5: 통합·최적화·문서화 (1주)
마지막 단계에서는 다음을 정리한다.

- 전체 회귀 테스트
- command merge/coalescing 튜닝
- 문서 업데이트
- 체크리스트 재검증

---

## 7. 일정 산정

### 권장 일정
| 구분 | 기간 |
|---|---|
| 최소 동작 가능한 핵심 슬라이스 | 4~6주 |
| 전체 마이그레이션 + 안정화 포함 | 8~12주 |

기존 초안의 7~10주는 가능은 하지만 다소 낙관적이다.  
특히 범용 property API, 바인딩 마이그레이션, stale handle 방어까지 포함하면 **8~12주**가 더 현실적이다.

---

## 8. 최종 검증 기준

## 기능 관점
- [ ] Ctrl+Z / Ctrl+Y가 실제 편집 동작과 연결됨
- [ ] 복합 작업이 하나의 undo 단위로 처리됨
- [ ] 연속 이동/드래그가 병합됨
- [ ] 예외 발생 시 rollback 보장

## 경계 관점
- [ ] Python에서 Component 직접 수정 불가
- [ ] 쓰기 작업은 Editor API만 사용
- [ ] 엔진이 데이터 소유권을 유지
- [ ] stale handle 접근이 안전하게 차단됨

## 구조 관점
- [ ] Editor API가 편집의 단일 진입점이 됨
- [ ] CommandManager가 히스토리를 일관되게 관리
- [ ] 레거시 문서/빌드 구조가 혼동을 주지 않음
- [ ] 문서와 실제 구현이 일치함

---

## 9. 아키텍처 재검증 목표

| 점검 항목 | 현재 상태 | 목표 상태 |
|---|---|---|
| Engine 공유 | ✅ 합격 | ✅ 유지 |
| 프로세스 구조 | ✅ 합격 | ✅ 유지 |
| World 관리 | ✅ 합격 | ✅ 유지 |
| 데이터 소유권 | ⚠️ 부분 합격 | ✅ 합격 |
| Reflection | ✅ 합격 | ✅ 유지 |
| Serialization | ✅ 합격 | ✅ 유지 |
| Command/Transaction | ❌ 불합격 | ✅ 합격 |
| 생산성 | ✅ 합격 | ✅ 유지 |
| Python 경계 | ⚠️ 부분 합격 | ✅ 합격 |

---

## 10. 최종 결론

이번 개선의 핵심은 기능 추가가 아니라 **편집 아키텍처의 통제권 회수**다.  
지금 문제는 Undo/Redo가 없다는 사실 자체보다, 상태 변경 경로가 분산되어 있다는 데 있다.

따라서 최종 목표는 아래 한 문장으로 요약된다.

> **엔진 상태를 바꾸는 모든 편집은 Editor API를 통해 Command로 기록되고, 필요 시 Transaction으로 묶이며, Python은 그 경계를 넘지 못한다.**

이 원칙이 구현되면 다음 효과를 기대할 수 있다.

- Undo/Redo가 구조적으로 가능해진다
- Python과 엔진의 책임 경계가 명확해진다
- 데이터 소유권과 수명주기가 안정된다
- 아키텍처 문서와 실제 구현의 정합성이 회복된다
- 장기 유지보수 비용이 크게 낮아진다

---



















차세대 게임 엔진 편집 아키텍처 개선 계획 검증 및 기술 정밀 보고서1. 아키텍처 진단 및 재정렬의 당위성제시된 '최종 아키텍처 개선 계획서'는 엔진 시스템 코어의 건전성에도 불구하고 에디터 서브시스템에서 발생하고 있는 아키텍처 정합성 파탄의 근본 원인을 정확히 포착하고 있다. 현재 아키텍처 검증 체크리스트 평가에서 67%(6/9 항목 합격)의 달성률을 보이고 있으나, 미해결 상태로 분류된 Command/Transaction, Python 경계, 데이터 소유권 영역은 개별적인 단일 결함이 아니라 상호의존적으로 연결된 구조적 결함에 해당한다.엔진 상태 변형이 Editor API → Command → CommandManager라는 통제된 파이프라인을 우회할 때, 시스템 내부에서는 세 가지 주요 문제가 동시 다발적으로 발생한다. 스크립트 레이어나 UI 컴포넌트가 ECS(Entity Component System) 내부 메모리에 직접 접근하여 값을 수정(Direct Mutation)하는 순간, 해당 변경 사항은 명령 객체로 캡슐화되지 못하여 Undo/Redo 스택에서 탈락한다. 이는 에디터의 작업 이력 추적을 불가능하게 만들며, 스크립트 수준에서 변경된 객체 수명주기와 엔진 내부 Registry의 실제 소유권 상태 간에 불일치를 야기하여 Stale Handle 및 메모리 누수 현상을 발생시킨다.모든 편집 행위의 진입점을 Editor API로 단일화하고, 내부 변경을 실행 및 취소가 가능한 Command 단위로 강제 결합하는 원칙은 에디터의 통제권을 회수하기 위한 필수적인 설계 재조정이다. 이러한 통제된 단일 수정 경로는 상용 게임 엔진인 Godot의 EditorUndoRedoManager나 Unity의 Undo 계층 구조에서도 에디터 상태 정합성 확보를 위해 채택하고 있는 검증된 패러다임이다.2. Command 및 Transaction 인프라의 메커니즘과 안정성 검증std::expected 기반 오류 처리와 원자적 트랜잭션 롤백기존의 전통적인 커맨드 패턴에서 사용되던 void Execute() 및 void Undo() 서명은 명령 수행 중 발생하는 오류나 런타임 예외를 상위 레이어로 전달할 수 없다는 치명적인 한계를 가진다. 이번 계획서에서 도입한 C++20 std::expected<void, EngineError> 기반 인터페이스는 함수 서명 자체에 실패 가능성을 명시하여 트랜잭션의 원자성(Atomicity)을 보장한다.단일 트랜잭션 내에 복수의 명령이 포함되어 실행되는 도중 $N$번째 명령에서 실패가 발생할 경우, 시스템은 이미 실행된 $0$부터 $N-1$까지의 명령들을 즉시 역순으로 Undo() 호출하여 트랜잭션 시작 전 상태로 완벽히 복구하는 롤백 정책을 수행한다.C++// engine/core/Transaction.cpp
#include "Transaction.h"

namespace Engine {

Transaction::Transaction(std::string name) : name_(std::move(name)) {}

void Transaction::AddCommand(std::unique_ptr<ICommand> command) {
    if (command) {
        commands_.push_back(std::move(command));
    }
}

bool Transaction::Empty() const { return commands_.empty(); }
size_t Transaction::Size() const { return commands_.size(); }

std::expected<void, EngineError> Transaction::Apply() {
    appliedCount_ = 0;
    for (size_t i = 0; i < commands_.size(); ++i) {
        auto result = commands_[i]->Apply();
        if (!result) {
            // 중간 실패 발생: 이미 적용된 커맨드를 역순으로 Rollback
            for (int j = static_cast<int>(appliedCount_) - 1; j >= 0; --j) {
                auto rollbackResult = commands_[j]->Undo();
                if (!rollbackResult) {
                    return std::unexpected(EngineError::CriticalRollbackFailure());
                }
            }
            appliedCount_ = 0;
            return std::unexpected(result.error());
        }
        appliedCount_++;
    }
    return {};
}

std::expected<void, EngineError> Transaction::Undo() {
    for (int i = static_cast<int>(appliedCount_) - 1; i >= 0; --i) {
        auto result = commands_[i]->Undo();
        if (!result) {
            return std::unexpected(result.error());
        }
    }
    appliedCount_ = 0;
    return {};
}

std::string Transaction::GetName() const { return name_; }

} // namespace Engine
연속 조작 병합(Coalescing)과 초깃값 오염 방지마우스 드래그나 기즈모 조작과 같이 연속적으로 이뤄지는 편집 행위는 매 프레임 독립된 Command를 생성하여 Undo 스택을 오염시키기 쉽다. 이를 해결하기 위해 제시된 CanMergeWith 및 MergeWith 인터페이스 방식은 메모리 효율성을 크게 향상시킨다.그러나 커맨드 병합 구현 시 자주 발생하는 심각한 아키텍처 오류는 지속적인 병합 과정에서 최초의 복원 기준값(oldValue)이 오염되는 현상이다. 뷰포트에서 객체를 드래그 이동할 때 생성되는 MoveEntityCommand는 병합 시 시작점 위치를 고정하고 최종 위치만을 업데이트해야 한다.병합 시점올바른 데이터 상태 (Initial Value 유지)오류 발생 데이터 상태 (Value Overwrite)드래그 시작 ($t=0$)oldPos: $(0, 0, 0)$, newPos: $(1, 0, 0)$oldPos: $(0, 0, 0)$, newPos: $(1, 0, 0)$드래그 진행 ($t=1$)oldPos: $(0, 0, 0)$, newPos: $(2, 0, 0)$oldPos: $(1, 0, 0)$, newPos: $(2, 0, 0)$드래그 종료 ($t=2$)oldPos: $(0, 0, 0)$, newPos: $(5, 0, 0)$oldPos: $(2, 0, 0)$, newPos: $(5, 0, 0)$Undo 실행 결과$(5, 0, 0) \rightarrow (0, 0, 0)$ 완전 복원$(5, 0, 0) \rightarrow (2, 0, 0)$ 오차 발생이와 같은 오류를 방지하기 위해 MergeWith 구현부는 입력받은 새로운 커맨드의 newPos 값만 수용하고, 자신이 보유한 oldPos는 절대로 변경하지 않도록 불변성(Immutability) 규칙을 강제해야 한다.3. ECS Handle 안정성 및 수명주기 제어 구조Create/Destroy Undo-Redo 사이클에서의 Handle Identity 보존계획서에서 P0-1의 핵심 이슈로 다룬 Create/Destroy 커맨드의 Handle 안정성 문제는 ECS 아키텍처의 식별자 재사용 메커니즘과 직결되어 있다. 일반적인 ECS Registry는 엔티티 파괴 시 해당 슬롯의 Index를 내부 Free-list에 등록하고 Generation 카운트를 1 증가시켜 식별자를 재활용한다.단일 트랜잭션 내에서 엔티티 생성 후 컴포넌트를 추가하고 위치를 이동하는 복합 작업이 수행된 경우, 이 트랜잭션을 Undo했다가 다시 Redo(Apply 재호출)할 때 단순 registry.CreateEntity()를 실행하면 새로운 Generation을 가진 Handle이 발급된다. 그 결과 트랜잭션 내 후속 커맨드들이 보유하고 있던 이전 Handle은 무효화되어 Redo 동작 전체가 실패하는 현상이 발생한다.이 문제를 해결하기 위해 제시된 ECSRegistry::RestoreEntity(EntityHandle) 방식은 저수준 메모리 슬롯을 정밀하게 제어함으로써 Handle Identity를 보존한다.C++// engine/ecs/ECSRegistry.cpp
#include "ECSRegistry.h"

namespace Engine {

std::expected<void, EngineError> ECSRegistry::RestoreEntity(EntityHandle handle) {
    uint32_t index = handle.GetIndex();
    uint32_t targetGen = handle.GetGeneration();

    if (index >= entitySlots_.size()) {
        entitySlots_.resize(index + 1);
    }

    auto& slot = entitySlots_[index];
    if (slot.isAlive) {
        return std::unexpected(EngineError::EntityAlreadyExists());
    }

    // 파괴되었던 슬롯의 Index와 Generation을 정확히 원래 상태로 복원
    slot.isAlive = true;
    slot.generation = targetGen;
    
    // Free-list 목록에서 해당 슬롯 제거
    UnlinkFromFreeList(index);

    return {};
}

} // namespace Engine
이러한 저수준 슬롯 복원 기법 외에도 상용 엔진에서는 Undo/Redo 사이클 동안 메모리 해제를 유예하는 패턴을 사용하기도 한다. Godot Engine은 add_do_reference를 통해 Undo 시 노드를 메모리에서 즉시 freed 처리하지 않고 참조 보관하다가 Redo 시 씬 트리에 재연결하는 방식을 사용하며, Unity Engine은 Undo 시 객체를 완전히 파괴하는 대신 hideFlags와 SetActive(false)를 이용하여 가비지 컬렉션을 유예하는 방식을 활용한다. 본 계획서의 RestoreEntity 방식은 ECS의 희소 집합(Sparse Set) 메커니즘과 부합하여 파괴-복원 간 메모리 파편화를 방지하는 이점을 제공한다.Stale Handle 탐지 및 안전성 확보엔진 소유권 유지 원칙에 따라 스크립트 레이어나 C++ 외부 시스템은 엔티티 포인터를 직접 소유할 수 없으며, 반드시 64비트 정수로 정렬된 EntityHandle만을 다뤄야 한다. Handle은 32비트 Index와 32비트 Generation 카운터로 구성된다.C++// engine/ecs/EntityHandle.h
#pragma once
#include <cstdint>

namespace Engine {

struct EntityHandle {
    uint32_t index{0xFFFFFFFF};
    uint32_t generation{0};

    bool IsValid() const { return index != 0xFFFFFFFF; }
    
    uint64_t ToUInt64() const {
        return (static_cast<uint64_t>(generation) << 32) | index;
    }

    static EntityHandle FromUInt64(uint64_t val) {
        return EntityHandle{
            static_cast<uint32_t>(val & 0xFFFFFFFF),
            static_cast<uint32_t>(val >> 32)
        };
    }
};

} // namespace Engine
Registry가 Handle의 유효성을 검사할 때, 현재 슬롯의 Generation과 Handle이 보관하고 있는 Generation을 대조하여 차이가 발생하는 경우 즉시 EngineError::StaleEntityHandle을 반환한다. 이를 통해 이미 삭제된 엔티티에 대한 접근이나 씬 로드 이후의 유효하지 않은 메모리 참조를 원천 차단한다.4. C++ 코어와 Python 바인딩 경계 격리쓰기 차단 및 Snapshot View 패턴현재 pybind11 바인딩 구조에서 def_readwrite를 사용함에 따라 Python 스크립트가 C++ 컴포넌트 멤버 변수에 직접 접근하여 값을 수정하는 문제는 아키텍처 경계를 와해시키는 핵심 요인이다.이를 해결하기 위해 바인딩 레이어에서 직접 필드 노출을 전면 폐기하고, 읽기 전용 속성(def_property_readonly) 및 값 기반 스냅샷 객체(TransformComponentView)를 도입한다.C++// engine/bindings/PythonBindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "EditorAPI.h"
#include "TransformComponentView.h"

namespace py = pybind11;
using namespace Engine;
using namespace Engine::Editor;

PYBIND11_MODULE(engine_py, m) {
    py::class_<EntityHandle>(m, "EntityHandle")
        .def(py::init<>())
        .def("is_valid", &EntityHandle::IsValid);

    // 쓰기 필드를 제거하고 Snapshot 조회 전용 View 클래스 제공
    py::class_<TransformComponentView>(m, "TransformComponentView")
        .def_property_readonly("position", &TransformComponentView::GetPosition)
        .def_property_readonly("rotation", &TransformComponentView::GetRotation)
        .def_property_readonly("scale", &TransformComponentView::GetScale);

    // 상태 변경은 오직 EditorAPI의 Typed 메서드로만 제한
    py::class_<EditorAPI>(m, "EditorAPI")
        .def_static("get_instance", &EditorAPI::GetInstance, py::return_value_policy::reference)
        .def("begin_transaction", &EditorAPI::BeginTransaction)
        .def("commit_transaction", &EditorAPI::CommitTransaction)
        .def("cancel_transaction", &EditorAPI::CancelTransaction)
        .def("move_entity", [](EditorAPI& self, EntityHandle e, const Vec3& pos) {
            UnwrapOrThrow(self.MoveEntity(e, pos));
        })
        .def("rotate_entity", [](EditorAPI& self, EntityHandle e, const Vec3& rot) {
            UnwrapOrThrow(self.RotateEntity(e, rot));
        })
        .def("scale_entity", [](EditorAPI& self, EntityHandle e, const Vec3& scale) {
            UnwrapOrThrow(self.ScaleEntity(e, scale));
        })
        .def("create_entity", [](EditorAPI& self, const std::string& name) {
            return UnwrapOrThrow(self.CreateEntity(name));
        })
        .def("destroy_entity", [](EditorAPI& self, EntityHandle e) {
            UnwrapOrThrow(self.DestroyEntity(e));
        });

    py::register_exception<EngineException>(m, "EngineError");
}

// std::expected 래퍼: 실패 시 Python 예외 변환
template<typename T>
T UnwrapOrThrow(std::expected<T, EngineError> result) {
    if (!result) throw EngineException(result.error());
    if constexpr (!std::is_void_v<T>) return *result;
}
이 구조가 적용되면 Python 환경에서 entity.transform.position.x = 100과 같은 직접 수정 구문은 런타임에 AttributeError를 발생시키며 거부된다. 모든 쓰기 작업은 editor.move_entity(entity, Vec3(100, 0, 0))와 같이 명시적인 Editor API 통로로 전달된다.GIL 제어 및 메인 스레드 DispatchingPython 환경은 GIL(Global Interpreter Lock)에 의해 구동되므로, 스크립트 실행 스레드와 엔진 메인 루프 간의 동기화가 불명확할 경우 데이터 경합(Data Race)이나 Deadlock이 유발될 수 있다.따라서 백그라운드 스레드나 스크립트 실행 스레드에서 제출된 Editor API 호출은 Engine Core의 Thread-safe Command Queue에 인큐된 후, 메인 스레드 틱(Main Thread Tick)에서 일괄 수거되어 순차 실행되는 Dispatching 아키텍처를 따라야 한다.C++// engine/editor/EditorThreadDispatcher.h
#pragma once
#include <functional>
#include <queue>
#include <mutex>

namespace Engine::Editor {

class EditorThreadDispatcher {
public:
    static EditorThreadDispatcher& GetInstance();

    void EnqueueTask(std::function<void()> task) {
        std::lock_guard<std::mutex> lock(queueMutex_);
        taskQueue_.push(task);
    }

    void ProcessMainThreadTasks() {
        std::queue<std::function<void()>> currentBatch;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            std::swap(currentBatch, taskQueue_);
        }
        while (!currentBatch.empty()) {
            currentBatch.front()();
            currentBatch.pop();
        }
    }

private:
    EditorThreadDispatcher() = default;
    std::mutex queueMutex_;
    std::queue<std::function<void()>> taskQueue_;
};

} // namespace Engine::Editor
5. 상용 엔진 아키텍처 비교 및 단계적 이행 전략상용 게임 엔진 아키텍처와의 구조 비교본 계획서가 제시하는 아키텍처 방향성을 산업계 표준 엔진인 Godot 4 및 Unity Engine과 다각도로 비교 분석한 결과는 아래 표와 같다.비교 기준본 개선 계획안Godot 4 (EditorUndoRedoManager)Unity Engine (Undo System)수정 경로 단일화Editor API 진입점 통일EditorUndoRedoManager 통과Undo 클래스 API 전용상태 변경 기록 방식Explicit ICommand 객체Callbacks / Callable ActionsSerialized Property Diff / Snapshot트랜잭션 오류 처리std::expected 원자적 RollbackC++ 바인딩 부분 처리 (Manual)Group ID 및 Batch CommitUndo 시 식별자 보존RestoreEntity (Index+Gen 강제)add_do_reference (지연 해제)RegisterCreatedObjectUndo[cite: 11, 17]스크립팅 바인딩 통제Read-Only View + Editor API 전용@tool 스크립트 매니저 연결SerializedProperty 간접 수정연속 커맨드 병합CanMergeWith + 병합 세션Action GroupingEvent-based Collapse (Mouse Drag)비교 결과, 본 계획안은 C++20 표준 함수형 에러 처리 패턴(std::expected)을 결합하여, 기존 엔진들의 한계점인 트랜잭션 중간 실패 시의 상태 파편화 문제를 명확히 극복하고 있음을 알 수 있다.Typed API 우선 도입 및 Reflection 확장 전략P2 단계로 연기된 범용 Property API(SetProperty(entity, path, json)) 도입 순서는 아키텍처 안정성 확보를 위해 적절한 선택이다. Reflection 기반 속성 수정은 문자열 기반 Path Parsing, JSON 포맷 타입 체킹, 런타임 타입 캐스팅, 구조체 내부 필드 부분 수정(Nested Property Write-back) 등의 높은 기술적 복잡도를 수반한다.초기 Phase 1~3 단계에서 move_entity, rotate_entity, create_entity 등 명확한 Typed API를 선제 구축함으로써 Command 및 Transaction 기반 인프라를 신속하게 검증할 수 있다. 이후 Typed API 체계가 안정화되면, 해당 구조 위에 Reflection 메타데이터 리졸버를 결합하여 범용 Property API로 확장하는 것이 이행 리스크를 최소화하는 표준 경로다.6. 로드맵 실행 리스크 분석 및 기술적 제언실행 단계별 잠재 리스크 및 대응 전략Phase 1 (Vertical Slice) 리스크:기존 스크립트 작성 방식과의 비호환성으로 인한 빌드 파탄 가능성.대응 전략: P0 인프라 구축 단계에서는 C++ 내부 조건부 컴파일 옵션(ENABLE_DIRECT_ECS_MUTATION_DEPRECATED)을 한시적으로 제공하여 완충 기간을 확보한 후 Phase 3 진입 시 완전히 폐기한다.Phase 2 (복합 트랜잭션 및 계층 구조) 리스크:부모-자식 계층 구조(Hierarchy)를 가진 엔티티 삭제 및 복원 시, 노드 순서(Sibling Index) 손실 문제 발생 가능성.대응 전략: DestroyEntityCommand 내부 스냅샷에 단순히 엔티티 데이터뿐만 아니라 계층 구조상의 부모 Handle 및 자식 순서 Index를 함께 기록하여 Undo() 실행 시 원래 계층 위치로 복원되도록 보장한다.Phase 3 (Python 바인딩 마이그레이션) 리스크:레거시 툴 스크립트 변환 과정에서 누락된 변환 작업으로 인한 런타임 예외 발생.대응 전략: Python 측에 Context Manager 패턴을 표준으로 배포하여 스크립트 작성 시 예외가 발생하더라도 자동으로 CancelTransaction()이 수행되도록 처리한다.Python# python/engine_tools/context.py
from contextlib import contextmanager
import engine_py

@contextmanager
def transaction(name: str):
    api = engine_py.EditorAPI.get_instance()
    api.begin_transaction(name)
    try:
        yield api
        api.commit_transaction()
    except Exception as e:
        api.cancel_transaction()
        raise e
아키텍처 목표 상태 재검증계획서의 로드맵에 맞춰 재구성 작업을 완료할 경우, 최종 아키텍처 점검 항목은 다음과 같이 개선된다.점검 항목개선 전 상태목표 상태핵심 검증 수단Engine 공유✅ 합격✅ 유지멀티 스레드 인스턴스 격리 유지프로세스 구조✅ 합격✅ 유지에디터 및 엔진 루프 독립 실행World 관리✅ 합격✅ 유지World 생명주기 제어 유지데이터 소유권⚠️ 부분 합격✅ 합격Handle + Generation 기반 Access 통제Reflection✅ 합격✅ 유지타입 메타데이터 시스템 유지Serialization✅ 합격✅ 유지씬 및 리소스 직렬화 유지Command/Transaction❌ 불합격✅ 합격Undo/Redo 스택 연결 및 Atomic Rollback생산성✅ 합격✅ 유지에디터 도구 생산성 유지Python 경계⚠️ 부분 합격✅ 합격Read-Only View + Editor API 전용 통로7. 종합 결론 및 추천 방향본 개선 계획서는 게임 엔진의 편집 아키텍처가 직면한 근본적인 문제점을 정확히 진단하고 이를 해결하기 위한 정밀한 설계안을 담고 있다. 에디터 상태를 변경하는 모든 수단을 Editor API → Command → CommandManager 경로로 집결시키는 원칙은 데이터 정합성 회복과 Undo/Redo 인프라 구축을 위한 최선의 아키텍처적 선택이다.제안된 8~12주의 종합 실행 일정은 기술적 난이도와 안전성 검증 기간을 고려했을 때 충분히 타당하다. P0 단계에서 본 보고서가 제언한 (1) 커맨드 병합 시 oldValue 불변성 유지, (2) RestoreEntity를 이용한 Handle Identity 보존, (3) Python GIL 및 메인 스레드 Task Dispatcher의 세 가지 저수준 메커니즘을 충실히 반영하여 구현을 진행한다면, 시스템 전체의 정합성을 완전히 확보할 수 있을 것으로 판단된다.