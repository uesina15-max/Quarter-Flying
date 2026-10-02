# 외부 리뷰 검증 기반 개선안 (2026-09-30)

외부 코드 리뷰(강점/약점/우선순위 표, RenderGraph 자원 상태 추적과 D3D12 상태 관리 "구현 완료" 보고)를
현재 코드(커밋 `ed83874` 기준)와 하나씩 대조했다. **사실로 확인된 지적만** 개선안에 반영했다.
틀렸거나 이미 해결된 지적은 §3에 제외 이유와 함께 모았다.

각 항목의 "근거"는 직접 확인한 파일과 줄 번호다. 상태 표기는 CLAUDE.md 관례 2번(컴파일 / 유닛 테스트 /
실제 실행)을 따른다.

---

## 1. 개선안 (우선순위 순)

### P0: 크래시와 "그려지는 것"

| # | 항목 | 근거 | 할 일 |
|---|---|---|---|
| P0-1 ✅ | 무효한 셰이더나 인덱스 버퍼 상태로 draw가 그대로 나감 | `OpenGLCommandList.cpp:297`은 `Invalid shader ID`를 로그로만 남기고 `glUseProgram(0)`으로 계속 진행한다. `Renderer.cpp:428-449`는 그 결과를 확인하지 않고 draw를 호출한다. `RendererInstancingTest.SubmitIndexedInstancedBatchSucceeds`가 이 경로에서 access violation으로 실패한다(남은 실패 테스트 1개). | `SetShader`가 실패를 반환하게 하고, `Submit*`은 draw를 건너뛰며 `Result` 에러를 돌려준다. 인덱스 draw는 IBO 바인딩 여부를 확인한다(관례 3번). 테스트는 실제 셰이더와 메시를 쓰도록 고친다. |
| P0-2 ✅ | 메시 에셋 브리지 없음(`scene.json` → `meshHandle`) | `RenderSystem.h:36-40`: 등록된 메시가 없으면 항상 절차적 큐브로 대체한다. ROADMAP §6에도 "별개 후속 작업"으로 남아 있다. | `objects[].model` 경로를 `RenderSystem::RegisterMesh()`로 이어 주는 최소 브리지를 만든다(OBJ 로더 `tinyobjloader`는 이미 링크돼 있음). |

### P1: 구조

| # | 항목 | 근거 | 할 일 |
|---|---|---|---|
| P1-1 ✅ | 계층(부모-자식) 컴포넌트 없음 (2026-10-01 `HierarchyComponent` + 다중 엔티티 프리팹 v2 + Scene Hierarchy 트리/드래그 재부모화. 검증: 컴파일 ✓, 유닛 테스트 545/545 ✓, 실제 에디터에서 트리 표시·루트 이동 시 자식 추종·실제 마우스 드래그 재부모화·Undo를 캡처로 확인 ✓. 설계와 한계는 `PREFAB_IMPLEMENTATION_PLAN.md` "Phase 5A 구현 기록") | `ecs/`에 `HierarchyComponent`나 `ParentComponent`가 없다. 프리팹 v1은 단일 엔티티 전용이다. | 최신 계획서(`INGAME_UI_CUSTOMIZATION_PLAN.md` §4.1)의 프리팹 Phase 5와 같은 작업이다. 순환 참조 검사를 포함해 진행한다. |
| P1-2 ✅ | `asset/` 고아 모듈 (2026-09-30 **유지하고 텍스처 경로로 연결**. 메시는 기존 `Mesh::loadFromFile`을 그대로 쓰고 `asset/`은 텍스처 파일 → 픽셀만 맡는다. 검증 단계: 컴파일 ✓, 유닛 테스트 ✓(`AssetManagerTextureTest` 4개는 실제 BMP 파일로, `RenderSystemTextureTest` 6개, 전체 528/528), 실제 에디터 실행 ✓(Test Cube와 `scene.json` 피라미드에 체커 텍스처가 그려지는 것을 화면 캡처로 확인했고, 텍스처 없는 그리드는 이전과 같은 색). **미검증**: 텍스처 상하 방향(체커라 캡처로 구분 불가), Inspector에서 Texture Path를 바꾸는 흐름) | `CMakeLists.txt:96-98`에서 빌드는 되지만, 모듈 밖에서 include하는 파일이 0개다. | 연결 경로: `RenderableComponent.texturePath` → `RenderSystem::ResolveTexture`(경로당 1회, 실패 시 경고 1회 후 텍스처 없이) → Engine이 조립한 로더(`AssetManager::LoadAsset` + `GetAsset<TextureData>` → `Texture2D::Create`) → 배치 키 `(mesh, texture)` → `InstancedBatchManager::RenderBatch`가 unit 0에 바인딩 → 셰이더 `uHasTexture`/`uTexture`. 연결하면서 드러난 선존재 버그 5개(임포터가 픽셀을 버림, `GetAsset<T>` 정의 없음, 레지스트리 참조 카운트 0, `uint32_t` 참조 카운트 언더플로, 전체 언로드 후 재로드 시 빈 데이터)를 고쳤고, 각 위치에 증상/원인 주석을 남겼다. 에디터 쪽에서는 `demo_scene_integration.py` 화이트리스트가 `texture`를 버리던 것과, Test Cube가 그리드 큐브 안에 묻혀 안 보이던 것을 고쳤다. |
| P1-3 ✅ | README의 멀티플랫폼 주장과 실제 코드 불일치 (2026-09-30 README를 Windows 전용으로 정정. 없는 스크립트/문서 링크, 존재하지 않는 단축키 설명, 연결 안 된 Open Scene 설명도 정정) | `README.md:3,87`에는 "Windows, Linux, macOS 지원"이라고 적혀 있다. 그러나 `PlatformFactory.cpp:15-16`은 비Windows에서 `nullptr`을 반환하고, 플랫폼 구현은 `Win32Platform` 하나뿐이다. | 먼저 README를 "현재 Windows 전용"으로 고친다(비용이 가장 작다). Linux 스텁은 실제로 포팅을 시작할 때 만든다. |
| P1-4 ✅ | `g_PlatformInstance` 전역 포인터 (2026-09-30 창별 GWLP_USERDATA로 교체. CreateWindow 실패 경로의 이중 해제도 수정) | `Win32Platform.cpp:34,44,53`: 생성자가 덮어쓰고, 소멸자는 **무조건** `nullptr`로 만든다. 에디터에 Engine이 두 개 생긴 지금(Scene과 Play), 한쪽이 파괴되면 다른 쪽의 `WindowProc` 입력이 끊긴다. 임베드 모드는 `WindowProc`를 쓰지 않아 영향이 작지만, 독립 창 모드는 영향을 받는다. | HWND별로 인스턴스를 찾도록 바꾼다(`SetWindowLongPtr(GWLP_USERDATA)`). |
| P1-5 | Scene Hierarchy "Duplicate" 메뉴가 엔진에 연결돼 있지 않음 (2026-10-01 추가) | `editor/panels/scene_hierarchy.py:544` `_duplicate_entity()`는 더미 id(`_dummy_counter`)로 트리 항목만 추가하고 ECS에는 아무것도 만들지 않는다. 그래서 메뉴를 누르면 `<이름>_copy`가 트리에 잠깐 보이다가, 500ms 갱신(`refresh_from_engine`)이 트리를 엔진 기준으로 다시 그릴 때 사라진다. 에러도 로그도 없어서 사용자는 "복제가 안 된다"로 본다. 같은 파일 `create_entity()`(393행)도 `registry.CreateEntity()`를 직접 불러서 Undo가 안 되고 Transform도 붙지 않는다. | `EditorAPI::DuplicateEntity` + `DuplicateEntityCommand`를 만든다. 대상과 자손 전체를 복제하고(`CaptureSubtreeSnapshots` 재사용) 새 UUID를 준다. 복제된 하위 트리 안을 가리키는 EntityRef(자식의 parent, 카메라 리그 target 등)는 새 엔티티로 다시 매핑하고, 바깥을 가리키는 참조는 그대로 둔다. 원본과 같은 부모 아래에 둔다. Undo로 복제본 전체를 지운다. 프리팹 인스턴스를 복제하면 `PrefabInstanceComponent`도 복사되어 여전히 같은 프리팹의 인스턴스로 남는다. `create_entity()`도 `EditorAPI.create_entity` 경로로 바꾼다. 검증은 실제 에디터에서 복제본이 트리와 화면에 남는지, Ctrl+Z로 사라지는지 확인한다. |

### P2: 유지보수와 장기 과제

| # | 항목 | 근거 | 할 일 |
|---|---|---|---|
| P2-1 ✅ | RenderGraph Barrier에 상태 정보 없음 (2026-09-30 `ResourceState` src->dst, 상태가 바뀔 때만 배리어, 내용 검사 테스트 추가) | `RenderGraph.h:100`에 `// TODO: Add barrier type and state information`이 있고, `CommandList::ResourceBarrier(handle)`는 인자 하나다. | 리뷰가 제안한 백엔드 중립 `ResourceState`(src→dst)와 "src ≠ dst일 때만 삽입" 방식을 **RenderGraph 쪽에만** 도입한다. 배리어 내용을 검사하는 유닛 테스트를 같이 추가한다. |
| P2-2 ✅ | **(리뷰가 놓친 문제)** 병렬 실행 경로의 배리어 시점 (2026-09-30 패스 잡 안에서 실행 직전에 기록. 이 경로를 처음 테스트하다가 JobSystem 의존성 디스패치가 항상 교착하던 버그를 발견해 함께 고침 - 커밋 메시지 참고) | `RenderGraph.cpp:475-492`: `ExecuteParallel`은 모든 패스 잡이 **끝난 뒤에** 배리어를 한꺼번에 기록한다. 순차 경로(`Execute`, 239행)는 패스 사이에 넣는다. | 배리어를 패스 의존성 사이(잡 시작 전)에 기록하도록 고친다. P2-1과 함께 한다. |
| P2-3 ✅ | CMake 전역 include 경로 (2026-09-30 `target_include_directories(quarterflying_engine PUBLIC ...)`로 이동) | `CMakeLists.txt:18`에 `include_directories(${CMAKE_CURRENT_SOURCE_DIR})`가 있다. | 타겟별 `target_include_directories`로 옮긴다. |
| P2-4 ❌ 정정 | ~~쓰지 않는 `pybind11::embed` 링크~~ **틀린 지적이었다(2026-09-30 정정).** `ecs/ScriptSystem.cpp`가 실행 중인 인터프리터 안에서 pybind11 API(`py::module_::import`, `py::object`, GIL)를 쓰므로 링크가 필요하다. 처음 검증 때 `scoped_interpreter`/`embed.h`만 검색해서 놓쳤다. CMakeLists.txt에 이유를 주석으로 남겼다. | `CMakeLists.txt:137`에서 엔진 `BASE_LIBRARIES`에 `pybind11::embed`가 들어 있지만, 코드에 `scoped_interpreter`나 `pybind11/embed.h` 사용이 없다. 엔진 DLL이 불필요하게 Python 런타임에 링크된다. | 제거 후 빌드하고, 에디터 실행으로 확인한다. |
| P2-5 | 문자열을 가진 컴포넌트 | `Components.h:86-88`(`AIComponent`의 `std::string` 3개), `:126`(`ActionPlayerComponent`). | 당장 성능 문제는 없다. 컴포넌트 수가 늘어날 때 경로나 이름을 핸들(해시/ID)로 바꾸는 것을 검토한다. |
| P2-6 | 문서와 코드 불일치, 잉여 파일 | README(P1-3), ROADMAP 상태표의 날짜 지난 항목들, 루트 `새 텍스트 문서*.txt` 등 | `docs/SURPLUS_FILES.md` 목록대로 정리한다. |
| P2-7 | 프리팹 베리언트, 인스턴스 수정을 원본 프리팹에 반영(Apply) — Prefab Phase 5B (2026-10-01 추가) | 지금은 방향이 한쪽뿐이다. 프리팹 → 인스턴스(Instantiate, Revert)는 되지만, 인스턴스에서 고친 값을 프리팹 파일에 되돌려 쓰는 경로가 없다. 방법은 "Create Prefab"으로 같은 경로에 덮어쓰는 것뿐인데, 이러면 인스턴스 위치 같은 배치 값까지 프리팹에 들어간다. 게다가 이미 씬에 있는 다른 인스턴스는 그 변경을 받지 못한다(Revert를 하나씩 눌러야 하고, 그러면 각 인스턴스의 개별 수정도 사라진다). 베리언트("기본 프리팹 + 차이만 저장")도 없다. `PrefabInstanceComponent.sourcePrefabVersion`은 파일 포맷 버전일 뿐이라 "어느 개정에서 갈라졌는지"를 판단할 수 없다(`PREFAB_IMPLEMENTATION_PLAN.md` §2.3 Option A, §5). | 착수 전에 `PREFAB_IMPLEMENTATION_PLAN.md`에 설계 절을 먼저 쓴다. 정할 것은 다음과 같다. (1) 인스턴스별 오버라이드 기록 방식: 필드 단위 "원본과 다른 값" 목록. 배치 값(루트 Transform, 부모)은 기본적으로 오버라이드로 취급해 Apply 대상에서 뺀다. (2) Apply: 인스턴스의 오버라이드를 프리팹 파일에 쓰고, 같은 파일의 다른 인스턴스에는 그들의 오버라이드를 보존한 채 새 값을 반영한다. 하나의 Undo 단위로 묶는다. (3) 개정 추적용 `sourceRevision` 필드(해시 또는 증가 카운터, Option B). (4) 베리언트 파일 포맷(`"base": "<경로>"` + 차이), 기본 프리팹이 바뀌었을 때의 전파, 순환(A의 base가 B, B의 base가 A) 거절. (5) 자식 엔티티를 오버라이드와 짝지을 기준: 프리팹 안의 엔티티 인덱스는 편집 중 바뀔 수 있으므로, 인스턴스 자식에 "프리팹 내 식별자"를 기록할지 결정한다. Inspector에는 오버라이드된 필드 표시와 필드별 Revert/Apply가 필요하다. |
| P2-8 | 형제 사이 순서를 저장하지 않음 (2026-10-01 추가) | `ecs/Hierarchy.cpp:87` `GetChildren()`과 `editor/hierarchy_model.py:51` `order_for_tree()`는 자식을 id 오름차순(= 생성 순서)으로 정렬한다. Scene Hierarchy에서 형제 사이에 끌어다 놓아도 부모만 바뀌고 순서는 그대로다(`hierarchy_model.py:76` `resolve_drop_parent` 주석). 프리팹 v2의 `entities` 배열 순서는 저장·복원되지만, 스폰하면 id 순이 곧 배열 순이라 우연히 유지될 뿐이다. 다른 엔티티를 끼워 넣으면 그 순서는 사라진다. | 지금은 순서가 동작에 영향을 주지 않는다(그리기와 변환은 순서와 무관). UI 계층(`INGAME_UI_CUSTOMIZATION_PLAN.md`)은 형제 순서가 곧 그리기 순서라 그때 필요해진다. 방법: `HierarchyComponent`에 `siblingIndex`(int)를 추가하고, `GetChildren()`이 그 값으로 정렬한다(같으면 id로). 드롭의 위/아래 위치로 인덱스를 정하고, 이 변경도 `SetParentCommand`처럼 Undo 가능하게 한다. 프리팹 캡처는 이미 배열 순서로 저장하므로, 스폰할 때 인덱스를 채우기만 하면 된다. |

---

## 2. 리뷰의 강점 평가 중 확인된 것

- C++23(`CMakeLists.txt:9`), `std::expected` 기반 수명주기, `Initialize` / `InitializeFromWindowHandle` / `TickFrame` 분리
- `EntityMeta`의 EntityID와 UUID 분리(`Entity.h:31-33`)
- Job System: `WorkStealingDeque`, `DependencyResolver`, `ValidationLayer`
- RenderGraph의 가상/물리 핸들 분리, `ExecuteParallel`
- `engine_binding.py` 단일 진입점, Command와 Transaction 기반 Undo/Redo, Motion Mixer
- **정정:** "Sparse Set 기반"은 절반만 맞다. dense 배열은 연속이라 순회는 캐시 친화적이지만, sparse 쪽은 `std::unordered_map`이다(`ComponentArray.h:220`). 엔티티 단위 조회는 해시 비용을 치른다. 조회가 병목이 되면 배열 기반 sparse로 바꾼다.

---

## 3. 제외한 지적과 이유

| 리뷰의 주장 | 판정 | 이유 |
|---|---|---|
| "RenderGraph 자원 상태 추적을 구현했습니다"(`ResourceState` enum, Barrier의 `srcState/dstState`, `ResourceBarrier(h, src, dst)`) | **사실 아님** | 저장소 어디에도 없다(커밋과 작업 트리 모두). 설계 아이디어만 P2-1에 반영했다. |
| "D3D12 리소스 상태 관리를 구현·연동했습니다"(`renderer/d3d12/`, `D3D12ResourceStateTests.cpp`) | **사실 아님 + 현 시점 제외** | 해당 파일이 없다. 또 D3D12 백엔드 자체가 없으므로(`renderer/opengl/`만 존재) 상태 트래커를 먼저 만드는 것은 순서가 맞지 않는다. 백엔드 착수 시 재검토한다. |
| Play Mode `RegisterClass` 중복 등록이 "아직 남아 있을 가능성 높음"(P0) | **이미 해결** | `ERROR_CLASS_ALREADY_EXISTS` 재사용 처리(`5c951c4`). 그 뒤 드러난 Play Mode 크래시도 수정했다(`449f868`). 실제 에디터에서 Play/Stop/Motion Editor 전환 시 크래시가 없음을 확인했다. 화면 렌더링은 미확인이다. |
| "테스트 스위트에 항상 5개 실패" | **갱신됨** | 4개 복구(`ed83874`). 남은 1개는 P0-1로 반영했다. |
| "Reflection과 인스펙터 연동 깊이가 제한적일 수 있음" | **틀림** | Inspector는 `GetComponentSchema`로 등록된 모든 컴포넌트의 편집 위젯을 동적으로 만든다(`inspector.py:101,488`). |
| "`pybind11::embed` + Qt 혼합 시 DLL/초기화 순서 이슈" | **해당 없음** | embed는 링크만 되고 사용되지 않는다(에디터가 Python 호스트이고 엔진은 확장 모듈). 실제 문제는 불필요한 링크이며 P2-4로 바꿔 반영했다. |
| "OpenGL 백엔드만 존재 → Vulkan/DX12 대비" | **사실이지만 우선순위 제외** | 사실이다. 다만 메시 파이프라인(P0-2)도 없는 지금 백엔드를 추상화하는 것은 이르다. |
| "Components.h include 순서 의존" | **이미 해결** | `<string>` 직접 include로 고쳐졌고, 주석으로 기록돼 있다(`Components.h:4-7`). |
| 종합 점수(7.4/10 등) | **제외** | 코드로 검증할 수 없는 주관적 평가다. |
