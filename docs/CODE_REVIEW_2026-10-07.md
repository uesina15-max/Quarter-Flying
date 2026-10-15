**Quarter Flying 전체 코드 리뷰**

- **기준**: 로컬 작업본 `C:\QuarterFlying` (HEAD `980dada` + 커밋 안 된 변경, 2026-10-07)
- **개정**: 2026-10-07 — 코드와 직접 대조해 사실 관계를 고치고, 표현을 다듬음
- **규모**: C++ 헤더/소스 **253개**(그중 `engine/tests/`의 C++ 테스트 **54개**), Python 에디터 파일 **41개**(그중 `test_*.py` **11개**)

> **읽는 법**
> - **(실행 확인)**: 실제로 실행해 결과를 봤다.
> - **(코드 확인)**: 소스를 읽어 확인했다. 실행하지는 않았다.
> - **점수**: 리뷰어의 주관 평가(10점 만점)다. 측정값이 아니다.
> - 원문은 "이전 리뷰 대비" 개선을 언급하지만, 이전 리뷰의 시점과 위치는 기록되어 있지 않다.

이전 리뷰 이후 반영된 것: 계층(Hierarchy) 컴포넌트, 메시 경로(`meshPath`), AssetManager 연동, 카메라 리그, 다중 엔티티 프리팹, `ResourceState` 기반 배리어.

> **2026-10-07 개정 기록** — 원문이 추정·일반화했거나 이미 해소된 내용을 고쳤다.
> 1. **테스트 "5개 상시 실패"는 이미 해소되었다(실행 확인).** `SimpleEngineTests.exe` 실행 결과 **556개 전부 통과**. 원문 §5의 P0 항목 "실패 테스트 정리"를 지웠다. ROADMAP 본문에 남은 "406 통과 / 5 실패" 같은 기록은 그 시점의 이력이다.
> 2. **RenderGraph가 프레임에 기여하지 않는다는 것은 추정이 아니라 사실이다(코드 확인).** 테스트 밖에서 RenderGraph에 패스를 등록하는 코드가 없다. 그래서 `Renderer::Render()`는 매 프레임 `GetPassCount() == 0`에서 반환한다. 실제 화면은 그 앞의 즉시 모드 호출(화면 지우기 → 그리드 → 본 라인 → 기즈모 → 씬 메시 → 파티클)이 그린다.
> 3. **계층 부모 변경은 대부분 `SetParent` 경로로 통일되어 있다(코드 확인).** Inspector는 `HierarchyComponent.parent` 편집을 `editor_api.set_parent`로 보낸다(`inspector.py:388`). 아직 필드를 직접 쓰는 곳은 두 곳이다: `editor_api`가 없을 때의 폴백(`SetComponentFieldJson`)과 `SetComponentJson` 바인딩. `ecs/Hierarchy.h:19` 주석은 아직 "Inspector가 직접 쓴다"고 적혀 있어 현재 코드와 맞지 않는다.
> 4. **JobSystem은 매 프레임 경로에서 쓰이지 않는다(코드 확인, 같은 날 정정).** 병렬 시스템 실행 경로(`SystemDispatcher`, `ParallelGroupBuilder`)는 있지만, `World`·`WorldManager`의 `parallelExecution` 기본값이 `false`이고 켜는 코드가 없다. 그래서 ECS 시스템은 `TickFrame`을 부른 스레드에서 순서대로 돈다. (이 문서의 첫 개정본은 "ECS 시스템 실행이 JobSystem 위에서 돈다"고 적었는데 틀렸다. 또 그때 `ScriptSystem`은 어떤 World에도 등록되어 있지 않았다 — Build Game 구현 중 연결함, `docs/GAME_BUILD_PLAN.md` §9.3.)
> 5. **LOD·Occlusion·DynamicResolution은 상태가 서로 다르다(코드 확인).** LOD와 프러스텀 컬링은 기본으로 켜져 매 프레임 돈다(`Renderer.cpp`의 `enableLOD(true)`, `enableFrustumCulling(true)`). 오클루전 컬링과 동적 해상도는 기본으로 꺼져 있다.
> 6. **전역 `include_directories` 제거는 엔진 타깃에만 적용되었다(코드 확인).** `engine/tests/CMakeLists.txt:6-7`에는 전역 `include_directories`가 남아 있다.
> 7. **AssetManager는 텍스처만 처리한다(코드 확인).** 메시는 `RenderSystem`이 `Mesh::loadFromFile`을 직접 호출한다(`RenderSystem.cpp:27`). 원문의 "AssetManager가 유일한 입구는 아니다"를 "메시는 AssetManager를 전혀 거치지 않는다"로 구체화했다.

---

### 1. 한 줄 요약

**설계 의도와 문서화 수준은 개인·소규모 프로젝트 중 상위권**이고, "컴파일 통과"와 "실제 실행"을 구분하는 습관이 코드와 문서에 자리 잡혀 있다.
반면 유지보수 비용을 키우는 요인이 세 가지 있다. **Windows+OpenGL에만 묶인 구조**, **문서(특히 ROADMAP 앞부분)와 코드 사이의 시차**, **이중 경로·미완 연결**(대표적으로 프레임에 쓰이지 않는 RenderGraph)이다.

---

### 2. 모듈별 평가 (점수는 주관 평가)

| 모듈 | 점수 | 근거 |
|------|------|------|
| **Core / Engine** | 8.5 | `std::expected` 기반 에러 처리, 서브시스템 등록, HWND 임베딩, 게임/에디터 카메라 분리가 명확하다. |
| **ECS** | 8.0 | Sparse-set 저장소, Hierarchy, Reflection, RenderSystem과 카메라 시스템들이 실제로 쓰이는 수준이다. |
| **Job System** | 8.0 | Work-stealing, 작업 간 의존성, 검증 기능까지 있다. 다만 World의 병렬 시스템 실행이 기본으로 꺼져 있어(`parallelExecution(false)`) 매 프레임 경로에서는 쓰이지 않는다. 시스템 대부분도 `CanRunInParallel() == false`다 |
| **RenderGraph** | 7.5 | 리소스 수명 관리·메모리 재사용(alias)·상태 기반 배리어(`Barrier{src,dst}`)가 있다. 그러나 **게임 실행 중 등록되는 패스가 0개**라 테스트로만 동작이 확인된 모듈이다. |
| **Renderer (OpenGL)** | 7.0 | 인스턴싱, 그리드, 본 라인, 파티클, `meshPath` 로드가 연결되어 있다. 실제 프레임은 `Renderer::Render()`의 즉시 모드 호출 순서로 그린다. 백엔드 추상화는 얇다. |
| **Animation / Motion Mixer** | 8.5 | 계약서 기반으로 구현했고 테스트가 가장 탄탄하다. |
| **Prefab** | 7.5 | 다중 엔티티 프리팹(v2)과 Hierarchy 연동이 있다. 에디터 사용성과 예외 상황은 계속 검증해야 한다. |
| **Asset** | 7.0 | `Engine`에 등록되어 텍스처 로드에 쓰인다. 메시는 이 경로를 거치지 않는다(`Mesh::loadFromFile` 직접 호출). |
| **Platform** | 6.0 | Win32만 있다(`PlatformFactory`가 Windows가 아니면 `nullptr`을 반환). README가 "Windows 전용"이라고 명시한 점은 좋다. |
| **Python 에디터** | 8.0 | 엔진 모듈을 `engine_binding` 한 곳에서만 불러오고, 엔진 없이도 뜨는 더미 모드(`HAS_ENGINE=False`)가 있고, 패널이 잘 나뉘어 있다. |
| **테스트·문서** | 8.5 | C++ 테스트 556개가 전부 통과한다(실행 확인). CLAUDE.md 관례와 계획 문서들이 강점이다. ROADMAP 앞부분에는 낡은 서술이 섞여 있다. |

**종합: 약 7.7 / 10** (주관 평가. 이전 리뷰보다 높게 본 이유: Hierarchy·메시 경로·에셋·카메라 기능이 코드로 뒷받침됨)

---

### 3. 잘된 점

**1) 아키텍처 경계가 문서화되어 있다**
- 핸들 종류(논리 핸들 / RenderGraph 핸들 / GPU 핸들)를 섞지 말라는 규칙이 `core/Types.h` 주석에 명시되어 있다.
- RenderGraph의 `ResourceState`, `Barrier{src,dst}`, `GetBarriers()`로 배리어 계산을 테스트할 수 있다.
- 게임 카메라와 에디터 카메라(`ViewCamera`)를 나눈 이유가 `core/Engine.h`에 기록되어 있다.

**2) ECS가 데모 수준을 넘어섰다**
- `Hierarchy` + `SetParent`: 순환 부모 지정을 거절하고, `keepWorldTransform` 옵션이 있다.
- `RenderableComponent.meshPath` / `texturePath`로 파일 경로를 직접 지정할 수 있다.
- 카메라 우선순위·전환 블렌드, ActionPlayer(Sound Lite), 프리팹 연동이 있다.

**3) 품질 문화**
- CLAUDE.md: "컴파일도 되고 크래시도 없지만 조용히 틀리는 버그"의 증상과 원인을 코드 주석에 남기는 관례가 있다.
- `engine_binding.to_entity()`가 pybind11 타입 변환 실수를 막는다.
- CMake: 엔진 타깃은 전역 `include_directories` 대신 `target_include_directories`를 쓴다(테스트 CMake에는 전역 지정이 남아 있음).

**4) 실제 장애를 고친 기록**
- `RegisterClassExW`가 `ERROR_CLASS_ALREADY_EXISTS`를 반환하면 기존 창 클래스를 재사용하도록 고쳤다(두 번째 뷰포트 초기화 실패 해결).
- Qt 네이티브 뷰포트 위에 스타일시트가 덧칠되는 문제를 찾아 고쳤다.
- AssetManager를 `Engine` 초기화 때 등록하도록 했다.

---

### 4. 약점 · 위험

**1) 플랫폼·그래픽스가 한 가지 경로뿐이다**
- `platform/`은 Win32만 있다. Windows가 아니면 `PlatformFactory::CreatePlatform()`이 `nullptr`을 반환한다.
- 렌더 백엔드는 OpenGL뿐이다. D3D12/Vulkan 경로와 리소스 상태 추적기가 없다.
- `ResourceState`의 값은 7개(Undefined, Common, RenderTarget, DepthWrite, DepthRead, ShaderResource, UnorderedAccess)뿐이다. Copy·Present 상태가 없어 D3D12 상태를 그대로 옮기기에는 부족하다.

**2) 이중 경로·남은 잔재**
- 메시 지정에 `meshHandle`(uint32)과 `meshPath`(문자열)가 함께 있다. `meshPath`가 비어 있지 않으면 우선한다는 규칙은 주석에 있지만, API만 보면 헷갈린다.
- `AIComponent`는 리플렉션에 등록되어 저장은 되지만, **읽는 시스템이 없다.** 액션 재생을 ActionPlayer로 분리한 것은 의도된 설계지만, AI 쪽 필드는 쓰이지 않는 데이터로 남았다.
- ROADMAP 한 파일 안에 시점이 다른 서술이 섞여 있다. §1 "현재 상태 (검증됨, 2026-08-10)"~§5의 "미검증/부재" 서술과 §6 이후의 "완료" 서술이 함께 있다. 예: §4 "미해결"에는 아직 "scene.json → 실제 ECS 엔티티 생성 브리지 미구현"이 남아 있지만, CLAUDE.md에는 2026-09-30에 해결하고 화면으로 확인했다고 기록되어 있다. 처음 보는 사람은 어느 쪽이 현재 상태인지 판단하기 어렵다.
- 코드 주석에도 같은 시차가 있다. `Renderer.cpp:297`은 "현재는 아무도 패스를 채워넣지 않는다 - RenderSystem 부재"라고 적혀 있다. 앞부분(패스 없음)은 지금도 사실이지만, RenderSystem은 이제 존재한다.

**3) RenderGraph와 실제 프레임이 따로 논다**
- RenderGraph의 컴파일·배리어 계산 기능은 있지만, **게임 실행 중 패스를 등록하는 코드가 없어** 매 프레임 `GetPassCount() == 0`에서 반환한다(코드 확인). 실제 화면은 즉시 모드 경로(화면 지우기, 그리드, 본 라인·기즈모, ECS 메시 배치, 파티클)가 전부 그린다.
- OpenGL의 `ResourceBarrier`는 Trace 로그만 남기고 아무것도 하지 않는다. 그래서 상태 추적이 아직 실제 동작에 영향을 주지 않는다.

**4) 제품 범위에 비해 인프라가 넓다**
- Job/LOD/Occlusion/DynamicResolution 같은 "엔진급" 모듈이 많다. 이 중 LOD와 프러스텀 컬링은 기본으로 켜져 매 프레임 돌고, 오클루전 컬링과 동적 해상도는 기본으로 꺼져 있다.
- 게임 콘텐츠·도구의 완성도에 비해 인프라 범위가 넓어서, 회귀와 문서 시차의 비용이 크다.

**5) 에디터와 엔진의 결합**
- HWND 임베딩 + `TickFrame` 방식은 타당하다. 하지만 스레드·창 크기 변경·다중 뷰포트에서 계속 문제가 생기기 쉽다. `TickFrame` 재진입 가드와 GL 컨텍스트 전환 수정 이력이 그 예다.
- 계층 부모 변경은 Inspector까지 `SetParent` 경로로 통일되었다. 하지만 `editor_api`가 없을 때의 폴백과 `SetComponentJson` 바인딩은 아직 필드를 직접 쓴다. 이 경로에는 순환 검사와 Undo가 없다. 대신 계층을 따라 올라가는 함수는 `kMaxHierarchyDepth`에서 멈추고 경고를 남긴다.

---

### 5. 우선 개선 제안

| 순위 | 항목 | 이유 |
|------|------|------|
| **P0** | ROADMAP 앞부분을 "현재 상태" 표 하나로 다시 쓴다 | ROADMAP이 의사결정의 근거인데 시차가 크다(예: 해결된 scene.json 브리지가 "미구현"으로 남아 있음). |
| **P1** | `meshHandle` / `meshPath`를 하나로 합친다 | 호출부·직렬화·Inspector가 단순해진다. **2026-10-15 해결**(`USABILITY_IMPROVEMENT_PLAN.md` R-4): `RenderableComponent` 스키마 2에서 `meshHandle`·`materialHandle`을 빼고 경로 필드(`meshPath`·`materialPath`·`texturePath`)로 통일했다. 코드가 만든 메시는 `RegisterMesh(이름)`으로 등록하고 `meshPath`에 그 이름을 쓴다. |
| **P1** | RenderGraph를 실제 프레임 경로에 넣을지, 테스트 대상 인프라로만 둘지 정한다 | 지금은 게임 실행 중 패스가 0개다. 그래프와 즉시 모드가 이중으로 존재하는 상태를 정리한다. |
| **P2** | 남은 계층 직접 쓰기 경로(폴백, `SetComponentJson`)를 정리하고 `Hierarchy.h` 주석을 현재 코드에 맞춘다 | Inspector 경로는 이미 통일되었다. |
| **P2** | `ResourceState`에 Copy·Present 등을 추가하고, D3D12 상태 매핑 헤더를 둔다 | 백엔드 확장 준비. 이전에 논의된 D3D12 매핑 설계가 있다면 그것과 맞춘다(문서 위치는 기록되어 있지 않음). |
| **P2** | 메시·애니메이션 클립도 AssetManager를 거쳐 불러오게 한다 | 지금 메시는 `Mesh::loadFromFile`을 직접 호출한다. 파일 로딩 경로가 흩어지는 것을 막는다. |
| **P3** | 기본으로 꺼진 Occlusion/DynamicResolution의 상태를 문서에 표시한다 | `docs/SURPLUS_FILES.md`·`docs/archive/UNUSED_AND_LEGACY_REPORT.md`의 기록과 코드를 일치시킨다. |

> 원문의 P0 "테스트 스위트 상시 실패분 정리"는 지웠다. 2026-10-07 실행 결과 556개 전부 통과했다(실행 확인).

---

### 6. 아키텍처 다이어그램 (실제로 쓰이는 경로 기준)

```text
[PySide6 에디터]
    engine_binding → quarterflying.pyd
         │
         ▼
    Engine (TickFrame / PushInputEvent)
         ├─ Win32Platform (GL 컨텍스트, Present)
         ├─ JobSystem ── 초기화는 되지만 매 프레임 경로에서 쓰이지 않음 (World 병렬 실행 꺼짐)
         ├─ Renderer  ── 즉시 모드: 화면 지우기 → 그리드 → 본 라인/기즈모 → 씬 메시 → 파티클
         │               (RenderGraph: 생성·컴파일 코드는 있으나 게임 실행 중 패스 0개)
         ├─ AssetManager (텍스처만)
         ├─ World / ECSRegistry
         │     시스템: Render, Camera, CameraRig, CameraGizmo, Particle, Script(2026-10-07 연결)
         │     Hierarchy, 프리팹 생성/되돌리기
         └─ MotionPreviewState (Motion Editor)
```

JobSystem과 RenderGraph는 둘 다 구현·테스트는 되어 있지만 매 프레임 경로에서는 쓰이지 않는다. 둘을 실제 프레임 경로에 넣을지(또는 범위를 줄일지)가 다음 설계 정리 대상이다.

---

### 7. 총평

이 작업본은 "엔진 뼈대만 있는 저장소"가 아니다. **에디터에서 메시·계층·프리팹·모션·카메라를 직접 다뤄 볼 수 있는 단계**까지 와 있다.
강점은 **의도를 설명하는 주석, 버그의 원인을 남기는 습관, Motion/Prefab/ECS의 실제 구현, 전부 통과하는 테스트 스위트**다.
약점은 **단일 플랫폼, 문서 시차, API 이중성, 프레임에 쓰이지 않는 인프라(RenderGraph)를 포함한 넓은 범위**다.

바로 손대면 효과가 큰 것:
1. ROADMAP "현재 상태" 표 현행화
2. 메시 경로 API 정리
3. RenderGraph의 역할 결정(실제 프레임 경로에 넣거나, 범위를 줄이거나)
4. (원하면) `ResourceState` 위에 D3D12 상태 매핑 추가

특정 모듈(RenderGraph, ECS Hierarchy, Prefab, 에디터 바인딩, JobSystem)을 더 깊게 보고 싶으면 모듈 이름을 지정해 주세요.
