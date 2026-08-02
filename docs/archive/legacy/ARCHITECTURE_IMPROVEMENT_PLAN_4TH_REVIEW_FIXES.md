# 4차 검토 수정 가이드

## 긴급 수정 항목 (우선순위 순)

### 🔴 #2 — pybind11 예외 변환 레이어: 5.3절에 추가 필요

**문제**: pybind11 바인딩 코드가 문서에 없으며, UnwrapOrThrow 래퍼가 정의되지 않음

**수정할 위치**: 5.3절 (Python 바인딩 경계 강화)

**추가할 내용**:
```cpp
// engine/bindings/PythonBindings.cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "EditorAPI.h"
#include "TransformComponentView.h"

namespace py = pybind11;
using namespace Engine;
using namespace Engine::Editor;

// std::expected 래퍼: 실패 시 Python 예외 변환
template<typename T>
T UnwrapOrThrow(std::expected<T, EngineError> result) {
    if (!result) throw EngineException(result.error());
    if constexpr (!std::is_void_v<T>) return *result;
}

PYBIND11_MODULE(engine_py, m) {
    py::register_exception<EngineException>(m, "EngineError");

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
}
```

**검증 기준 추가**:
- [ ] Python에서 엔진 오류가 실제 예외로 발생하는가 (std::expected → EngineException 변환)

---

### 🔴 #3 — Destroy 서브트리 지원: 컴파일 버그 수정

**문제**: 
- `entity` 매개변수가 Apply()에서 참조되지 않음
- `registry` vs `registry_` 일관성 부족
- `componentSnapshot`이 단수형이어야 함

**수정할 코드**:
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

struct SubtreeNodeSnapshot {
    EntityHandle handle;
    EntityHandle parentHandle;
    int siblingIndex;
    std::vector<SerializedComponent> componentSnapshots; // 엔티티는 여러 컴포넌트를 가질 수 있음
};
```

---

### 🟠 #5 — Redo 스택 클리어: 검증 기준에 추가

**수정할 위치**: 5.1절 검증 기준

**추가할 항목**:
- [ ] Undo 이후 새 Command를 Execute하면 redoStack_이 즉시 비워짐
- [ ] 비워진 redoStack_의 Command 객체들이 리소스 누수 없이 파괴됨

---

### 🟠 #6 — World/Scene reload 훅: 통합 지점 명시

**수정할 위치**: Phase 2~4 계획에 통합 지점 추가

**추가할 내용**:
"World/Scene 로드 완료 시점(WorldManager::OnSceneLoaded)에서 CommandManager::Clear()를 호출하여 Undo/Redo 메뉴를 비활성화하는 훅을 추가. 이는 Phase 2(World 관리)와 Phase 4(소유권 강화)의 통합 지점이다."

---

### ❌ #7 — Handle 재사용 정책: 명시적 정책 추가

**수정할 위치**: 5.4절 (데이터 소유권 명확화)

**추가할 내용**:
```markdown
### Handle 재사용 시 Redo 정책
삭제된 슬롯이 새로운 CreateEntity()에 의해 재사용되는 경우, 해당 index를 참조하던 오래된 redo 항목은 무효화된다.

**권장 정책**: 어떤 index든 새로 재사용해서 handle을 발급하는 순간, 그 index를 참조하던 이전 handle 관련 redo 스택 항목을 전부 무효화하고 버린다. Redo 가능 범위가 줄어들지만 사용자에게 혼란스러운 실패가 노출되지 않는다.
```

---

### ❌ #10 — Selection/Undo 분리: 명시적 정책 추가

**수정할 위치**: 5.3절 (Python 바인딩 경계 강화)

**추가할 내용**:
```markdown
### Selection/Undo 분리 정책
엔티티 선택/해제를 CommandManager의 Undo 스택에 포함시키지 않음. Selection은 CommandManager 밖에서 관리되는 휘발성 UI 상태이며, Undo/Redo 대상이 아님. 이는 Godot/Unity의 표준 정책과 일치. `CreateEntityCommand::Undo()` 등에서 선택 상태 복원은 불필요.
```

---

### #1 — 스레드 안전성 문구 수정

**수정할 위치**: 5.3절, 5.4절 검증 기준

**변경할 내용**:
- 변경 전: "스레드 안전성: Python GIL 하에서 엔진 메인 스레드와 동기화"
- 변경 후: "Python 재진입 호출 시 GIL 재귀 잠금이 정상 동작하는가"

---

## 검증 기준 업데이트 요약

### 5.1절 (Command/Transaction) 검증 기준 추가:
- [ ] Undo 이후 새 Command를 Execute하면 redoStack_이 즉시 비워짐
- [ ] 비워진 redoStack_의 Command 객체들이 리소스 누수 없이 파괴됨
- [ ] handle 재사용이 오래된 redo 항목과 충돌할 때의 동작 테스트

### 5.3절 (Python 바인딩) 검증 기준 수정:
- [ ] Python에서 엔진 오류가 실제 예외로 발생하는가 (std::expected → EngineException 변환)
- [ ] Python 재진입 호출 시 GIL 재귀 잠금이 정상 동작하는가

---

## 상단 요약 수정

상단 "3차 보강 검토 반영 완료" 섹션을 다음과 같이 수정:

```markdown
## 4차 검토 반영 진행 중 (2026-07-31)

### 실제 반영 상태
| 항목 | 상태 | 비고 |
|---|---|---|
| 동기/비동기 계약 모순 해결 | ✅ 반영됨 | EditorAPI 주석에 명시 |
| pybind11 예외 변환 레이어 | ⚠️ 진행 중 | 코드 추가 필요 |
| Destroy 서브트리 지원 | ✅ 버그 수정됨 | 컴파일 오류 해결 |
| RemoveComponent 필드 스냅샷 | ✅ 반영됨 | Reflection 활용 |
| Redo 스택 클리어 시맨틱 | ⚠️ 진행 중 | 검증 기준 추가 필요 |
| World/Scene reload 훅 | ⚠️ 진행 중 | 통합 지점 명시 필요 |
| Handle 재사용 정책 | ⚠️ 진행 중 | 명시적 정책 추가 필요 |
| Merge Session 명시적 API | ✅ 반영됨 | 코드 추가됨 |
| View 스냅샷 의미론 | ✅ 반영됨 | 문서화됨 |
| Selection/Undo 분리 | ⚠️ 진행 중 | 명시적 정책 추가 필요 |
```