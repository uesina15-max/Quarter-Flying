# Requirements Document

## Introduction

이 문서는 Quarter Flying 게임 엔진의 **에디터 커맨드 시스템(Editor Command System)** 에 대한 요구사항을 정의합니다.

에디터 커맨드 시스템은 에디터에서 발생하는 모든 편집 변경(Entity 이동/회전/스케일, 생성/삭제, Component 추가/제거)을 명시적인 Command 객체로 캡슐화하여 Undo/Redo를 보장하는 시스템입니다. 모든 편집 변경은 반드시 `EditorAPI → Command → CommandManager` 경로를 통해야 하며, Python 스크립트는 엔진이 소유한 Entity/Component에 직접 접근하는 대신 핸들(Handle)과 스냅샷(Snapshot)만을 다룹니다.

기존에 이미 구현된 `ICommand` 인터페이스와 `CommandManager` 위에 아래의 컴포넌트들을 추가로 구현합니다:

- **Transaction 시스템**: 여러 Command를 원자적(atomic) 하나의 Undo 단위로 묶는 기능
- **구체적 Command 구현체들**: Transform/Entity/Component 조작을 위한 typed Command 클래스들
- **EditorAPI 레이어**: Python 및 에디터 UI가 편집 변경을 요청하는 단일 진입점
- **Python 바인딩 경계 강화**: 직접 수정 차단 및 안전한 읽기 전용 뷰(View) 제공
- **EntityHandle generation 검증**: stale 핸들 방어 및 핸들 재사용 무효화 정책

---

## Glossary

- **ICommand**: Apply/Undo 인터페이스를 정의하는 추상 기반 클래스 (이미 구현됨)
- **CommandManager**: undoStack/redoStack을 관리하며 Command를 실행·취소·재실행하는 싱글턴 (이미 구현됨)
- **Transaction**: 여러 Command를 하나의 Undo 단위로 묶는 복합 Command. `ICommand`를 상속
- **EditorAPI**: 에디터의 모든 편집 변경 요청을 받아 적절한 Command를 생성·실행하는 파사드(Facade) 싱글턴
- **EntityHandle**: `{ EntityID id; uint32_t generation; }` 구조로 Entity를 식별하는 핸들. generation으로 stale 여부를 검증
- **EntityID**: ECS 런타임 내부에서 Entity를 식별하는 uint32_t 값 (이미 구현됨)
- **ECSRegistry**: Entity 및 Component의 소유권을 관리하는 클래스 (이미 구현됨)
- **World**: ECSRegistry와 System들을 소유하는 ECS 최상위 컨테이너 (이미 구현됨)
- **MergeSession**: CommandManager의 BeginMergeSession/EndMergeSession 사이의 구간. 이 구간 내에서만 Command 병합이 허용됨
- **ComponentSnapshot**: Reflection 시스템을 통해 직렬화된 Component의 전체 필드 복사본 (nlohmann::json 기반)
- **SubtreeSnapshot**: DestroyEntityCommand가 삭제 전에 저장하는 Entity 서브트리(부모-자식 계층) 전체의 스냅샷
- **TransformComponentView**: Python 바인딩에 노출되는 TransformComponent의 읽기 전용 스냅샷 구조체
- **stale 핸들**: generation이 현재 ECSRegistry의 해당 EntityID generation과 불일치하는 EntityHandle
- **EngineError**: `EngineErrorCode`와 메시지, 컴포넌트 이름을 담는 에러 구조체 (이미 구현됨)

---

## Requirements

### Requirement 1: Transaction 시스템

**User Story:** 에디터 개발자로서, 여러 편집 작업을 하나의 Undo 단위로 묶고 싶다. 그렇게 해야 복잡한 작업(예: 여러 Entity를 동시에 이동)을 한 번의 Ctrl+Z로 되돌릴 수 있기 때문이다.

#### 수용 기준

1. THE **Transaction** SHALL `ICommand`를 상속하여 `Apply()`, `Undo()`, `GetName()`을 구현한다.
2. WHEN **EditorAPI**가 `BeginTransaction()`을 호출한 후 하나 이상의 Command가 실행되고 `CommitTransaction()`이 호출되면, THE **CommandManager** SHALL 해당 Command들을 하나의 Transaction 단위로 undoStack에 추가한다.
3. WHEN **Transaction**의 `Apply()` 실행 중 포함된 Command 중 하나라도 실패하면, THE **Transaction** SHALL 이미 성공한 Command들을 역순으로 `Undo()` 호출하여 원자적 롤백(atomic rollback)을 수행하고 `EngineError`를 반환한다.
4. WHEN **EditorAPI**가 `CommitTransaction()`을 호출했으나 Transaction에 포함된 Command가 하나도 없으면, THE **CommandManager** SHALL 해당 Transaction을 undoStack에 추가하지 않는다.
5. WHEN **EditorAPI**가 `CancelTransaction()`을 호출하면, THE **Transaction** SHALL 이미 실행된 Command들을 역순으로 `Undo()` 호출하여 롤백하고, 해당 Transaction을 undoStack에 추가하지 않는다.
6. IF **Transaction**이 이미 활성화된 상태에서 `BeginTransaction()`이 다시 호출되면, THEN THE **EditorAPI** SHALL `EngineErrorCode::InvalidState` 에러를 반환하고 중첩 Transaction을 허용하지 않는다.
7. THE **Transaction** SHALL `GetName()`을 통해 포함된 Command 이름들을 요약한 문자열을 반환한다.

---

### Requirement 2: Transform Command 구현체

**User Story:** 에디터 개발자로서, Entity의 위치·회전·스케일 변경을 Undo/Redo 가능한 Command로 캡슐화하고 싶다.

#### 수용 기준

1. THE **MoveEntityCommand** SHALL `ICommand`를 상속하고, `Apply()` 시 대상 Entity의 `TransformComponent::position`을 새 위치로 설정하며, `Undo()` 시 이전 위치로 복원한다.
2. THE **RotateEntityCommand** SHALL `ICommand`를 상속하고, `Apply()` 시 대상 Entity의 `TransformComponent::rotation`을 새 회전값으로 설정하며, `Undo()` 시 이전 회전값으로 복원한다.
3. THE **ScaleEntityCommand** SHALL `ICommand`를 상속하고, `Apply()` 시 대상 Entity의 `TransformComponent::scale`을 새 스케일로 설정하며, `Undo()` 시 이전 스케일로 복원한다.
4. WHEN MergeSession이 활성화된 상태에서 동일 Entity에 대한 동일 타입 Transform Command가 연속 실행되면, THE **CommandManager** SHALL `CanMergeWith()`와 `MergeWith()`를 통해 두 Command를 병합하고, 병합 결과 Command는 최초 실행 전의 값(oldPosition, oldRotation, oldScale)을 보존한다.
5. IF Transform Command의 대상 EntityHandle이 stale 핸들인 경우, THEN THE **MoveEntityCommand/RotateEntityCommand/ScaleEntityCommand** SHALL `EngineErrorCode::EntityNotFound` 에러를 반환한다.
6. WHILE MergeSession이 비활성화 상태일 때, THE **CommandManager** SHALL Transform Command 간 병합을 수행하지 않는다.

---

### Requirement 3: Entity 생성·삭제 Command 구현체

**User Story:** 에디터 개발자로서, Entity 생성 및 삭제를 Undo/Redo 가능한 Command로 캡슐화하고 싶다. 특히 Undo 후 Redo 시에 동일한 EntityHandle이 복원되어야 씬 참조가 깨지지 않기 때문이다.

#### 수용 기준

1. THE **CreateEntityCommand** SHALL `ICommand`를 상속하고, `Apply()` 시 `ECSRegistry`에 새 Entity를 생성하며 해당 EntityHandle을 내부에 저장한다.
2. WHEN **CreateEntityCommand**의 `Apply()`가 두 번째 이후로 호출되면(Redo), THE **CreateEntityCommand** SHALL 첫 번째 `Apply()` 시 생성된 것과 동일한 EntityHandle(`id`와 `generation` 모두 일치)을 가진 Entity를 복원한다.
3. THE **CreateEntityCommand** SHALL `Undo()` 시 해당 Entity를 `ECSRegistry`에서 삭제한다.
4. THE **DestroyEntityCommand** SHALL `ICommand`를 상속하고, `Apply()` 호출 전에 대상 Entity와 해당 Entity의 모든 자식 Entity(서브트리 전체)의 스냅샷(`SubtreeSnapshot`)을 리플렉션 시스템을 통해 저장한다.
5. WHEN **DestroyEntityCommand**의 `Apply()`가 호출되면, THE **DestroyEntityCommand** SHALL 서브트리의 리프(leaf) Entity부터 루트(root) Entity 순으로 삭제한다.
6. WHEN **DestroyEntityCommand**의 `Undo()`가 호출되면, THE **DestroyEntityCommand** SHALL 저장된 `SubtreeSnapshot`을 이용해 루트 Entity부터 리프 Entity 순으로 Entity와 Component들을 복원한다.
7. IF **DestroyEntityCommand**의 대상 EntityHandle이 유효하지 않은 경우, THEN THE **DestroyEntityCommand** SHALL `EngineErrorCode::EntityNotFound` 에러를 반환한다.

---

### Requirement 4: Component 추가·제거 Command 구현체

**User Story:** 에디터 개발자로서, Entity에 Component를 추가하거나 제거하는 작업을 Undo/Redo 가능한 Command로 캡슐화하고 싶다.

#### 수용 기준

1. THE **AddComponentCommand** SHALL `ICommand`를 상속하고, `Apply()` 시 지정된 Entity에 지정된 타입의 Component를 기본값으로 추가하며, `Undo()` 시 해당 Component를 제거한다.
2. THE **RemoveComponentCommand** SHALL `ICommand`를 상속하고, `Apply()` 호출 전에 리플렉션 시스템(`ComponentRegistry::serialize`)을 통해 대상 Component의 전체 필드 스냅샷(`ComponentSnapshot`)을 저장한다.
3. WHEN **RemoveComponentCommand**의 `Apply()`가 호출되면, THE **RemoveComponentCommand** SHALL 대상 Component를 `ECSRegistry`에서 제거한다.
4. WHEN **RemoveComponentCommand**의 `Undo()`가 호출되면, THE **RemoveComponentCommand** SHALL 저장된 `ComponentSnapshot`을 이용해 리플렉션 시스템(`ComponentRegistry::deserialize`)으로 Component와 모든 필드를 복원한다.
5. IF **AddComponentCommand**의 대상 Entity에 이미 동일 타입의 Component가 존재하는 경우, THEN THE **AddComponentCommand** SHALL `EngineErrorCode::InvalidState` 에러를 반환한다.
6. IF **RemoveComponentCommand**의 대상 Entity에 해당 타입의 Component가 존재하지 않는 경우, THEN THE **RemoveComponentCommand** SHALL `EngineErrorCode::ComponentNotFound` 에러를 반환한다.

---

### Requirement 5: EditorAPI 레이어

**User Story:** 에디터 UI 개발자 및 Python 스크립트 작성자로서, 편집 변경을 위한 단일하고 일관된 진입점을 원한다. 그렇게 해야 모든 편집 변경이 자동으로 Undo/Redo 스택에 기록되기 때문이다.

#### 수용 기준

1. THE **EditorAPI** SHALL 싱글턴으로 `GetInstance()` 정적 메서드를 통해 접근 가능하다.
2. THE **EditorAPI** SHALL `BeginTransaction()`, `CommitTransaction()`, `CancelTransaction()` 메서드를 제공하여 여러 편집 작업을 하나의 Undo 단위로 묶을 수 있게 한다.
3. THE **EditorAPI** SHALL `MoveEntity(EntityHandle, Vec3 newPosition)`, `RotateEntity(EntityHandle, Quaternion newRotation)`, `ScaleEntity(EntityHandle, Vec3 newScale)` 메서드를 제공한다.
4. THE **EditorAPI** SHALL `CreateEntity(optional<string> name)`, `DestroyEntity(EntityHandle)` 메서드를 제공한다.
5. THE **EditorAPI** SHALL `AddComponent(EntityHandle, StringHash componentType)`, `RemoveComponent(EntityHandle, StringHash componentType)` 메서드를 제공한다.
6. THE **EditorAPI** SHALL 모든 메서드의 반환 타입을 `std::expected<T, EngineError>`로 정의하여 실패를 명시적으로 표현한다.
7. WHEN **EditorAPI**의 편집 메서드가 호출되면, THE **EditorAPI** SHALL 해당하는 Command 객체를 생성하고 `CommandManager::Execute()`를 통해 실행한다. 직접적인 `ECSRegistry` 수정은 수행하지 않는다.
8. WHEN **EditorAPI**가 활성 Transaction 내에서 편집 메서드를 호출하면, THE **EditorAPI** SHALL 해당 Command를 현재 Transaction에 추가하고 즉시 `Apply()`를 실행한다.
9. THE **EditorAPI** SHALL `SetProperty` 메서드를 2차 구현 범위로 예약하며, 1차 구현에서는 제공하지 않는다.

---

### Requirement 6: Python 바인딩 경계

**User Story:** 게임 스크립트 개발자로서, Python에서 Entity와 Component에 안전하게 접근하고 싶다. 직접 수정은 차단되어야 엔진 상태의 일관성이 보장되고 Undo/Redo가 깨지지 않기 때문이다.

#### 수용 기준

1. THE **Python 바인딩 레이어** SHALL `TransformComponent`의 필드를 pybind11 `def_readwrite` 대신 `def_readonly`로 노출하여 Python에서의 직접 수정을 차단한다.
2. THE **Python 바인딩 레이어** SHALL `TransformComponentView`(읽기 전용 스냅샷 구조체)를 Python에 노출하며, 이 구조체는 `position`, `rotation`, `scale` 필드를 값 복사(value copy)로 포함한다.
3. WHEN Python 코드가 Entity Transform을 수정하려 할 때, THE **Python 바인딩 레이어** SHALL `EditorAPI`의 `move_entity`, `rotate_entity`, `scale_entity` 메서드를 통해서만 수정 가능하도록 한다.
4. WHEN **EditorAPI**의 C++ 메서드가 `EngineError`를 반환하면, THE **Python 바인딩 레이어** SHALL 해당 에러를 pybind11 예외(`EngineException`)로 변환하여 Python 예외로 전달한다.
5. THE **Python 바인딩 레이어** SHALL Selection(선택) 상태를 Undo 스택과 분리하여, Selection 변경은 `CommandManager`의 undoStack/redoStack에 포함되지 않는다.
6. THE **Python 바인딩 레이어** SHALL Entity의 `EntityHandle`과 읽기 전용 Component 스냅샷만 Python에 노출하며, 내부 `Entity*` 포인터나 Component 포인터는 노출하지 않는다.

---

### Requirement 7: EntityHandle generation 검증

**User Story:** 엔진 개발자로서, Undo/Redo 스택에 저장된 EntityHandle이 삭제 후 재생성된 Entity를 가리키는 상황(stale handle)을 방지하고 싶다. 그렇게 해야 잘못된 Entity에 Command가 적용되는 버그를 막을 수 있기 때문이다.

#### 수용 기준

1. THE **EntityHandle** SHALL `EntityID id`와 `uint32_t generation` 두 필드를 가지는 구조체로 정의된다.
2. THE **ECSRegistry** SHALL `RestoreEntity(EntityHandle)` 메서드를 제공하여, CreateEntityCommand의 Redo 시 지정된 `id`와 `generation`으로 Entity를 복원할 수 있게 한다.
3. THE **ECSRegistry** SHALL `IsValid(EntityHandle)` 메서드를 제공하여, 현재 레지스트리에서 해당 `id`의 Entity가 존재하고 `generation`이 일치하는지 확인한다.
4. WHEN 임의의 Command가 `Apply()` 또는 `Undo()`를 실행하기 전에, THE **Command** SHALL `ECSRegistry::IsValid(handle)`를 호출하여 stale 핸들 여부를 검증하고, stale 핸들인 경우 `EngineErrorCode::EntityNotFound` 에러를 반환한다.
5. WHEN 새 Entity가 이미 삭제된 Entity와 동일한 `EntityID`를 재사용하면, THE **ECSRegistry** SHALL 해당 `EntityID`의 `generation`을 증가시켜, 이전 generation을 가진 EntityHandle이 `IsValid()` 검사를 통과하지 못하게 한다.
6. WHEN `EntityID`가 재사용되어 기존 Redo 스택의 특정 Command가 무효화되면, THE **CommandManager** SHALL 해당 Command 이후의 Redo 스택 항목 전체를 제거한다.
7. WHEN **World** 또는 **Scene**이 재로드되면, THE **CommandManager** SHALL `Clear()`를 호출하여 모든 Undo/Redo 스택을 초기화한다.

---

### Requirement 8: Command 병합 정책

**User Story:** 에디터 사용자로서, 드래그로 Entity를 이동하는 동안 매 프레임 Command가 스택에 쌓이지 않고 하나로 합쳐지길 원한다. 그렇게 해야 Undo 시 최초 이동 전 상태로 한 번에 돌아갈 수 있기 때문이다.

#### 수용 기준

1. WHILE MergeSession이 활성화된 상태에서, THE **CommandManager** SHALL `Execute()` 시 undoStack 상단의 Command와 `CanMergeWith()`가 `true`를 반환하면 `MergeWith()`를 호출하여 병합한다.
2. WHILE MergeSession이 비활성화된 상태에서, THE **CommandManager** SHALL Command 병합을 시도하지 않고 새 Command를 undoStack에 추가한다.
3. WHEN 두 Transform Command가 병합될 때, THE **병합 결과 Command** SHALL 첫 번째 Command의 `old` 값(oldPosition, oldRotation, oldScale)과 두 번째 Command의 `new` 값을 유지하여, Undo 시 최초 이동 전 상태로 복원한다.
4. THE **MoveEntityCommand**, **RotateEntityCommand**, **ScaleEntityCommand** SHALL 각각 동일 Entity를 대상으로 하는 동일 타입 Command에 대해서만 `CanMergeWith()`가 `true`를 반환한다.

---

### Requirement 9: 오류 처리 및 실패 표현

**User Story:** 엔진 개발자로서, 모든 Command 및 EditorAPI 호출의 성공·실패가 명시적으로 표현되길 원한다. 그렇게 해야 호출자가 에러를 무시하는 상황을 컴파일 타임에 방지할 수 있기 때문이다.

#### 수용 기준

1. THE **모든 Command 구현체** SHALL `Apply()`와 `Undo()`의 반환 타입을 `std::expected<void, EngineError>`로 정의한다.
2. THE **EditorAPI** SHALL 모든 편집 메서드의 반환 타입을 `std::expected<T, EngineError>`로 정의한다.
3. WHEN `CommandManager::Execute()`에 nullptr Command가 전달되면, THE **CommandManager** SHALL `EngineErrorCode::InvalidParameter` 에러를 반환한다.
4. WHEN `CommandManager::Undo()`가 호출됐으나 undoStack이 비어 있으면, THE **CommandManager** SHALL `EngineErrorCode::InvalidState` 에러를 반환한다.
5. WHEN `CommandManager::Redo()`가 호출됐으나 redoStack이 비어 있으면, THE **CommandManager** SHALL `EngineErrorCode::InvalidState` 에러를 반환한다.
6. WHEN Command의 `Apply()`가 실패하면, THE **CommandManager** SHALL 해당 Command를 undoStack에 추가하지 않는다.
7. WHEN Command의 `Undo()`가 실패하면, THE **CommandManager** SHALL 해당 Command를 undoStack에서 제거하지 않고 원래 위치로 복원한다.

---

### Requirement 10: Undo/Redo 사이클 안정성

**User Story:** 에디터 사용자로서, Undo → Redo 사이클을 반복해도 씬 상태가 일관되게 유지되길 원한다.

#### 수용 기준

1. FOR ALL Command 구현체에 대해, `Apply()` 후 `Undo()` 후 `Apply()` 를 반복 수행하면, THE **ECSRegistry** SHALL 매번 동일한 씬 상태를 가져야 한다 (라운드트립 속성).
2. WHEN `CommandManager::Undo()`가 성공하면, THE **CommandManager** SHALL 해당 Command를 redoStack으로 이동시킨다.
3. WHEN `CommandManager::Redo()`가 성공하면, THE **CommandManager** SHALL 해당 Command를 undoStack으로 이동시킨다.
4. WHEN 새 Command가 `Execute()`를 통해 실행되면, THE **CommandManager** SHALL redoStack을 비운다.
5. THE **CommandManager** SHALL `SetMaxUndoDepth(size_t depth)` 설정값을 초과하는 경우 undoStack의 가장 오래된 항목부터 제거한다.
