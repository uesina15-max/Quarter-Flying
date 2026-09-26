# Implementation Plan

## Overview

설계 문서 기반 구현 태스크 목록이다. Phase 순서를 따르되 각 태스크는 독립적으로 빌드·검증 가능하도록 구성했다.

기존 구현(`ICommand`, `CommandManager`, `MoveEntityCommand`)은 변경하지 않는다.

## Task Dependency Graph

```
Task 1 (EngineErrorCode + Transaction)
  └─▶ Task 2 (ECSRegistry::IsValid)
        └─▶ Task 3 (Rotate/ScaleEntityCommand)
        └─▶ Task 4 (Create/DestroyEntityCommand)
        └─▶ Task 5 (Add/RemoveComponentCommand)
              └─▶ Task 6 (EditorAPI 확장)
                    └─▶ Task 7 (World PIE 훅)
                    └─▶ Task 8 (Python 바인딩)
                          └─▶ Task 9 (통합 검증)
```

## Tasks

- [x] 1. EngineErrorCode 확장 및 Transaction 구현
  - `EngineError.h`의 `EngineErrorCode` enum에 Editor/Command 에러 코드 7개 추가 (`StaleEntityHandle`, `DuplicateComponent`, `InvalidPropertyPath`, `TransactionAlreadyOpen`, `NoOpenTransaction`, `NotMergeable`, `InternalCommandFailure`)
  - `engine/core/Transaction.h` 및 `Transaction.cpp` 신규 작성
    - `ICommand` 상속, `commands_` 벡터, `appliedCount_` 추적
    - `Apply()`: 순차 실행, 실패 시 `[appliedCount_-1..0]` 역순 Undo 후 에러 반환
    - `Undo()`: 전체 역순 실행
    - `GetName()`: 포함된 Command 이름 쉼표 구분 문자열 반환 (비면 빈 문자열)
    - `Empty()`, `Size()`, `AddCommand()` 구현
  - `CMakeLists.txt`의 `CORE_SOURCES`에 `core/Transaction.cpp` 추가
  - **검증**: `Transaction` Apply 중 2번째 Command 실패 시 1번째 Command가 Undo되는지 확인 (Req 1.3)
  - **검증**: 빈 Transaction은 `Empty() == true` 반환 확인 (Req 1.4)
  - _Requirements: 1.1, 1.3, 1.4, 1.5, 1.7, 9.1_

- [x] 2. ECSRegistry::IsValid 추가
  - `ECSRegistry.h`에 `bool IsValid(Entity entity) const` 선언 추가
  - `ECSRegistry.cpp`에 구현: `entity.IsValid() && EntityManager에 해당 id가 살아있는지` 확인
  - **검증**: 살아있는 Entity에 대해 `true`, DestroyEntity 후 `false` 반환 확인 (Req 7.3)
  - _Requirements: 7.3, 7.4_

- [x] 3. RotateEntityCommand, ScaleEntityCommand 구현
  - `engine/editor/commands/RotateEntityCommand.h/.cpp` 신규 작성
    - `MoveEntityCommand` 패턴 동일 적용
    - 생성자에서 `oldRotation_` 캡처 (`GetTransformComponent` 활용)
    - `Apply()`: `SetTransformRotation`, `IsValid()` 검사 포함
    - `Undo()`: `oldRotation_` 복원
    - `CanMergeWith()`: 동일 Entity 동일 타입 기준
    - `MergeWith()`: `newRotation_` 갱신, `oldRotation_` 불변 유지
  - `engine/editor/commands/ScaleEntityCommand.h/.cpp` 신규 작성 (동일 패턴)
  - `CMakeLists.txt`의 `EDITOR_SOURCES`에 두 파일 추가
  - **검증**: Apply/Undo 왕복 후 rotation/scale 원복 확인 (Req 2.2, 2.3)
  - **검증**: MergeSession 중 병합 시 `oldRotation_`/`oldScale_`가 최초 값 유지 확인 (Req 2.4, 8.3)
  - **검증**: stale Entity에 Apply 시 `EntityNotFound` 반환 (Req 2.5)
  - _Requirements: 2.2, 2.3, 2.4, 2.5, 8.3, 8.4, 9.1_

- [x] 4. CreateEntityCommand, DestroyEntityCommand 구현
  - `engine/editor/commands/CreateEntityCommand.h/.cpp` 신규 작성
    - 멤버: `registry_`, `entityName_`, `createdEntity_`, `savedUUID_`
    - `Apply()` 첫 호출: `CreateEntity()` → `GetUUID(entity)` 저장 → `SetEntityName`
    - `Apply()` 이후 호출 (Redo): `CreateEntityWithUUID(savedUUID_)` → `SetEntityName`
    - `Undo()`: `IsValid()` 검사 후 `DestroyEntity(createdEntity_)`
  - `engine/editor/commands/DestroyEntityCommand.h/.cpp` 신규 작성
    - `EntitySnapshot { UUID uuid; string name; nlohmann::json componentData; }` 정의
    - `Apply()` 최초 호출: `ComponentRegistry::serialize` 모든 등록 컴포넌트 순회하여 `snapshot_` 캡처 → `DestroyEntity`
    - `Apply()` 이후 호출: 스냅샷 재사용, `DestroyEntity`만 재실행
    - `Undo()`: `CreateEntityWithUUID(snapshot_.uuid)` → `ComponentRegistry::deserialize`로 복원 → `SetEntityName`
    - 대상 Entity가 유효하지 않으면 `EntityNotFound` 반환
  - `CMakeLists.txt`의 `EDITOR_SOURCES`에 두 파일 추가
  - **검증**: `CreateEntity` → Undo → Redo 후 동일 UUID 확인 (Req 3.2, 10.1)
  - **검증**: `DestroyEntity` Undo 후 Entity와 모든 Component가 복원되는지 확인 (Req 3.6)
  - _Requirements: 3.1, 3.2, 3.3, 3.4, 3.5, 3.6, 3.7, 7.4, 9.1_

- [x] 5. AddComponentCommand, RemoveComponentCommand 구현
  - `engine/editor/commands/AddComponentCommand.h/.cpp` 신규 작성
    - `Apply()`: `ComponentRegistry::GetComponentInfo(componentType_)` 조회 → info 없으면 `InvalidComponentType` → 이미 있으면 `DuplicateComponent` → `info->deserialize`로 기본값 추가 (`nlohmann::json{}` 전달)
    - `Undo()`: `ComponentRegistry`의 dynamic remove 활용 (타입 문자열 기반)
  - `engine/editor/commands/RemoveComponentCommand.h/.cpp` 신규 작성
    - 멤버: `snapshot_` (`nlohmann::json`), `snapshotCaptured_`
    - `Apply()`: 첫 호출에 `info->serialize`로 `snapshot_` 캡처 → dynamic remove
    - `Undo()`: `info->deserialize`로 `snapshot_` 복원
    - Component 없으면 `ComponentNotFound` 반환
  - `CMakeLists.txt`의 `EDITOR_SOURCES`에 두 파일 추가
  - **검증**: `RemoveComponent` Undo 후 모든 필드 값 일치 확인 (Req 4.4)
  - **검증**: 중복 Component 추가 시 `DuplicateComponent` 반환 (Req 4.5)
  - _Requirements: 4.1, 4.2, 4.3, 4.4, 4.5, 4.6, 9.1_

- [x] 6. EditorAPI 확장 (Transaction + 신규 편집 메서드)
  - `EditorAPI.h` 확장:
    - `BeginTransaction`, `CommitTransaction`, `CancelTransaction`, `IsInTransaction` 추가
    - `RotateEntity`, `ScaleEntity`, `CreateEntity`, `DestroyEntity`, `AddComponent`, `RemoveComponent` 추가
    - `activeTransaction_` (`unique_ptr<Transaction>`) 멤버 추가
    - `Dispatch(unique_ptr<ICommand>)` private 메서드 추가
  - `EditorAPI.cpp` 구현:
    - `BeginTransaction`: `activeTransaction_` 존재 시 `TransactionAlreadyOpen` 반환
    - `CommitTransaction`: `activeTransaction_` 없으면 `NoOpenTransaction` → `Empty()`면 버림 → `CommandManager::Execute` 위임
    - `CancelTransaction`: `activeTransaction_` 없으면 `NoOpenTransaction` → Transaction Undo 후 폐기
    - `Dispatch`: `activeTransaction_` 존재 시 `AddCommand + Apply()`, 없으면 `CommandManager::Execute`
    - 각 편집 메서드: `IsValid` 검사 → Command 생성 → `Dispatch`
  - **검증**: Transaction 내 EditAPI 호출이 단일 Undo 단위로 묶이는지 확인 (Req 1.2, 5.8)
  - **검증**: 중첩 Transaction 시 `TransactionAlreadyOpen` 반환 (Req 1.6)
  - **검증**: 예외 발생 시 `CancelTransaction`이 중간 상태를 롤백하는지 확인 (Req 1.5)
  - _Requirements: 1.2, 1.4, 1.5, 1.6, 5.1, 5.2, 5.3, 5.4, 5.5, 5.6, 5.7, 5.8, 9.2_

- [x] 7. World/WorldManager PIE 훅 연결
  - `World.cpp`의 `Stop()` 내부에 `CommandManager::GetInstance().Clear()` 추가 (레지스트리 복원 이후)
  - `WorldManager.cpp`의 `SetActiveWorld()` 내부에 `CommandManager::GetInstance().Clear()` 추가
  - **검증**: `World::Stop()` 이후 `CommandManager::CanUndo() == false` 확인 (Req 7.7)
  - **검증**: `SetActiveWorld()` 호출 이후 Undo/Redo 스택이 비워지는지 확인 (Req 7.7)
  - _Requirements: 7.7_

- [x] 8. Python 바인딩 경계 강화
  - `engine/bindings/ECSBindings.cpp` 수정:
    - `TransformComponent` 바인딩: `def_readwrite` → `def_readonly` 전환 (position, rotation, scale)
    - `position_x/y/z`, `rotation_x/y/z`, `scale_x/y/z` 개별 setter 람다 제거
    - `TransformComponentView` C++ 구조체 정의 (헤더 또는 바인딩 내 로컬)
    - `TransformComponentView` Python 노출 (`def_readonly`)
  - `engine/bindings/EditorBindings.cpp` 확장:
    - `begin_transaction`, `commit_transaction`, `cancel_transaction` 추가 (`UnwrapOrThrow` 래핑)
    - `rotate_entity`, `scale_entity`, `create_entity`, `destroy_entity`, `add_component`, `remove_component` 추가
  - Python context manager 헬퍼 작성 (editor Python 패키지 또는 `ge_python` wrapper)
  - **검증**: Python에서 `transform.position.x = 1.0` 시도 시 `AttributeError` 발생 확인 (Req 6.1)
  - **검증**: `EngineException`이 Python 레벨에서 catch 가능한지 확인 (Req 6.4)
  - **검증**: `transaction()` context manager 내 예외 발생 시 자동 `cancel_transaction` 호출 확인 (Req 1.5)
  - _Requirements: 6.1, 6.2, 6.3, 6.4, 6.5, 6.6_

- [ ] 9. 통합 검증 및 회귀 테스트
  - 기존 `MoveEntityCommand` 테스트가 그대로 통과하는지 확인
  - Transform Command 전체 (Move/Rotate/Scale) Apply/Undo/Redo 왕복 테스트
  - MergeSession 중 연속 이동 후 Undo 시 최초 위치 복원 확인 (Req 8.1, 8.3)
  - `SetMaxUndoDepth(3)` 설정 후 4번째 Command 실행 시 가장 오래된 항목 제거 확인 (Req 10.5)
  - `Execute()` 후 `redoStack_` 클리어 확인 (Req 10.4)
  - stale Entity에 Command Apply 시 `EntityNotFound` 반환, ECS 상태 변경 없음 확인 (Req 7.4)
  - Python 바인딩 전체 경로 E2E 테스트 (create → move → undo → redo)
  - _Requirements: 8.1, 8.2, 9.3, 9.4, 9.5, 9.6, 9.7, 10.2, 10.3, 10.4, 10.5_

## Notes

- Task 3, 4, 5는 Task 2 완료 후 병렬 진행 가능
- `ComponentRegistry` 기반 dynamic component remove는 Task 5에서 구현 방법을 확인 후 타입 분기 fallback이 필요하면 별도 태스크로 분리
- Python context manager는 C++ 구현(Task 8) 완료 후 Python 래퍼 파일에 작성
- 서브트리 삭제(DestroyEntityCommand 계층 지원)는 HierarchyComponent 도입 전까지 단일 Entity 스냅샷으로 제한
