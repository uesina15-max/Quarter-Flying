# Design Document

## Overview

에디터 커맨드 시스템은 Quarter Flying 게임 엔진의 모든 편집 변경을
`EditorAPI → Command → CommandManager` 단일 경로로 강제하여 Undo/Redo를
구조적으로 보장하는 레이어다.

기존에 이미 구현된 `ICommand`, `CommandManager`, `MoveEntityCommand`를 기반으로
다음 컴포넌트를 추가한다: `Transaction`, 나머지 Command 구현체들, `EditorAPI` 확장,
Python 바인딩 경계 강화, UUID 기반 stale handle 방어.

### 기존 구현 현황 (변경 없이 유지)

| 항목 | 상태 |
|------|------|
| `ICommand` (Apply/Undo/MergeWith) | ✅ 완료 |
| `CommandManager` (stack, merge session, depth limit) | ✅ 완료 |
| `MoveEntityCommand` (이동 + 병합) | ✅ 완료 |
| `EditorAPI::MoveEntity` | ✅ 완료 (부분) |
| Reflection (serialize/deserialize/patchField) | ✅ 완료 |
| Python 바인딩 구조 (EditorBindings, ECSBindings) | ✅ 완료 (부분) |

---

## Architecture

### 레이어 구조

```
┌─────────────────────────────────────────────────────┐
│  Python / Qt UI                                     │
│  editor.move_entity()  editor.create_entity()  ...  │
└────────────────────┬────────────────────────────────┘
                     │ (pybind11 → EngineException)
┌────────────────────▼────────────────────────────────┐
│  EditorAPI  (Engine::Editor namespace)              │
│  BeginTransaction / CommitTransaction / Cancel      │
│  MoveEntity / RotateEntity / ScaleEntity            │
│  CreateEntity / DestroyEntity                       │
│  AddComponent / RemoveComponent                     │
└────────────────────┬────────────────────────────────┘
                     │ creates & dispatches
┌────────────────────▼────────────────────────────────┐
│  ICommand 구현체들 (engine/editor/commands/)         │
│  MoveEntityCommand  RotateEntityCommand  ...        │
│  CreateEntityCommand  DestroyEntityCommand          │
│  AddComponentCommand  RemoveComponentCommand        │
│  Transaction (ICommand 조합)                        │
└────────────────────┬────────────────────────────────┘
                     │ Execute / Undo / Redo
┌────────────────────▼────────────────────────────────┐
│  CommandManager  (Engine namespace)                 │
│  undoStack_ / redoStack_ / mergeSession             │
└────────────────────┬────────────────────────────────┘
                     │ 직접 ECS 수정
┌────────────────────▼────────────────────────────────┐
│  ECSRegistry  /  ComponentRegistry (Reflection)     │
└─────────────────────────────────────────────────────┘
```

---

## Components and Interfaces

> 각 컴포넌트의 인터페이스 계약을 정의한다. 구현 세부사항은 아래 Component Design 섹션을 참조.

### ICommand (기존, 변경 없음)

```cpp
class ICommand {
public:
    virtual std::expected<void, EngineError> Apply() = 0;
    virtual std::expected<void, EngineError> Undo() = 0;
    virtual bool CanMergeWith(const ICommand&) const { return false; }
    virtual std::expected<void, EngineError> MergeWith(const ICommand&);
    virtual std::string GetName() const = 0;
};
```

### Transaction : ICommand

`Apply()` — 내부 커맨드 순차 실행, 실패 시 atomic rollback  
`Undo()` — 내부 커맨드 역순 실행  
`AddCommand(unique_ptr<ICommand>)` — 커맨드 추가  
`Empty() / Size()` — 상태 조회

### EditorAPI

단일 진입점. `Dispatch(cmd)` 내부에서 Transaction 유무를 판단해 라우팅한다.  
모든 메서드 반환 타입: `std::expected<T, EngineError>`

### CommandManager (기존, 변경 없음)

`Execute / Undo / Redo / CanUndo / CanRedo`  
`BeginMergeSession / EndMergeSession`  
`Clear / SetMaxUndoDepth`

---

## Data Models

### EntitySnapshot

삭제/복원 커맨드가 사용하는 단일 엔티티 스냅샷.

```cpp
struct EntitySnapshot {
    UUID        uuid;
    std::string name;
    nlohmann::json componentData;  // ComponentRegistry::serialize 결과
};
```

### TransformComponentView

Python에 노출되는 읽기 전용 값 복사 구조체.

```cpp
struct TransformComponentView {
    Vec3       position;
    Quaternion rotation;
    Vec3       scale;
};
```

### EngineErrorCode 추가 항목

```cpp
// Editor / Command errors (EngineError.h에 추가)
StaleEntityHandle,
DuplicateComponent,
TransactionAlreadyOpen,
NoOpenTransaction,
NotMergeable,
InternalCommandFailure,
```

---

## Component Design

### 1. Transaction

`ICommand`를 상속하는 복합 커맨드. `EditorAPI::BeginTransaction()` /
`CommitTransaction()` 사이에 쌓인 Command들을 하나의 Undo 단위로 묶는다.


**파일 위치**: `engine/core/Transaction.h`, `engine/core/Transaction.cpp`

```cpp
// engine/core/Transaction.h
namespace Engine {

class Transaction : public ICommand {
public:
    explicit Transaction(std::string name);

    void AddCommand(std::unique_ptr<ICommand> cmd);
    bool Empty() const { return commands_.empty(); }
    size_t Size() const { return commands_.size(); }

    std::expected<void, EngineError> Apply() override;
    std::expected<void, EngineError> Undo() override;
    std::string GetName() const override;

private:
    std::string name_;
    std::vector<std::unique_ptr<ICommand>> commands_;
    size_t appliedCount_ = 0;  // Apply 중 rollback 지점 추적
};

} // namespace Engine
```

**Apply 원자성 보장**:
1. `commands_[i]->Apply()` 순차 실행, `appliedCount_` 증가
2. 실패 시 `[appliedCount_-1 .. 0]` 역순 Undo 후 에러 반환
3. Undo는 전체 역순 실행

**정책**:
- 빈 Transaction은 `CommandManager`에 추가되지 않음
- 중첩 Transaction은 금지 (`EngineErrorCode::InvalidState` 반환)

---

### 2. Command 구현체들

**파일 위치**: `engine/editor/commands/`

#### 2a. Transform Commands (RotateEntityCommand, ScaleEntityCommand)

`MoveEntityCommand`와 동일한 패턴. 이미 구현된 `MoveEntityCommand`를 참조 구현으로 삼는다.

```
engine/editor/commands/
├── MoveEntityCommand.h/.cpp      ← 기존 (유지)
├── RotateEntityCommand.h/.cpp    ← 신규
└── ScaleEntityCommand.h/.cpp     ← 신규
```

각 Command는 생성자에서 old값을 캡처하고, Apply()에서 new값 적용, Undo()에서 old값 복원.
`CanMergeWith` / `MergeWith`는 동일 Entity 동일 타입 기준, **old값은 최초 값 유지** (매 병합마다 갱신 금지).

#### 2b. CreateEntityCommand

Undo→Redo 사이클에서 동일한 `Entity.id`를 복원해야 한다.
현재 `Entity`에는 generation이 없으므로, **UUID 기반 복원** 전략을 사용한다.

```cpp
class CreateEntityCommand : public ICommand {
public:
    explicit CreateEntityCommand(ECSRegistry* registry, std::string name = "");

    std::expected<void, EngineError> Apply() override;
    // 첫 호출: CreateEntity() + UUID 저장
    // 이후 호출 (Redo): CreateEntityWithUUID(savedUUID_)

    std::expected<void, EngineError> Undo() override;
    // DestroyEntity(createdEntity_)

    std::string GetName() const override { return "Create Entity"; }

private:
    ECSRegistry* registry_;
    std::string entityName_;
    Entity createdEntity_;   // Apply() 후 채워짐
    UUID savedUUID_;         // Apply() 후 채워짐, Redo에서 재사용
};
```


#### 2c. DestroyEntityCommand (서브트리 지원)

삭제 전 서브트리 전체를 스냅샷으로 저장, Undo 시 복원.
현재 ECSRegistry에 부모-자식 계층 API가 없으므로 **Phase 2에서 HierarchyComponent 도입 시 확장**.
1차 구현은 단일 Entity 삭제 + 전체 Component 스냅샷.

```cpp
struct EntitySnapshot {
    UUID uuid;
    std::string name;
    nlohmann::json componentData;  // SerializeRegistry 부분 활용
};

class DestroyEntityCommand : public ICommand {
public:
    explicit DestroyEntityCommand(ECSRegistry* registry, Entity entity);

    std::expected<void, EngineError> Apply() override;
    // 1차 Apply: snapshot_ 캡처 (ComponentRegistry::serialize 활용)
    // DestroyEntity(entity_)

    std::expected<void, EngineError> Undo() override;
    // CreateEntityWithUUID(snapshot_.uuid)
    // 각 컴포넌트 deserialize 복원

    std::string GetName() const override { return "Destroy Entity"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    EntitySnapshot snapshot_;
    bool snapshotCaptured_ = false;
};
```

#### 2d. AddComponentCommand

```cpp
class AddComponentCommand : public ICommand {
public:
    AddComponentCommand(ECSRegistry* registry, Entity entity,
                        std::string componentType);

    std::expected<void, EngineError> Apply() override;
    // ComponentRegistry::deserialize로 기본값 컴포넌트 추가

    std::expected<void, EngineError> Undo() override;
    // ComponentRegistry::patchField 없이 RemoveComponent<T> 직접 호출
    // → 타입별 분기 또는 ComponentRegistry 기반 dynamic remove (2차)

    std::string GetName() const override { return "Add Component"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    std::string componentType_;
};
```

#### 2e. RemoveComponentCommand

```cpp
class RemoveComponentCommand : public ICommand {
public:
    RemoveComponentCommand(ECSRegistry* registry, Entity entity,
                           std::string componentType);

    std::expected<void, EngineError> Apply() override;
    // ComponentRegistry::serialize로 snapshot_ 캡처 (최초 1회)
    // ComponentRegistry 기반 dynamic remove

    std::expected<void, EngineError> Undo() override;
    // ComponentRegistry::deserialize로 snapshot_ 복원

    std::string GetName() const override { return "Remove Component"; }

private:
    ECSRegistry* registry_;
    Entity entity_;
    std::string componentType_;
    nlohmann::json snapshot_;
    bool snapshotCaptured_ = false;
};
```

---

### 3. EditorAPI 확장

**파일 위치**: `engine/editor/EditorAPI.h`, `engine/editor/EditorAPI.cpp`

기존 `EditorAPI`에 다음을 추가한다.

```cpp
namespace Engine::Editor {

class EditorAPI {
public:
    static EditorAPI& GetInstance();

    void SetRegistry(ECSRegistry* registry);

    // --- Transaction lifecycle ---
    std::expected<void, EngineError> BeginTransaction(const std::string& name);
    std::expected<void, EngineError> CommitTransaction();
    std::expected<void, EngineError> CancelTransaction();
    bool IsInTransaction() const { return activeTransaction_ != nullptr; }

    // --- Typed editing APIs ---
    std::expected<void, EngineError> MoveEntity(Entity entity, const Vec3& position);    // 기존
    std::expected<void, EngineError> RotateEntity(Entity entity, const Quaternion& rotation);
    std::expected<void, EngineError> ScaleEntity(Entity entity, const Vec3& scale);

    std::expected<Entity, EngineError> CreateEntity(const std::string& name = "");
    std::expected<void, EngineError> DestroyEntity(Entity entity);

    std::expected<void, EngineError> AddComponent(Entity entity,
                                                   const std::string& componentType);
    std::expected<void, EngineError> RemoveComponent(Entity entity,
                                                      const std::string& componentType);

private:
    EditorAPI() = default;

    // Command를 Transaction에 추가하거나, 없으면 CommandManager에 직접 실행
    std::expected<void, EngineError> Dispatch(std::unique_ptr<ICommand> cmd);

    ECSRegistry* registry_ = nullptr;
    std::unique_ptr<Transaction> activeTransaction_ = nullptr;
};

} // namespace Engine::Editor
```

**Dispatch 로직**:
- `activeTransaction_` 존재 시: `AddCommand` 후 즉시 `Apply()` 실행
- 없으면: `CommandManager::Execute()`에 위임


---

### 4. UUID 기반 stale handle 방어

현재 `Entity`에는 generation 필드가 없다. `Handle<Tag>` 제네릭 타입이 generation을 지원하지만 ECS Entity에 적용되지 않았다. 요구사항의 `EntityHandle`은 다음 두 가지 방식 중 하나로 충족할 수 있다.

**선택된 설계**: Entity에 generation 추가 대신 **UUID를 persistent ID로 사용**

| 역할 | 타입 | 설명 |
|------|------|------|
| 런타임 참조 | `Entity` (id) | 메모리 내 빠른 조회 |
| 영속 참조 | `UUID` (uint64_t) | 저장/복원/Redo 복원용 |
| 유효성 검증 | `ECSRegistry::IsValid(Entity)` | id != 0 && EntityManager에 존재 |

**stale handle 시나리오 처리**:

| 시나리오 | 처리 방법 |
|----------|-----------|
| 삭제된 Entity로 Command 실행 | `Apply()` / `Undo()` 초입 `IsValid()` 검사 → `EntityNotFound` |
| World PIE Play/Stop | `World::Stop()` → `CommandManager::Clear()` 연결 (신규 훅) |
| World Scene 재로드 | `WorldManager`에 이벤트 훅 → `CommandManager::Clear()` |
| Redo 중 ID 재사용 | `CreateEntityWithUUID(savedUUID_)`로 동일 UUID 복원 보장 |

**`ECSRegistry::IsValid(Entity)` 구현 방향**:

```cpp
// ECSRegistry에 추가
bool IsValid(Entity entity) const;
// → entity.IsValid() && EntityManager에 해당 id가 살아있는지 확인
```

---

### 5. Python 바인딩 경계 강화

**파일 위치**: `engine/bindings/EditorBindings.cpp`, `engine/bindings/ECSBindings.cpp`

#### 5a. TransformComponent readonly 전환

```cpp
// ECSBindings.cpp 변경: def_readwrite → def_readonly
py::class_<TransformComponent>(m, "TransformComponent")
    .def(py::init<>())
    .def_readonly("position", &TransformComponent::position)
    .def_readonly("rotation", &TransformComponent::rotation)
    .def_readonly("scale",    &TransformComponent::scale);

// TransformComponentView 신규 노출 (값 복사 스냅샷)
struct TransformComponentView {
    Vec3       position;
    Quaternion rotation;
    Vec3       scale;
};
py::class_<TransformComponentView>(m, "TransformComponentView")
    .def_readonly("position", &TransformComponentView::position)
    .def_readonly("rotation", &TransformComponentView::rotation)
    .def_readonly("scale",    &TransformComponentView::scale);
```

#### 5b. EditorAPI 바인딩 확장

```cpp
// EditorBindings.cpp 추가
.def("begin_transaction", [](EditorAPI& self, const std::string& name) {
    UnwrapOrThrow(self.BeginTransaction(name));
})
.def("commit_transaction", [](EditorAPI& self) {
    UnwrapOrThrow(self.CommitTransaction());
})
.def("cancel_transaction", [](EditorAPI& self) {
    UnwrapOrThrow(self.CancelTransaction());
})
.def("rotate_entity", [](EditorAPI& self, Entity e, const Quaternion& rot) {
    UnwrapOrThrow(self.RotateEntity(e, rot));
})
.def("scale_entity", [](EditorAPI& self, Entity e, const Vec3& scale) {
    UnwrapOrThrow(self.ScaleEntity(e, scale));
})
.def("create_entity", [](EditorAPI& self, const std::string& name) {
    return UnwrapOrThrow(self.CreateEntity(name));
})
.def("destroy_entity", [](EditorAPI& self, Entity e) {
    UnwrapOrThrow(self.DestroyEntity(e));
})
.def("add_component", [](EditorAPI& self, Entity e, const std::string& type) {
    UnwrapOrThrow(self.AddComponent(e, type));
})
.def("remove_component", [](EditorAPI& self, Entity e, const std::string& type) {
    UnwrapOrThrow(self.RemoveComponent(e, type));
});
```

#### 5c. Python context manager (transaction)

Python 쪽에서 제공할 헬퍼:
```python
# editor/__init__.py 또는 ge_python wrapper
from contextlib import contextmanager

@contextmanager
def transaction(editor, name: str):
    editor.begin_transaction(name)
    try:
        yield
        editor.commit_transaction()
    except Exception:
        editor.cancel_transaction()
        raise
```

사용 예:
```python
with transaction(editor, "Create and Configure Entity"):
    e = editor.create_entity("NewObject")
    editor.add_component(e, "TransformComponent")
    editor.move_entity(e, Vec3(0, 0, 0))
```


---

### 6. World/CommandManager 연결 훅

PIE Play/Stop 또는 Scene 재로드 시 CommandManager를 초기화해야 한다.
현재 `World::Stop()`에 아래 한 줄을 추가한다.

```cpp
// World.cpp - Stop() 내부
void World::Stop() {
    // ... 기존 코드 ...
    CommandManager::GetInstance().Clear();  // ← 추가
    m_EditorState = EditorState::Edit;
}
```

WorldManager 레벨의 Scene 전환도 동일하게 처리:
```cpp
// WorldManager.cpp - SetActiveWorld() 내부
void WorldManager::SetActiveWorld(World* world) {
    CommandManager::GetInstance().Clear();  // ← 추가
    activeWorld = world;
}
```

---

### 7. EngineErrorCode 확장

요구사항이 요구하는 에러 코드를 기존 enum에 추가한다.

```cpp
// EngineError.h - EngineErrorCode enum 추가
// Editor / Command errors
StaleEntityHandle,
DuplicateComponent,
InvalidPropertyPath,
TransactionAlreadyOpen,
NoOpenTransaction,
NotMergeable,
InternalCommandFailure,
```

---

## Data Flow

### 단일 Command 실행 흐름

```
Python: editor.move_entity(e, Vec3(1,2,3))
  → EditorAPI::MoveEntity(entity, pos)
      → IsValid(entity) 검사
      → new MoveEntityCommand(registry_, entity, pos)
      → Dispatch(cmd)
          → CommandManager::Execute(cmd)
              → cmd->Apply()
              → merge 시도 (MergeSession 중이면)
              → undoStack_.push_back(cmd)
```

### Transaction 실행 흐름

```
editor.begin_transaction("Spawn")
  → activeTransaction_ = new Transaction("Spawn")

editor.create_entity("Box")
  → EditorAPI::CreateEntity("Box")
      → new CreateEntityCommand(...)
      → Dispatch(cmd) → activeTransaction_->AddCommand(cmd); cmd->Apply()

editor.commit_transaction()
  → CommandManager::Execute(activeTransaction_)
      → transaction->Apply() → 이미 Apply됨 (appliedCount_ 검증)
      → undoStack_.push_back(transaction)
      → activeTransaction_ = nullptr
```

### Undo/Redo 흐름

```
CommandManager::Undo()
  → cmd = undoStack_.back(); undoStack_.pop_back()
  → cmd->Undo()  (Transaction이면 역순 Undo)
  → redoStack_.push_back(cmd)

CommandManager::Redo()
  → cmd = redoStack_.back(); redoStack_.pop_back()
  → cmd->Apply()  (CreateEntityCommand: CreateEntityWithUUID 경로)
  → undoStack_.push_back(cmd)
```

---

## File Structure

```
engine/
├── core/
│   ├── ICommand.h                    ← 기존 (유지)
│   ├── CommandManager.h/.cpp         ← 기존 (유지)
│   ├── EngineError.h                 ← 에러 코드 추가
│   ├── Transaction.h                 ← 신규
│   └── Transaction.cpp               ← 신규
├── editor/
│   ├── EditorAPI.h                   ← 확장
│   ├── EditorAPI.cpp                 ← 확장
│   └── commands/
│       ├── MoveEntityCommand.h/.cpp  ← 기존 (유지)
│       ├── RotateEntityCommand.h     ← 신규
│       ├── RotateEntityCommand.cpp   ← 신규
│       ├── ScaleEntityCommand.h      ← 신규
│       ├── ScaleEntityCommand.cpp    ← 신규
│       ├── CreateEntityCommand.h     ← 신규
│       ├── CreateEntityCommand.cpp   ← 신규
│       ├── DestroyEntityCommand.h    ← 신규
│       ├── DestroyEntityCommand.cpp  ← 신규
│       ├── AddComponentCommand.h     ← 신규
│       ├── AddComponentCommand.cpp   ← 신규
│       ├── RemoveComponentCommand.h  ← 신규
│       └── RemoveComponentCommand.cpp← 신규
├── ecs/
│   └── ECSRegistry.h/.cpp            ← IsValid(Entity) 추가
├── bindings/
│   ├── EditorBindings.cpp            ← 확장 (Transaction API + 신규 메서드)
│   └── ECSBindings.cpp               ← 수정 (readonly + TransformComponentView)
└── CMakeLists.txt                    ← EDITOR_SOURCES에 신규 파일 추가
```

---

## Requirements Traceability

| 요구사항 | 설계 컴포넌트 |
|----------|--------------|
| Req 1: Transaction 시스템 | `Transaction`, `EditorAPI::BeginTransaction/Commit/Cancel` |
| Req 2: Transform Command | `RotateEntityCommand`, `ScaleEntityCommand` (+ 기존 `MoveEntityCommand`) |
| Req 3: Entity 생성·삭제 | `CreateEntityCommand` (UUID 복원), `DestroyEntityCommand` (스냅샷) |
| Req 4: Component 추가·제거 | `AddComponentCommand`, `RemoveComponentCommand` (Reflection 기반) |
| Req 5: EditorAPI 레이어 | `EditorAPI` 확장, `Dispatch()` 내부 라우팅 |
| Req 6: Python 바인딩 경계 | `ECSBindings` readonly 전환, `EditorBindings` 확장, context manager |
| Req 7: EntityHandle 검증 | `ECSRegistry::IsValid()`, UUID 기반 Redo 복원, World 훅 |
| Req 8: Command 병합 정책 | `CommandManager` 기존 MergeSession + Transform Command `CanMergeWith` |
| Req 9: 오류 처리 | `std::expected` 전면 적용, `EngineErrorCode` 확장 |
| Req 10: Undo/Redo 안정성 | `CommandManager` 기존 스택 + `CreateEntityWithUUID` Redo 경로 |

---

## Correctness Properties

### Property 1: Undo/Redo 라운드트립
모든 Command에서 `Apply() → Undo() → Apply()` 반복 후 ECSRegistry 상태가 동일해야 한다. `CreateEntityWithUUID`로 UUID를 고정하여 보장.

**Validates: Requirements 10.1, 3.2**

### Property 2: Transaction 원자성
`Apply()` 중 실패 시 `appliedCount_` 기반 역순 Undo로 중간 상태가 남지 않는다.

**Validates: Requirements 1.3, 1.5**

### Property 3: 병합 old값 불변
`MergeWith()` 시 기존 Command의 `old*` 필드를 변경하지 않는다.

**Validates: Requirements 8.3, 2.4**

### Property 4: stale handle 안전
`IsValid(entity)` false면 `EntityNotFound`를 반환하고 ECS 상태를 변경하지 않는다.

**Validates: Requirements 7.4, 9.6**

### Property 5: Command Apply 실패 시 스택 오염 없음
`CommandManager::Execute`는 Apply 실패 Command를 undoStack에 추가하지 않는다.

**Validates: Requirements 9.6, 10.1**

---

## Error Handling

| 상황 | 에러 코드 | 처리 주체 |
|------|-----------|-----------|
| null Command 전달 | `InvalidParameter` | `CommandManager::Execute` |
| Undo 스택 비어 있음 | `InvalidState` | `CommandManager::Undo` |
| Redo 스택 비어 있음 | `InvalidState` | `CommandManager::Redo` |
| 유효하지 않은 Entity | `EntityNotFound` | 각 Command의 Apply/Undo |
| TransformComponent 없음 | `ComponentNotFound` | Transform Commands |
| 이미 Transaction 활성 | `TransactionAlreadyOpen` | `EditorAPI::BeginTransaction` |
| Transaction 없는데 Commit/Cancel | `NoOpenTransaction` | `EditorAPI::CommitTransaction/Cancel` |
| 중복 Component 추가 | `DuplicateComponent` | `AddComponentCommand::Apply` |
| Component 없는데 제거 | `ComponentNotFound` | `RemoveComponentCommand::Apply` |
| Python에서 엔진 오류 | `EngineException` | `UnwrapOrThrow` 래퍼 |

모든 Command 및 EditorAPI 메서드는 `std::expected<T, EngineError>`를 반환한다.
Python 바인딩 레이어의 `UnwrapOrThrow`가 이를 pybind11 예외로 변환한다.

---

## Testing Strategy

### Phase 1 검증 (수직 슬라이스)

- `MoveEntityCommand` Apply/Undo 왕복 테스트 (기존 패턴 확인)
- `CommandManager` MergeSession + 병합 후 old값 불변 확인
- `EditorAPI::MoveEntity` → Command 생성 → 스택 기록 확인

### Phase 2 검증 (Transaction + 나머지 Command)

- `Transaction` 중간 실패 rollback 테스트 (2개 커맨드 중 2번째 실패)
- `CreateEntityCommand` Undo→Redo UUID 동일성 확인
- `DestroyEntityCommand` 스냅샷 복원 후 Component 값 일치 확인
- `RemoveComponentCommand` 필드 값 완전 복원 확인
- 빈 Transaction이 undoStack에 추가되지 않음 확인

### Phase 3 검증 (Python 경계)

- Python에서 `entity.transform.position.x = 100` 시도 시 `AttributeError` 발생 확인
- Python `transaction()` context manager 예외 시 자동 rollback 확인
- `EngineException`이 Python 레벨에서 정상 catch 가능한지 확인

### 공통 검증

- stale Entity 접근 시 `EntityNotFound` 반환 (Entity 삭제 후 Command 재실행)
- `World::Stop()` 후 `CommandManager.CanUndo() == false` 확인
- `SetMaxUndoDepth(3)` 후 4번째 Command 시 가장 오래된 항목 제거 확인
