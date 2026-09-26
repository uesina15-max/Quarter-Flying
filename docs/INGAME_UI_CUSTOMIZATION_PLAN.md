# 인게임 UI 커스터마이징 기능 구현 계획서

**작성일**: 2026-09-19
**목표**: 프리팹 시스템을 기반으로 인게임 UI를 커스텀 구현할 수 있는 기능을 추가하여, 실사용 가능한 수준으로 발전시키는 방안 제시

---

## 1. 프리팹 시스템 현재 상태 점검

### 1.1 완료된 기능 (Phase 1-4, 2026-08-19 완료)

프리팹 시스템은 현재 **단일 엔티티 프리팹** 기준으로 완성된 상태입니다:

| 구성 요소 | 상태 | 설명 |
|---|---|---|
| **PrefabAsset 자료구조** | ✅ 완료 | `engine/prefab/PrefabAsset.h/.cpp` - 컴포넌트 직렬화/역직렬화, 파일 I/O |
| **PrefabInstanceComponent** | ✅ 완료 | 인스턴스 식별용 메타 컴포넌트, 리플렉션 등록 완료 |
| **인스턴스화** | ✅ 완료 | `EditorAPI::InstantiatePrefab()`, Command 패턴, Undo/Redo 지원 |
| **프리팹 저작 UI** | ✅ 완료 | Prefab Browser 패널, Scene Hierarchy 우클릭 메뉴 |
| **Revert 기능** | ✅ 완료 | `RevertPrefabInstanceCommand`, Inspector 통합 |
| **실제 실행 검증** | ✅ 완료 | 에디터 실행으로 인스턴스화/렌더링/Revert 동작 확인 |

### 1.2 현재 프리팹 시스템의 한계점

**핵심 제약**: 현재 프리팹 시스템은 **엔티티 1개 = 프리팹 1개**로 설계되어 있습니다.

```
현재 가능한 프리팹:
- 단일 큐브 메시 + Transform + RenderableComponent
- 단일 파티클 시스템 (VFX Lite 구현 시)
- 단일 오디오 소스

현재 불가능한 프리팹:
- "버튼 UI 요소" (하나의 UI 요소는 여러 컴포넌트로 구성됨)
- "패널 + 버튼 + 텍스트"로 구성된 UI 패널
- 계층 구조를 가진 UI 트리
```

**구체적인 한계**:

1. **엔티티 계층 부재**: ECS에 부모-자식 관계를 표현하는 `HierarchyComponent`나 `ParentComponent`가 없음
2. **UI 전용 컴포넌트 부재**: `UIComponent`, `CanvasComponent`, `ButtonComponent`, `TextComponent` 등 UI 관련 컴포넌트가 없음
3. **UI 렌더링 파이프라인 부재**: 2D UI를 렌더링하는 전용 시스템(Orthographic Camera, UI Canvas, Text Rendering)이 없음
4. **이벤트 시스템 부재**: UI 클릭/호버/드래그 등 이벤트를 처리하는 시스템이 없음

### 1.3 프리팹 시스템의 확장성 분석

**긍정적 측면**:
- `ComponentRegistry` 리플렉션 시스템이 잘 설계되어 있어 새 컴포넌트 추가가 용이
- `PrefabAsset::ApplyToEntity()`의 Definition A 정책(컴포넌트 집합 동기화)이 UI 컴포넌트에도 그대로 적용 가능
- `SerializeOptions`로 캡처 정책을 제어할 수 있어 UI 특화 정책 추가 용이

**부정적 측면**:
- Phase 5(계층 컴포넌트 → 다중 엔티티 프리팹)가 구현되지 않으면 복합 UI 요소를 프리팹으로 만들 수 없음
- UI는 보통 "Canvas → Panel → Button → Text" 같은 깊은 계층 구조를 가지므로 단일 엔티티 프리만으로는 부족

---

## 2. 인게임 UI 커스터마이징을 위한 프리팹 시스템 활용 방안

### 2.1 단계적 접근 전략

프리팹 시스템의 현재 상태를 고려하여 **3단계 접근**을 제안합니다:

```
Phase 1: 단일 UI 요소 프리팹 (현재 기반 활용)
   ↓
Phase 2: UI 컴포넌트 시스템 구축 (신규 컴포넌트 추가)
   ↓
Phase 3: 계층형 UI 프리팹 (프리팹 Phase 5 선행)
```

### 2.2 Phase 1: 단일 UI 요소 프리팸 (현재 기반 활용)

**목표**: 현재 프리팹 시스템을 활용하여 **가장 단순한 UI 요소**를 프리팹화

**가능한 범위**:
- 단일 텍스트 라벨 (TextComponent 추가 시)
- 단일 이미지 스프라이트 (SpriteComponent 추가 시)
- 단일 배경 패널 (PanelComponent 추가 시)

**구현 방안**:

```cpp
// 1. UI 전용 컴포넌트 추가 (engine/ecs/)
struct TextComponent
{
    std::string text;
    glm::vec3 color = glm::vec3(1.0f);
    float fontSize = 12.0f;
    // ...
};

struct SpriteComponent
{
    uint32_t textureHandle = 0;
    glm::vec2 size = glm::vec2(100.0f, 100.0f);
    // ...
};

// 2. ComponentRegistry에 등록 (기존 패턴 따름)
GE_BEGIN_COMPONENT(TextComponent)
    GE_FIELD(text)
    GE_FIELD(color)
    GE_FIELD(fontSize)
GE_END_COMPONENT()

// 3. 기존 프리팹 시스템이 자동으로 지원
// - PrefabAsset::CaptureFromEntity()가 TextComponent를 직렬화
// - PrefabAsset::ApplyToEntity()가 TextComponent를 복원
// - Inspector에서 TextComponent 필드 편집 가능
```

**실사용 가능성**: 중간
- 단일 요소(예: "HP 텍스트", "코인 아이콘")는 프리팹으로 만들 수 있음
- 하지만 복합 UI(예: "HUD 패널")는 불가능

### 2.3 Phase 2: UI 컴포넌트 시스템 구축

**목표**: UI를 구성하는 기본 컴포넌트들을 완성하고, 렌더링 파이프라인 연결

**필요한 컴포넌트**:

| 컴포넌트 | 용도 | 필드 예시 |
|---|---|---|
| `CanvasComponent` | UI 렌더링 대상 화면 | `resolution`, `renderMode` (Screen Space/World Space) |
| `RectTransformComponent` | UI 요소 위치/크기/앵커 | `anchoredPosition`, `sizeDelta`, `anchorMin/Max`, `pivot` |
| `TextComponent` | 텍스트 렌더링 | `text`, `fontSize`, `color`, `alignment`, `fontAsset` |
| `ImageComponent` | 이미지/스프라이트 렌더링 | `sprite`, `color`, `preserveAspect` |
| `ButtonComponent` | 버튼 인터랙션 | `onClick`, `transition` (Color/Tint/Sprite) |
| `UIEventComponent` | 이벤트 시스템 연결 | `onClick`, `onHover`, `onDrag` |

**렌더링 파이프라인**:

```cpp
// engine/ecs/UISystem.cpp (신규)
class UISystem : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // 1. Canvas 순회 (UI 렌더링 순서 결정)
        // 2. 각 Canvas 하위의 UI 요소들을 RectTransform 기준으로 정렬
        // 3. TextComponent → TextRenderer (글리프 렌더링)
        // 4. ImageComponent → SpriteRenderer (텍스처 렌더링)
        // 5. Orthographic projection으로 렌더링
    }
};
```

**프리팹과의 통합**:

```cpp
// 기존 프리팹 시스템이 자동으로 지원
// 예: "HP Bar 프리팹" = ImageComponent (배경) + ImageComponent (체력바) + TextComponent (수치)
{
  "version": 1,
  "name": "HP Bar",
  "components": {
    "RectTransformComponent": { ... },
    "ImageComponent": { "sprite": "hp_bar_bg.png", ... },
    "ImageComponent": { "sprite": "hp_bar_fill.png", ... },  // 문제: 동일 컴포넌트 타입 중복
    "TextComponent": { "text": "100/100", ... }
  }
}
```

**문제점**: 현재 프리팹 포맷은 컴포넌트 타입당 **최대 1개**만 지원 (JSON 키가 컴포넌트 이름이므로)
- HP Bar 예시에서 `ImageComponent`가 2개 필요한데 현재 포맷으로는 불가능

### 2.4 Phase 3: 계층형 UI 프리팹 (프리팹 Phase 5 선행)

**목표**: 다중 엔티티 프리팹을 지원하여 복합 UI 요소를 프리팹화

**선행 조건**: 프리팹 시스템 Phase 5 완료
- `HierarchyComponent` 또는 `ParentComponent` 추가
- 프리팹 포맷을 `"entities": [...]` 배열로 확장
- `EntityRef` 필드 직렬화 지원

**구현 후 가능해지는 것**:

```json
{
  "version": 2,
  "name": "HUD Panel",
  "entities": [
    {
      "components": {
        "RectTransformComponent": { "anchoredPosition": [0, 0], "sizeDelta": [200, 100] },
        "ImageComponent": { "sprite": "panel_bg.png" },
        "ParentComponent": { "parent": null }  // 루트
      }
    },
    {
      "components": {
        "RectTransformComponent": { "anchoredPosition": [-80, 30], "sizeDelta": [40, 40] },
        "ImageComponent": { "sprite": "hp_icon.png" },
        "ParentComponent": { "parent": "entities[0].uuid" }  // 자식
      }
    },
    {
      "components": {
        "RectTransformComponent": { "anchoredPosition": [-30, 30], "sizeDelta": [100, 20] },
        "TextComponent": { "text": "HP: 100" },
        "ParentComponent": { "parent": "entities[0].uuid" }
      }
    }
  ]
}
```

**실사용 가능성**: 높음
- 복합 UI 요소를 완전히 프리팹화 가능
- "HUD 프리팹", "메뉴 패널 프리팹", "대화상자 프리팹" 등을 만들어 재사용

---

## 3. 인게임 UI 시스템 아키텍처 설계

### 3.1 전체 아키텍처

```
┌─────────────────────────────────────────────────────────┐
│                     Editor (PySide6)                      │
│  - UI Editor Panel (WYSIWYG UI 배치)                      │
│  - Prefab Browser (UI 프리팹 목록)                       │
│  - Inspector (UI 컴포넌트 속성 편집)                       │
└──────────────────────┬────────────────────────────────────┘
                       │ pybind11
┌──────────────────────▼────────────────────────────────────┐
│                  Engine Core (C++)                        │
│  ┌──────────────────────────────────────────────────┐   │
│  │              ECS Layer                            │   │
│  │  - Entity / Component / System                   │   │
│  │  - UI 전용 컴포넌트 (RectTransform, Text, etc.)   │   │
│  │  - HierarchyComponent (Phase 5 선행)             │   │
│  └──────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────┐   │
│  │           Prefab System                          │   │
│  │  - PrefabAsset (단일/다중 엔티티 지원)            │   │
│  │  - PrefabInstanceComponent                       │   │
│  │  - ApplyToEntity (Definition A)                  │   │
│  └──────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────┐   │
│  │           UI Systems                              │   │
│  │  - UISystem (Canvas 기반 렌더링 순서)            │   │
│  │  - TextRenderer (SDF 폰트 렌더링)                │   │
│  │  - SpriteRenderer (2D 스프라이트 렌더링)        │   │
│  │  - EventSystem (클릭/호버/드래그)               │   │
│  └──────────────────────────────────────────────────┘   │
│  ┌──────────────────────────────────────────────────┐   │
│  │           Rendering Backend                       │   │
│  │  - OpenGL (기존 RenderGraph 재활용)              │   │
│  │  - Orthographic Camera (UI 전용)                │   │
│  │  - UI Shader (Text/Sprite 전용)                  │   │
│  └──────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────┘
```

### 3.2 컴포넌트 설계

**핵심 컴포넌트 3종**:

```cpp
// 1. RectTransformComponent (모든 UI 요소 필수)
struct RectTransformComponent
{
    glm::vec2 anchoredPosition;  // 앵커 기준 위치
    glm::vec2 sizeDelta;         // 앵커 기준 크기
    glm::vec2 anchorMin;         // 부모 기준 최소 앵커 (0~1)
    glm::vec2 anchorMax;         // 부모 기준 최대 앵커 (0~1)
    glm::vec2 pivot;            // 자체 기준 피벗 (0~1)
    glm::vec2 rotation;         // 회전 (z축)
    glm::vec2 scale;            // 스케일
};

// 2. CanvasComponent (UI 렌더링 루트)
struct CanvasComponent
{
    enum class RenderMode { ScreenSpace, WorldSpace };
    RenderMode renderMode = RenderMode::ScreenSpace;
    glm::vec2 resolution;       // Canvas 해상도
    int sortOrder = 0;          // 렌더링 순서
};

// 3. UI 전용 베이스 컴포넌트 (이벤트 처리용)
struct UIBaseComponent
{
    bool interactable = true;   // 인터랙션 가능 여부
    bool raycastTarget = true;  // 레이캐스트 대상 여부
};
```

### 3.3 렌더링 파이프라인

**기존 렌더링 시스템과의 통합**:

```cpp
// engine/ecs/UISystem.cpp
class UISystem : public System
{
public:
    void Update(ECSRegistry& registry, float deltaTime) override
    {
        // 1. Canvas 컴포넌트가 있는 엔티티 수집
        auto canvasEntities = registry.GetEntitiesWithComponent<CanvasComponent>();

        // 2. 각 Canvas별로 sortOrder 정렬
        std::sort(canvasEntities.begin(), canvasEntities.end(),
            [&](Entity a, Entity b) {
                return registry.GetComponent<CanvasComponent>(a).sortOrder <
                       registry.GetComponent<CanvasComponent>(b).sortOrder;
            });

        // 3. 각 Canvas 하위 UI 요소들을 계층 순서로 정렬 (DFS)
        for (Entity canvas : canvasEntities)
        {
            RenderCanvas(registry, canvas);
        }
    }

private:
    void RenderCanvas(ECSRegistry& registry, Entity canvasEntity)
    {
        // Orthographic camera로 전환
        renderer->SetCameraMode(CameraMode::Orthographic);

        // Canvas 하위 계층 순회
        std::vector<Entity> uiElements = GatherUIHierarchy(registry, canvasEntity);

        // 각 UI 요소 렌더링
        for (Entity element : uiElements)
        {
            if (registry.HasComponent<TextComponent>(element))
            {
                RenderText(registry, element);
            }
            else if (registry.HasComponent<ImageComponent>(element))
            {
                RenderImage(registry, element);
            }
        }
    }
};
```

---

## 4. 프리팹 시스템 확정을 위한 선행 작업

### 4.1 프리팹 Phase 5: 계층 컴포넌트 구현

**우선순위**: 최상위 (P1.6 → P1.6-Phase 5)

**작업 내용**:

1. **HierarchyComponent 추가**
   ```cpp
   struct HierarchyComponent
   {
       Entity parent = Entity::Null;
       std::vector<Entity> children;
   };
   ```

2. **프리팹 포맷 확장** (v1 → v2)
   ```json
   {
     "version": 2,
     "name": "Complex UI",
     "entities": [
       { "uuid": "...", "components": { ... } },
       { "uuid": "...", "components": { ... }, "parent": "..." }
     ]
   }
   ```

3. **EntityRef 직렬화 지원**
   - `SerializeOptions::rejectEntityRefs` 제거 또는 UI 전용 예외 처리
   - UUID 기반 참조로 직렬화/역직렬화

4. **계층 순회 API 추가**
   ```cpp
   std::vector<Entity> GetChildren(ECSRegistry& registry, Entity parent);
   Entity GetParent(ECSRegistry& registry, Entity child);
   void SetParent(ECSRegistry& registry, Entity child, Entity parent);
   ```

### 4.2 UI 컴포넌트 중복 허용 포맷 변경

**문제**: 현재 프리팹 포맷은 컴포넌트 타입당 1개만 허용

**해결안 1**: 컴포넌트 배열 포맷으로 변경
```json
{
  "components": [
    { "type": "ImageComponent", "data": { "sprite": "bg.png" } },
    { "type": "ImageComponent", "data": { "sprite": "fill.png" } }
  ]
}
```

**해결안 2**: 명명된 컴포넌트 인스턴스
```json
{
  "components": {
    "ImageComponent:bg": { "sprite": "bg.png" },
    "ImageComponent:fill": { "sprite": "fill.png" }
  }
}
```

**권장**: 해결안 1 (배열 포맷) - 더 일반적이고 확장성 있음

### 4.3 UI 전용 SerializeOptions 추가

```cpp
struct SerializeOptions
{
    bool excludePrefabMetadata = false;
    bool rejectEntityRefs = false;
    
    // UI 전용 옵션 (신규)
    bool includeUIComponents = true;     // UI 컴포넌트 포함 여부
    bool flattenUIHierarchy = false;    // UI 계층을 평탄화하여 단일 엔티티로 캡처
};
```

---

## 5. 실사용 가능 수준으로의 발전 로드맵

### 5.1 단계별 구현 계획

| 단계 | 작업 | 선행 조건 | 기간 | 실사용 가능성 |
|---|---|---|---|---|
| **Phase 0** | 프리팹 Phase 5 완료 (계층 컴포넌트) | 현재 프리팹 Phase 1-4 | 1주 | 중간 (다중 엔티티 프리팹 가능) |
| **Phase 1** | UI 기본 컴포넌트 구현 (RectTransform, Canvas) | Phase 0 | 1주 | 낮음 (컴포넌트만 있음, 렌더링 없음) |
| **Phase 2** | UI 렌더링 파이프라인 (TextRenderer, SpriteRenderer) | Phase 1 | 2주 | 중간 (단일 UI 요소 표시 가능) |
| **Phase 3** | UI 이벤트 시스템 (클릭, 호버) | Phase 2 | 1주 | 중간 (기본 인터랙션 가능) |
| **Phase 4** | UI 에디터 패널 (WYSIWYG 배치) | Phase 3 | 2주 | 높음 (에디터에서 UI 제작 가능) |
| **Phase 5** | UI 프리팹 시스템 통합 | Phase 4 | 1주 | 높음 (UI 프리팹 재사용 가능) |
| **Phase 6** | 고급 UI 기능 (스크롤뷰, 그리드 레이아웃) | Phase 5 | 2주 | 매우 높음 (실사용 수준) |

### 5.2 최소 실사용 가능 기준 (MVP)

**실사용 가능한 최소 기능 세트**:

1. ✅ 프리팹 시스템 (다중 엔티티 지원)
2. ✅ RectTransformComponent (위치/크기/앵커)
3. ✅ TextComponent + TextRenderer (텍스트 표시)
4. ✅ ImageComponent + SpriteRenderer (이미지 표시)
5. ✅ ButtonComponent + 기본 이벤트 (클릭 처리)
6. ✅ UI 에디터 패널 (드래그앤드롭 배치)
7. ✅ UI 프리팹 저장/로드

**이 기능만 있으면**:
- "HP 바 프리팹"을 만들어 게임 전체에서 재사용
- "메뉴 버튼 프리팹"으로 일관된 UI 스타일 유지
- 에디터에서 UI를 시각적으로 배치하고 프리팹으로 저장
- Python 스크립트에서 UI 동작 제어

### 5.3 우선순위 조정

**즉시 착수해야 할 작업**:

1. **프리팹 Phase 5** (최우선)
   - 이것이 없으면 복합 UI 프리팹이 불가능
   - UI뿐만 아니라 다른 시스템(캐릭터, 오브젝트)에도 필요

2. **UI 기본 컴포넌트** (2순위)
   - RectTransform, Canvas, Text, Image
   - 이것이 없으면 UI를 구성할 수 없음

3. **UI 렌더링 파이프라인** (3순위)
   - TextRenderer, SpriteRenderer
   - 이것이 없으면 UI가 화면에 표시되지 않음

**나중에 해도 되는 작업**:

- 고급 레이아웃 시스템 (Grid, Stack, Scroll)
- 애니메이션 시스템 (UI 트랜지션)
- 스타일 시스템 (USS/유니티 UI Toolkit 같은)
- 테마/스킨 시스템

---

## 6. 기술적 고려사항 및 리스크

### 6.1 성능 고려사항

**UI 렌더링 최적화**:

1. **배치 렌더링**: 기존 `InstancedBatchManager` 재활용
   - 동일한 텍스처를 쓰는 UI 요소들을 하나의 드로우콜로 렌더링
   - TextRenderer는 글리프별로 인스턴싱

2. **UI 계층 순회 최적화**:
   - 매 프레임 전체 계층을 순회하지 않고 dirty 플래그 사용
   - Canvas별로 변경된 UI 요소만 다시 계산

3. **텍스트 렌더링 최적화**:
   - SDF (Signed Distance Field) 폰트 사용
   - 자주 쓰이는 텍스트는 텍스처 아틀라스에 캐싱

### 6.2 프리팹 시스템과의 통합 리스크

**리스크 1**: 컴포넌트 중복 문제
- 현재 포맷은 컴포넌트 타입당 1개만 허용
- UI는 같은 컴포넌트를 여러 개 쓰는 경우가 많음 (예: 여러 ImageComponent)
- **대응**: 포맷을 배열 기반으로 변경 (Phase 0 선행)

**리스크 2**: 계층 순환 참조
- UI 계층에서 순환 참조가 생길 수 있음
- **대응**: SetParent 시 순환 참조 검사 및 거부

**리스크 3**: UI 컴포넌트와 3D 컴포넌트 충돌
- TransformComponent와 RectTransformComponent가 공존할 수 있음
- **대응**: UI 엔티티는 TransformComponent를 제외 (SerializeOptions로 제어)

### 6.3 에디터 통합 리스크

**리스크 1**: UI 에디터와 3D 뷰포트 충돌
- UI 에디터는 2D 공간, 3D 뷰포트는 3D 공간
- **대응**: 별도의 UI Editor 탭 또는 2D/3D 모드 전환

**리스크 2**: UI 프리팹 인스턴스화 시 위치 계산
- 3D 공간에 배치된 UI (World Space Canvas)와 2D 공간 UI (Screen Space)의 위치 계산이 다름
- **대응**: Canvas의 renderMode에 따라 다른 위치 계산 로직

---

## 7. 결론 및 권장 사항

### 7.1 프리팹 시스템은 UI 커스터마이징의 강력한 기반이 될 수 있음

**긍정적 평가**:
- ✅ 컴포넌트 리플렉션 시스템이 잘 설계되어 있어 UI 컴포넌트 추가가 용이
- ✅ PrefabAsset의 ApplyToEntity가 UI 컴포넌트에도 그대로 적용 가능
- ✅ SerializeOptions로 UI 전용 캡처 정책을 추가할 수 있음
- ✅ 에디터 통합 (Prefab Browser, Inspector)이 이미 완성되어 있음

### 7.2 하지만 선행 작업이 필수적임

**반드시 선행되어야 할 작업**:
1. **프리팹 Phase 5 완료** (계층 컴포넌트, 다중 엔티티 프리팹)
   - 이것이 없으면 복합 UI 프리팹이 불가능
   - 단계: 1주, 난이도: 중간

2. **프리팹 포맷 확장** (컴포넌트 배열 기반)
   - UI에서 같은 컴포넌트를 여러 개 쓰는 경우 지원
   - 단계: Phase 5와 함께, 난이도: 낮음

3. **UI 기본 컴포넌트 구현**
   - RectTransform, Canvas, Text, Image
   - 단계: 1주, 난이도: 낮음

### 7.3 실사용 가능 수준까지의 예상 기간

**최소 실사용 가능 기능 (MVP)**: 6~8주
- Phase 0 (프리팹 Phase 5): 1주
- Phase 1-2 (UI 컴포넌트 + 렌더링): 3주
- Phase 3-4 (이벤트 + 에디터): 3주
- Phase 5 (프리팹 통합): 1주

**완전한 실사용 수준 (고급 기능 포함)**: 10~12주
- 위 MVP + Phase 6 (고급 UI 기능): 4주

### 7.4 권장 실행 순서

```
1. 프리팹 Phase 5 착수 (최우선)
   - 계층 컴포넌트 구현
   - 다중 엔티티 프리팹 지원
   - EntityRef 직렬화

2. UI 기본 컴포넌트 구현
   - RectTransform, Canvas, Text, Image
   - ComponentRegistry 등록

3. UI 렌더링 파이프라인
   - TextRenderer, SpriteRenderer
   - 기존 RenderGraph와 통합

4. UI 이벤트 시스템
   - 클릭, 호버, 드래그
   - Raycast 시스템

5. UI 에디터 패널
   - WYSIWYG 배치
   - 드래그앤드롭

6. UI 프리팹 시스템 통합
   - Prefab Browser에 UI 프리팹 표시
   - Inspector에서 UI 컴포넌트 편집
```

### 7.5 최종 결론

**프리팹 시스템은 인게임 UI 커스터마이징의 대모(Dominant Pattern)이 될 수 있음**

- 현재 프리팹 시스템의 설계가 UI 요구사항과 잘 맞음
- 하지만 **계층 컴포넌트(Phase 5)**가 선행되어야 실사용 가능
- UI 컴포넌트 추가와 렌더링 파이프라인 구축이 필요
- 6~8주 내에 최소 실사용 가능 기능을 구현할 수 있음

**권장**: 프리팹 Phase 5를 즉시 착수하고, 그 후 UI 컴포넌트 구현을 진행할 것을 권장함. 이 순서가 가장 효율적이고 리스크가 낮음.

---

## 8. 레거시 흔적 분석 및 정리 계획 (2026-09-19 추가)

### 8.1 레거시 흔적 분석 결과

전체 코드베이스에서 레거시 흔적을 분석한 결과, 다음 사항들이 확인되었습니다:

#### 8.1.1 문서와 실제 코드의 불일치

| 항목 | 문서 기술 | 실제 상태 | 영향 |
|---|---|---|---|
| **BUILD_LEGACY_MAIN 옵션** | 여러 문서에서 `BUILD_LEGACY_MAIN` CMake 옵션 존재로 기술됨 | 실제 `engine/CMakeLists.txt`에는 해당 옵션 없음 | 문서만 레거시, 실제 코드는 정리됨 |
| **ge_python 모듈명** | 일부 문서에서 `ge_python`으로 기술됨 | 실제로는 `quarterflying`로 개명됨 (ROADMAP.md P1 Phase 6 완료) | 문서 업데이트 필요 |
| **죽은코드 폴더** | `UNUSED_AND_LEGACY_REPORT.md`에서 `죽은코드/` 폴더 분류 언급 | 실제로는 해당 폴더 존재하지 않음 | 보고서 내용만, 실제 미이행 |

#### 8.1.2 아카이브된 레거시 파일

**존재하는 아카이브**:
- `docs/archive/legacy/` - 5개 레거시 문서 보관
  - `FINAL_SINGLE_GUI_REPORT.md` - PySide6 단일 GUI 결정 기록
  - `GUI_INTEGRATION_PLAN.md` - GUI 통합 계획
  - `ARCHITECTURE_IMPROVEMENT_PLAN_v3.md` - 아키텍처 개선 계획
  - `ARCHITECTURE_IMPROVEMENT_PLAN_4TH_REVIEW_FIXES.md` - 리뷰 수정본
  - `IMPROVEMENT_PLAN_KO.md` - 한국어 개선 계획

**정리된 레거시**:
- `engine/app/` - GLFW 기반 메인 애플리케이션 (삭제됨)
- QML 실험 파일 - `docs/archive/legacy/`로 이동됨

#### 8.1.3 현재 활성화된 바인딩 구조

**이미 개선된 상태**:
- `engine/editor/engine_binding.py` - 단일 진입점 래퍼 (ROADMAP.md P1 Phase 3 완료)
- `quarterflying` 모듈명 - 개명 완료 (ROADMAP.md P1 Phase 6 완료)
- 모든 에디터 코드가 `engine_binding.py`를 통해서만 엔진 임포트

**남은 문제점**:
- 일부 문서에서 여전히 `ge_python`/`ge_engine` 옛 이름 사용
- `BUILD_LEGACY_MAIN` 관련 기술이 여전히 문서에 남음

### 8.2 레거시 정리 계획

#### 8.2.1 문서 업데이트 (즉시 실행 권장)

**우선순위 1: 모듈명 일치**
- `UNUSED_AND_LEGACY_REPORT.md`의 `ge_python` → `quarterflying`로 전체 수정
- `PLAN.md`의 `ge_python` → `quarterflying`로 전체 수정
- `engine/ARCHIVE_INFO.md`의 관련 기술 수정

**우선순위 2: BUILD_LEGACY_MAIN 제거**
- `engine/ARCHIVE_INFO.md`에서 `BUILD_LEGACY_MAIN` 관련 기술 제거
- `engine/editor/PHASE3_LEGACY_REMOVAL.md`에서 해당 옵션 기술 제거 또는 "제거됨"으로 수정
- `docs/archive/legacy/FINAL_SINGLE_GUI_REPORT.md`에서 관련 기술 수정

**우선순위 3: 아카이브 문서 정리**
- `docs/archive/legacy/` 폴더의 문서들이 현재 상태와 일치하는지 검토
- 완전히 폐기된 계획인 경우 `docs/archive/deprecated/`로 추가 분류 고려

#### 8.2.2 코드 정리 (이미 완료됨)

다음 항목들은 이미 정리되었으므로 추가 작업 불필요:

- ✅ 단일 진입점 래퍼 (`engine_binding.py`)
- ✅ 모듈명 개명 (`ge_python` → `quarterflying`)
- ✅ GLFW 메인 애플리케이션 제거
- ✅ QML 실험 파일 아카이빙

#### 8.2.3 빌드 시스템 정리 (검토 필요)

**확인 필요 항목**:
- `engine/build/` - 사실상 빈 캐시인지 확인 후 삭제 고려
- `engine/build_debug/` - 실제 사용 중인 빌드인지 확인
- 중복 의존성 다운로드 (`build/deps/` vs `engine/build_debug/deps/`)

### 8.3 UI 커스터마이징 작업과의 연계

#### 8.3.1 UI 컴포넌트 추가 시 영향

레거시 정리는 UI 커스터마이징 작업에 직접적인 영향을 주지 않습니다:

- **긍정적 영향**: 문서가 정리되면 신규 개발자가 현재 상태를 더 빨리 이해
- **부정적 영향**: 없음 - UI 컴포넌트는 완전히 새로운 기능이므로 레거시와 무관

#### 8.3.2 프리팹 Phase 5와의 연계

프리팹 Phase 5 (계층 컴포넌트)는 레거시와 무관하게 진행:

- 새로운 컴포넌트 추가이므로 기존 레거시 코드와 충돌 없음
- 기존 `PrefabAsset`/`PrefabInstanceComponent` 확장이므로 호환성 유지 용이

### 8.4 정리 일정

| 단계 | 작업 | 기간 | UI 작업과의 연계 |
|---|---|---|---|
| **Step 1** | 문서 모듈명 일치 (`ge_python` → `quarterflying`) | 1시간 | 독립적 - UI 작업 전 완료 권장 |
| **Step 2** | BUILD_LEGACY_MAIN 관련 기술 제거 | 30분 | 독립적 |
| **Step 3** | 아카이브 문서 검토 및 재분류 | 2시간 | 독립적 |
| **Step 4** | 빌드 디렉토리 정리 검토 | 1시간 | 독립적 |
| **총계** | 4.5시간 | UI 작업과 병행 가능 |

### 8.5 권장 실행 순서

**옵션 A: 레거시 정리 우선**
```
1. 문서 업데이트 (Step 1-3) 완료
2. 프리팹 Phase 5 착수
3. UI 컴포넌트 구현
```

**옵션 B: 병행 진행**
```
1. 문서 업데이트 (Step 1-2) - 1.5시간 투자
2. 프리팹 Phase 5 착수
3. UI 컴포넌트 구현 진행 중 아카이브 검토 (Step 3)
4. 빌드 디렉토리 검토는 빌드 시 자연스럽게 확인
```

**권장**: 옵션 B (병행 진행)
- 문서 업데이트는 1.5시간이면 충분하므로 UI 작업 시작 전 빠르게 완료
- 나머지는 UI 작업 진행 중 자연스럽게 처리
- 레거시 정리가 UI 작업을 지연시키지 않음

### 8.6 결론

**레거시 상태 요약**:
- 대부분의 레거시 코드는 이미 정리됨
- 주요 문제는 **문서와 실제 코드의 불일치**
- 바인딩 구조는 이미 잘 정리되어 있음 (`engine_binding.py`)

**UI 커스터마이징 작업에 미치는 영향**:
- 기술적 영향: 없음
- 문서적 영향: 긍정적 - 정리된 문서가 개발 속도 향상

**최종 권장**:
1. 문서 업데이트 (1.5시간)을 UI 작업 시작 전 빠르게 완료
2. 프리팹 Phase 5와 UI 컴포넌트 구현을 병행 진행
3. 나머지 레거시 정리는 작업 중 자연스럽게 처리