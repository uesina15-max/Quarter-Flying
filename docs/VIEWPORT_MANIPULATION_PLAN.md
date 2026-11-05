# 뷰포트에서 오브젝트 조작 (2026-10-30)

요청: Scene 뷰포트에서 오브젝트를 직접 고르고 움직인다. 이전에는 Scene Hierarchy·Inspector로만 고르고 숫자로만 옮길 수 있었다(좌클릭은 카메라 조작에도 쓰이지 않는 빈 입력이었다).

## 범위

- **클릭 = 선택**: 좌클릭한 곳 아래 가장 가까운 오브젝트를 고른다. 빈 곳을 누르면 선택 해제. Scene Hierarchy·Inspector·상태바와 같은 선택이다.
- **드래그 = 이동**: 좌클릭한 채 끌면 잡은 점이 커서를 따라 수평면(XZ)에서 움직인다. Shift를 누르면 위아래(Y)로만 움직인다(드래그 중 바꿔도 된다). 드래그 한 번 = Undo 한 번.
- **선택 강조**: 고른 오브젝트(와 자손)의 메시를 노란 기운으로 칠한다. 셰이더의 `isSelected`는 이미 있었고 항상 0이었다. Play 중에는 그리지 않는다.
- **변환 기즈모**(사용자 요청 "기즈모 개선", "로테이션과 스케일 무브 등을 선택 가능하게"로 추가): 하나만 골랐을 때 그 위치에 그린다. Inspector Transform의 줄 이름 버튼(✥ 이동 / ⟳ 회전 / ⤢ 크기) 또는 뷰포트에서 W / E / R로 고른다(Play 중에는 단축키가 꺼진다 - W는 게임 입력). 처음에는 툴바 버튼으로 만들었다가, 사용자가 "여길 통합해야 함"(Transform 칸)을 골라 Inspector로 옮기고 툴바 버튼은 없앴다.
    - 이동: X(빨강)·Y(초록)·Z(파랑) 화살표, 월드 축. 화살표를 끌면 그 축으로만.
    - 회전: 축마다 고리, 월드 축. 고리를 끌면 그 축을 중심으로 돈다(부모가 있으면 부모 기준 로컬 회전으로 저장). 고리가 화면과 거의 수직이면 가로 이동량(0.5도/px)으로 돌린다.
    - 크기: 끝에 상자가 달린 손잡이, 오브젝트의 로컬 축(크기는 로컬 값). 손잡이 길이만큼 끌면 그 성분이 두 배. 가운데 상자는 세 성분을 함께(오른쪽·위로 150px = 두 배). 0 이하로는 줄어들지 않는다.
    - 공통: 마우스를 올린(끌고 있는) 손잡이는 노랑. 화면 크기가 거리와 무관하게 일정하고, 물체에 가려지지 않게 맨 위에 그린다. Edit 상태 + 에디터 카메라로 볼 때만 있다(게임 카메라 보기·Play·Motion Editor에는 없다). 몸통을 끌어 옮기기는 이동 도구에서만(회전·크기 도구에서 몸통 클릭은 선택만).
- **Inspector Transform**(사용자 요청 "로테이션과 스케일을 gui내의 위젯에선 zyx축으로 통합", "여길 통합해야 함" → 사용자 선택: 도구 선택을 Transform에 통합):
    - 표 하나: 축 이름(X 빨강·Y 초록·Z 파랑, 기즈모와 같은 색)은 맨 위 한 줄, 줄은 이동(위치)·회전·크기. 줄 이름이 기즈모 도구 버튼이고 지금 도구가 강조된다.
    - Inspector 위쪽 고정 영역 맨 위에 둔다(예전에는 키보드·카메라 작업 위젯 뒤 스크롤 영역에 있어 패널 아래로 밀려 잘렸다).
    - 다른 컴포넌트의 XYZ 칸도 세 칸이 한 줄에 다 보이게 했다(예전에는 칸의 최소 폭이 범위 문자열(-99999.00) 길이라 Z 칸이 패널 밖으로 밀렸다).
- 그대로 둔 것: 우클릭 궤도·휠클릭 이동·휠 줌·Alt+좌클릭 궤도, Play 중 입력(게임으로 감), Motion Editor.
- 하지 않는 것: 평면(두 축) 핸들, 이동·회전의 로컬 축 전환, 눈금 맞춤(스냅), 여러 개 동시 변환, 사각 영역 선택, 삼각형 단위 피킹(상자 근사).

## 1. 바뀌는 곳

- `renderer/Mesh.{h,cpp}`: 정점 경계(로컬 AABB)를 `create` 때 계산해 보관(`setLocalBounds`).
- `ecs/Picking.{h,cpp}` (새): 화면 좌표 → 광선(`MakePickRay`), 광선-상자 교차, `PickEntity`(대상 규칙의 유일한 구현). GL 없음.
- `ecs/Hierarchy.{h,cpp}`: `LocalPositionForWorld` - 월드 위치가 되려면 필요한 로컬 위치(부모 역행렬).
- `ecs/RenderSystem.{h,cpp}`: `GetMeshBounds`(불러오지 않는 조회), `SetSelectedEntities` → 인스턴스 `isSelected`.
- `core/Engine.{h,cpp}`: `GetViewRay`, `PickEntityAtScreen`(지금 그리는 카메라 기준), `SetSelectedEntities`(Edit 상태일 때만 RenderSystem에 전달).
- `bindings/EngineBindings.cpp`, `bindings/ECSBindings.cpp`: 위 함수 + `GetWorldPosition`, `LocalPositionForWorld`.
- `editor/core/viewport_manipulation.py` (새): 드래그 평면 계산 + 클릭/드래그 흐름(Qt 없음).
- `editor/viewport.py`: 좌클릭·이동·놓기를 조작기로 보낸다.
- `editor/panels/scene_hierarchy.py`: `select_entity(id)` - 바깥(뷰포트)에서 고른 것을 트리 선택으로.
- `editor/main.py`: 조작기 연결, 트리 선택 → 엔진 강조, 상태바 선택 이름.
- CMake: `ecs/Picking.cpp`, `renderer/TransformGizmo.cpp`(엔진), `tests/PickingTests.cpp`.
- 기즈모: `renderer/TransformGizmo.{h,cpp}`(새, 세 도구의 손잡이 형상·화면 크기·잡기 판정 - 그리기와 잡기가 같은 계산을 쓴다), `renderer/BoneLineRenderer`(`depthTest` 인자), `renderer/Renderer`(`GetOverlayLines` - 메시 뒤에 깊이 무시로 그림), `core/Engine`(`UpdateTransformGizmo` 매 프레임, `PickGizmoAt`, `SetGizmoHighlight`, `SetGizmoMode`, 뷰포트 픽셀 크기), 바인딩 `PickGizmo`·`SetGizmoHighlight`·`SetGizmoMode`·`GetWorldRotation`, `core/viewport_manipulation.py`(`AxisDrag`·`RotateDrag`·`ScaleDrag`, `hover`, `set_mode`), `main.py`(W/E/R, Inspector 도구 신호 연결).
- Inspector: `panels/inspector.py` - `DynamicComponentWidget`의 TransformComponent 표(`tool_selected` 신호, `set_tool`), `InspectorPanel`의 Transform 고정 영역(`transform_widget`, `set_gizmo_tool`, `gizmo_tool_selected`), Vec3/오일러 칸(폭 나눠 쓰기, 축 색).

## 2. 영향받는 곳

- 렌더: `RenderSystem::Update`가 엔티티마다 선택 여부를 본다(선택 수 × 조상 깊이, 보통 1개).
- Undo/저장 표시: 드래그는 기존 `EditorAPI.move_entity`·`rotate_entity`·`scale_entity`(Move/Rotate/ScaleEntityCommand) + 병합 세션 - 새 명령 없음. 병합 규칙은 `CommandManager::Execute`(같은 세션 안에서만 합침).
- Play/Stop: 조작기는 Edit 모드·Scene Editor 화면에서만 켜진다. 엔티티 id가 Stop에서 바뀌는 문제는 기존 재선택 경로(main `_on_stop`)를 그대로 탄다.
- Player: Mesh·RenderSystem·Engine 변경이 엔진 DLL에 들어간다. Player는 선택을 설정하지 않으므로 화면 변화 없음. 템플릿을 다시 빌드해야 DLL이 맞는다.
- 저장 형식: 없음.

## 3. 호환성

저장 형식·리플렉션 변경 없음. 바인딩은 추가만 했다.

## 4. 고장 시 갈리는 지점

- 클릭했는데 엉뚱한 것이 잡힘: `Engine.GetViewRay`의 광선(그린 카메라와 같은가) → `PickEntityAtScreen`의 `point`·`distance` → 메시 경계(`RenderSystem::GetMeshBounds`).
- 잡혔는데 안 움직임: `[Viewport]` 로그(명령 실패 사유), `move_entity` 예외(Play 잠금 등).
- 움직이는데 커서와 어긋남: 부모 변환(`LocalPositionForWorld`) 또는 평면 교차(`core/viewport_manipulation.py`).
- 강조가 안 보임: 트리 선택 → `Engine.SetSelectedEntities` → `RenderSystem` `isSelected`.
- 기즈모가 안 보이거나 안 잡힘: 선택 수(1개여야 함)·뷰 카메라(Editor)·World 상태(Edit) → `Engine.PickGizmo`가 None인가 → `TransformGizmo` 허용 거리(`kPickTolerancePx`)와 뷰포트 픽셀 크기(`HandleWindowResize`).
- 축으로 끄는데 튐: 축이 시선과 거의 나란함(`closest_axis_parameter`가 None을 주면 그 이동은 건너뛴다).
- 회전이 엉뚱한 축으로 돎: 부모 회전(`GetWorldRotation(parent)`)과 `RotateDrag.value`의 inverse(P)·R·W 순서.
- 크기가 다른 축으로 늘어남: 크기 기즈모의 축은 로컬(엔티티 월드 회전) - `PickGizmo`의 `axis`와 `handle`(1~3 = 로컬 X~Z 성분).

## 5. 회귀 확인

- C++ `PickingTests`: 광선 방향, 광선-상자 거리(이동·스케일·회전·뒤·안·스케일 0), 가장 가까운 것, 빈 엔티티 제외, 자식의 월드 행렬, 메시 경계 조회, 카메라·파티클 대리 상자, 회전·스케일 부모의 로컬 위치 역산.
- Python `test_viewport_manipulation.py`: 평면 교차, 수평/수직 드래그, 드래그 중 Shift 전환, 문턱값 아래 클릭은 이동 없음, 드래그 = Undo 한 번, 빈 곳 클릭 = 선택 해제.
- 실제 실행: 에디터에서 클릭 선택·강조·드래그·Undo를 화면 캡처로 확인.
- 기즈모: C++ `TransformGizmo` 테스트 5개(거리와 무관한 화면 크기·카메라 뒤 없음, 이동 축 잡기·허용 거리 경계, 시선 방향 축 제외·선분이 축 위, 회전 고리가 축에 수직·고리 잡기, 크기는 로컬 축·가운데는 전체). Python 축 드래그·hover·회전(부모 있는 경우 포함)·크기·도구별 몸통 드래그 테스트 + 실제 엔진으로 이동 화살표·크기 가운데 상자·회전 고리를 잡아 끌고 각각 Undo 한 번, Play 중 기즈모 없음.

## 결과 (2026-10-30)

검증 단계(CLAUDE.md 관례 2):

- **단위 테스트 통과**: C++ 전체 통과(새 `Picking` 7개 + `TransformGizmo` 5개). 에디터 Python 테스트 16개 파일 통과(새 `test_viewport_manipulation.py` 17개 - 실제 엔진 모듈로 피킹·드래그·이동/회전/크기 기즈모·Undo·Play 중 기즈모 없음 포함).
- **실제 실행(화면 캡처)**: 에디터를 엔진과 함께 띄워 확인했다.
  - 클릭 선택: 뷰포트에서 고른 피라미드가 노랗게 강조되고, Scene Hierarchy·Inspector·상태바(`Selected: center_pyramid`)가 같이 바뀌었다.
  - 몸통 드래그: 사용자가 직접 끌어 위치가 바뀌는 것을 화면에서 확인했다(Inspector 위치 갱신). 로그에 오류 없음.
  - 기즈모: 고른 Test Cube 위에 세 화살표가 물체에 가려지지 않고 그려지고, 마우스를 올린 축이 노랗게 바뀌었다.
  - FPS: 선택이 있는 상태에서 33~38(이전과 같은 범위).
  - Inspector 맨 위의 Transform 표(축 이름 한 줄, 줄 이름 = 도구 버튼, 지금 도구 강조)가 보이고, 툴바에는 도구 버튼이 없다.
  - 회전 도구: 고른 Head에 세 고리가 그려지고, 끄는 고리가 노랗게 강조되며 회전 값이 바뀌는 것을 확인했다(사용자가 쓰는 중이라 자동 클릭이 그 드래그와 겹쳤을 수 있다).
  - 사용자가 이동 도구로 화살표를 잡아 옮기는 동안 그 축이 노랗게 강조되는 것을 확인했다. 로그 오류 없음.
- **화면으로 확인하지 않은 것**: 크기 손잡이가 그려진 모습은 캡처하지 못했다(사용자가 같은 창을 쓰는 중이라 입력을 멈췄다). 형상과 잡기는 C++ 테스트, 끌기·Undo는 실제 엔진 테스트가 확인한다. 그리는 경로는 화면에서 확인한 화살표·고리와 같다.
- Player 템플릿은 다시 빌드하지 않았다(엔진 DLL 변경 - Build Game 전에 `player_template` 재빌드 필요. Player 화면 동작은 바뀌지 않는다).
