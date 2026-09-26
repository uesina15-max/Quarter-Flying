# Quarter Flying — 개발 마스터플랜

**작성일**: 2026-08-18
**기준**: `C:\QuarterFlying` (정본 경로 — `D:\Quarter Flying`는 구버전 사본, [CLAUDE.md](../CLAUDE.md) 참고)

## 이 문서와 다른 계획 문서의 관계

이 저장소에는 계획 문서가 여러 층으로 존재한다. 서로 대체하지 않고 각자의 역할이 다르다:

- **이 문서(`docs/MASTER_PLAN.md`)** — 제품/전략 관점의 장기 우선순위. "기술 데모에서 실사용 가능한 제작 도구로" 가기 위해 뭘 순서대로 만들어야 하는가.
- **[ROADMAP.md](../ROADMAP.md)** — 세션별로 실제 검증된 진행 기록(ground truth). "지금 뭐가 실제로 되고 뭐가 안 되는가"를 실행 결과·스크린샷·테스트 통과 여부로 기록한다. 이 마스터플랜의 각 항목이 실제로 착수되면 ROADMAP.md에 상세 실행 기록이 남는다 — **상태를 확인할 때는 이 문서가 아니라 ROADMAP.md를 먼저 봐야 한다.**
- **[docs/MOTION_MIXER_IMPLEMENTATION_PLAN.md](MOTION_MIXER_IMPLEMENTATION_PLAN.md)**, **[docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md](PYTHON_BINDING_IMPLEMENTATION_PLAN.md)** — 개별 기능의 상세 설계/Phase 분해. 이 마스터플랜에서는 "P1.5" 한 줄로만 요약되는 작업의 실제 API 시그니처·JSON 포맷·테스트 명세가 여기 있다.
- **`PLAN.md`, `PROBLEM_ANALYSIS_REPORT.md`, `UNUSED_AND_LEGACY_REPORT.md`(저장소 루트)** — 더 오래된 진단 문서. ROADMAP.md §3(P3)에 이미 기록돼 있듯, 이번 세션들에서 검증된 사실과 상당 부분 어긋나거나 이미 해소됐을 가능성이 있어 우선순위가 낮다.

이 문서 자체는 사용자가 제시한 외부 리뷰 정리본을 기반으로 하되, 저장소를 직접 조사해 확인 가능한 부분(§6.1, §6.2)은 실측으로 교정했다.

---

## 0. 한 줄 정의

> **"기술 데모"에서 "게임을 끝까지 만들어 뽑아낼 수 있는 도구"로 넘어가는 것.**
> 판정 기준은 단 하나 — *외부 개발자가 이 도구로 6개월 안에 Steam에 출시할 수 있는가.*

**End-to-End 파이프라인 (닫혀야 할 루프)**
`프로젝트/씬 생성` → `에셋 임포트` → `머티리얼·오브젝트 배치` → `콜라이더/컨트롤러/UI/오디오` → `플레이 & 디버그` → `.exe 빌드`

---

## 1. 현재 좌표 — 어디까지 왔나

| 구분 | 상태 |
|---|---|
| **확보** | 엔진 코어(메인 루프·서브시스템), ECS(리플렉션/JSON 직렬화), Work-Stealing Job System, RenderGraph + OpenGL 백엔드, PySide6 에디터(Scene/Play/Motion), pybind11 바인딩, `scene.json` 기반 저장, **ECS→실제 draw call 파이프라인(RenderSystem + InstancedBatchManager, 2026-08-18)**, **Motion Mixer(스켈레톤/키프레임/레이어 블렌딩/믹서 UI, 2026-08-15)** |
| **미확보** | 완성 게임 패키징 흐름, 실무형 에셋 파이프라인(메타데이터·참조 추적), 프리팹 계층(§6.2 참고 — 현재 구현률 0%), 제작자용 플레이 디버깅 UX |
| **부채** | 빌드 아티팩트 저장소 혼재 · FetchContent 과의존/CI 취약 · 테스트 커버리지 ~60%(373개 테스트, 정확한 커버리지 수치는 미측정) · CommandList race condition · **같은 프로세스 안 여러 GL 컨텍스트가 뒤섞이는 문제(2026-08-18 신규 발견 — §6.3)** |

**진단**: 병목은 "기능 개수"가 아니라 **제작 파이프라인이 아직 닫히지 않았다**는 점. 기반기는 이미 데모 수준을 넘었다 — 특히 렌더링 파이프라인(ECS→화면)과 Motion Mixer는 2026-08-15~18 사이에 "미검증"에서 "실제 동작 확인"으로 넘어갔다(ROADMAP.md §5·§6, 부록 B~F 참고).

> ~~C++ GUI와 Python/Qt GUI 혼재~~는 부채 목록에서 제외했다 — §6.1 참고 (이미 2026-07-28에 PySide6 단일화로 결론남).

---

## 2. 통합 우선순위 매트릭스

원문의 두 표를 합치면 다음 순서가 된다. 핵심 조정은 **"기반 정비(구조·CI)"를 P0 앞단에 두고, Core Runtime과 함께 묶은 것**이다.

| 순위 | 축 | 항목 | 현재 상태 |
|---|---|---|---|
| **P0-a** | 유지보수 기반 | 저장소 구조 정리(아티팩트 제거), 문서 체계 통합, CI/CD, 테스트 커버리지 + Python smoke test | ⬜ 미착수 (테스트 스위트 자체는 373개 존재·통과, CI 자동화는 없음) |
| **P0-b** | Core Game Runtime | 에디터 분리형 독립 런타임 패키징, 씬 라이프사이클(`LoadScene`), 프리팹 인스턴스, ECS 컴포넌트/스크립팅, 물리·입력 매핑, 기본 에셋 임포터 | ⬜ 대부분 미착수. ECS 컴포넌트/스크립팅은 `ScriptSystem`(Python 라이프사이클 훅 + hot reload)로 일부 확보. **ECS→실제 렌더링 경로는 2026-08-18에 확보됨**(ROADMAP.md §6) — 이 항목은 원래 P0-b 소관이 아니라 렌더링 기반(§2의 "3D 뷰포트 실제 렌더링" P0 블로커) 쪽이었는데, 결과적으로 "기본 에셋 임포터"의 전제 조건이 이걸로 갖춰졌다 |
| **P1** | Production Pipeline | **배포 빌드/패키징**, **에셋 import/reimport + 참조 관리**, **플레이 디버그·프로파일러**, 애니메이션 스테이트 머신, UI 시스템(Canvas), 3D 오디오, 머티리얼 에디터, Save/Load | ⬜ 미착수. 단, "애니메이션 스테이트 머신"의 하위 기반(스켈레톤/키프레임/레이어 블렌딩)은 Motion Mixer Phase 1~5로 이미 확보(ROADMAP.md §P1.5) — 남은 건 상태 전이 그래프(FSM) 자체 |
| **P2** | Tool UX & 차별화 | Python API 강화(핫 리로드·에러 위치 표시), Undo/Redo, 에셋 브라우저, 프리팹 베리언트, 프레임 프로파일러, GPU Instancing / BVH Culling | Undo/Redo는 `CommandManager`로 이미 존재(범위 확인 필요). **GPU Instancing은 2026-08-18에 실제로 연결·검증됨**(`InstancedBatchManager` + `RenderSystem` + `SceneMeshRenderer`, 실제 큐브 렌더링 스크린샷 확인) — 아래 참고 |
| **P3** | Advanced | NavMesh, Terrain, 네트워킹, Vulkan, 멀티플랫폼 확장 | ⬜ 미착수 |

> ⚠️ **원문의 재배치 노트를 다시 교정함**: 원 리뷰는 "GPU 렌더링 최적화가 P3에 있는데 인스턴싱은 이미 착수 검토 단계이니 P2로 당겨야 한다"고 지적했다. 이후 2026-08-18 세션에서 `InstancedBatchManager`(이전까지 실행된 적 없는 dead code였음)가 `RenderSystem`을 통해 실제로 연결되어, ECS 엔티티가 인스턴스 배치로 그려지는 것까지 확인됐다(ROADMAP.md §6). 즉 "당겨야 한다"가 아니라 **"이미 P0-2의 부산물로 최소 구현이 끝났다"**로 갱신해야 정확하다 — 다만 프러스텀 컬링과의 상호작용, 대량 인스턴스 성능은 아직 실측 전이라 P2의 "최적화·확장" 관점 작업은 여전히 유효하다.

---

## 3. 8대 핵심 시스템 (요구사항 원장)

이 표는 "현재 얼마나 됐는가"가 아니라 **v1.0에 필요한 요구사항 목록**이다. 항목별 실제 구현 상태는 착수 시점에 ROADMAP.md에서 확인한다.

| # | 시스템 | 필수 요구 |
|---|---|---|
| 1 | 씬 & 레벨 에디팅 | 드래그앤드롭 씬 그래프, 프리팹, 레이어/태그 필터, Undo/Redo, **텍스트 기반 직렬화(Git 충돌 예방)** |
| 2 | 게임 로직 & 스크립팅 | Python 라이프사이클(`on_update`/`on_collision`), 핫 리로드, 비주얼 스크립팅, 이벤트/시그널 |
| 3 | 에셋 파이프라인 | glTF/FBX/OBJ, 텍스처 압축·아틀라스, 스켈레톤/블렌드 트리, 3D 사운드, SDF 폰트, **메타데이터·참조 추적·이동 시 참조 유지** |
| 4 | UI 시스템 | Canvas 해상도 대응, WYSIWYG 배치, Button/Slider/Text, 9-slice |
| 5 | 물리 & 게임플레이 | RigidBody/Collider/Raycast, 1·3인칭 컨트롤러, 카메라 스택, 파티클, 타임라인 |
| 6 | 빌드 & 배포 | 원클릭 exe, 미사용 에셋 제거, 릴리즈 최적화, CLI 빌드(CI 연동) |
| 7 | 디버깅 & 프로파일링 | 인게임 콘솔(`~`), CPU/GPU 프로파일러, ECS 디버거, 필터 가능한 로그, 충돌/트리거 시각화 |
| 8 | 프로젝트 관리 & 협업 | 장르별 템플릿, Git LFS, 바이너리 에셋 잠금 |

참고: #2의 "Python 라이프사이클 핫 리로드"는 `engine/ecs/ScriptSystem.cpp`에 이미 골격이 있다(모듈 reload + `on_create`/`on_update`/`on_destroy` 훅, `dirty` 플래그 기반 재인스턴스화). #3의 OBJ 로딩은 `tinyobjloader` 경유로 이미 가능(`engine/renderer/Mesh.cpp`)하지만 glTF/FBX·메타데이터·참조 추적은 없다. #5의 "파티클"은 범위/철학을 [docs/VFX_LITE_PLAN.md](VFX_LITE_PLAN.md)(2026-08-20)에서 미리 좁혀뒀다 — 범용 VFX 툴이 아니라 18개 값짜리 최소 효과 시스템으로 한정하고, Collision/Sub-emitter/GPU Particle 등은 명시적으로 제외. 성능 철학도 확정("파티클을 최적화" 대신 "비싸질 경로를 안 만듦" — 파티클은 ECS 엔티티가 아님, Max Particle 초과 시 신규 spawn 거부, 프레임별 동적 할당 금지 등). 아직 구현 계획서 단계는 아니고 범위 확정 문서만 있는 상태.

---

## 4. 로드맵

| 단계 | 기간 | 검증 게이트 (통과 조건) |
|---|---|---|
| **MVP** | 1~2개월 | 씬 저장/로드 + 3D 뷰포트 + 프리팹 기초 + Python 핫 리로드 |
| **Alpha** | 3~4개월 | glTF 임포터 + 물리 + 기본 UI + **.exe 빌드 내보내기** |
| **Beta** | 5~6개월 | 비주얼 스크립팅 + 프로파일러 + 파티클 + 애니메이션 SM |
| **v1.0** | 8~12개월 | 멀티플랫폼 + 패치 시스템 + **실제 개발자 제작 게임 1종 출시** |

---

## 5. 포지셔닝

범용 AAA 엔진 경쟁이 아니라 —

> **"Python으로 게임 규칙을 빠르게 붙여, 실제 플레이 가능한 게임을 만드는 경량 제작 도구"**
> (게임잼 / 인디 / 프로토타입 / 툴 제작)

근거: C++ 코어 + pybind11 + PySide6 에디터 + 씬/플레이/모션 편집 구조가 이미 갖춰져 있어 **차별점이 렌더러가 아니라 스크립팅 UX**에 있기 때문.

---

## 6. 지금 바로 만들 5개

1. **에디터 → 실행 파일 빌드/내보내기** ← 도구 가치가 여기서 발생
2. **프리팹 시스템** ← 반복 생산성의 전제 (§6.2 — 설계부터 시작해야 함, 기존 코드 없음)
3. **에셋 import/reimport + 참조 관리**
4. **Play Mode 디버그/로그/프로파일러**
5. **Python 게임플레이 컴포넌트 템플릿**

---

## 6.1 GUI 이원화 — 이미 결론 났음 (원문의 "미해결 지점" 교정)

원문 리뷰는 "Native C++ GUI와 Python/Qt GUI 혼재가 P0-a에서 방향을 확정해야 하는데 어느 표에도 없다"고 지적했다. 저장소를 확인한 결과 **이 결정은 이미 2026-07-28에 내려졌고 실행까지 끝났다**:

- [`docs/archive/legacy/FINAL_SINGLE_GUI_REPORT.md`](archive/legacy/FINAL_SINGLE_GUI_REPORT.md)("단일 GUI 통합 최종 보고서", 완료 상태) — **PySide6(Qt Widgets)를 단일 GUI로 선택**하고, 기존 GLFW 기반 독립 실행형 메인 앱을 제거(`BUILD_LEGACY_MAIN` CMake 플래그 뒤로 옵션화)해서 `engine/editor/main.py`(Python/PySide6)가 유일한 진입점이 되도록 정리함.
- 실제로 `engine/app/`(GLFW 메인)은 저장소에서 완전히 제거됐고(`engine/ARCHIVE_INFO.md`에 아카이브 경위 기록), ImGui 통합은 `docs/ARCHITECTURE_KO.md`에 설계 문서로만 언급될 뿐 실제 코드(`ImGuiLayer.h/cpp` 등)는 저장소 어디에도 없다 — 문서와 실물이 어긋난 사례(ROADMAP.md §3 P3의 "오래된 문서 불신뢰" 경고와 같은 종류).

**결론**: GUI는 PySide6로 이미 단일화되어 있고 재론할 필요 없음. 원문이 이 항목을 "미해결"로 본 건 `FINAL_SINGLE_GUI_REPORT.md`가 `docs/archive/legacy/`에 있어서 리뷰 작성 시점에 놓쳤을 가능성이 크다 — 문서 체계 정리(P0-a) 시 이 보고서를 `docs/archive/`에서 꺼내 이 마스터플랜에 링크해두는 정도만 하면 됨(아카이브 자체는 유지 — "완료된 결정의 기록"으로서 가치가 있다).

## 6.2 프리팹 — P0/P1 중복 표기 교정 + 구현 현황

원문 지적대로 "프리팹"이 P0-b("프리팹 인스턴스 시스템", 런타임)와 P1("프리팹 계층", 에디터 저작)에 중복 등장한다. 실제 구현 순서와 맞추려면 분리해야 한다:

- **P0-b: 런타임 프리팹 인스턴스** — 직렬화된 프리팹 데이터를 읽어 ECS 엔티티(들)로 스폰하는 최소 기능. `RenderSystem`(2026-08-18 완료)이 이제 ECS 엔티티를 실제로 그릴 수 있으므로, 이 위에 얹을 수 있는 상태가 됐다.
- **P1/P2: 에디터 저작 계층** — 프리팹 생성/편집 UI, 인스턴스↔원본 오버라이드 추적, 베리언트(P2의 "프리팹 베리언트"와 자연히 이어짐).

**구현 현황(실측, 0%)**: 저장소 전체에서 "프리팹" 관련 코드는 `engine/asset/AssetHandle.h`의 `AssetType` enum에 `Prefab` 값 하나뿐이고, 이 값을 참조하는 다른 코드가 전혀 없다(직렬화·ECS 컴포넌트·에디터 UI 전부 없음). 즉 프리팹은 **설계부터 시작해야 하는 완전 신규 기능**이다.

✅ **구현 계획 문서 작성 완료(2026-08-19)**: [docs/PREFAB_IMPLEMENTATION_PLAN.md](PREFAB_IMPLEMENTATION_PLAN.md) — Motion Mixer/Python Binding과 같은 형식(배경 실측 → 설계 결정 → Phase 분해 → 완료 기준)으로 작성. 핵심 결정: (1) 엔티티 계층(부모-자식) 컴포넌트가 ECS에 아예 없어서 v1은 **엔티티 1개짜리 프리팹**으로 범위를 좁히고 다중 엔티티/계층은 Phase 5 확장 과제로 미룸, (2) 기존 `ComponentRegistry`의 리플렉션 기반 직렬화(`SerializeRegistry`가 PIE 스냅샷에 쓰는 것과 동일한 함수)를 그대로 재사용해 `*.prefab.json` 포맷을 정의, (3) `engine/asset/`(AssetManager/AssetRegistry)은 CMake에는 있지만 어디에도 `#include`·바인딩되지 않는 고아 모듈임을 확인하고 프리팹은 이를 우회(파일 스캔 방식), (4) Undo/Redo는 기존 `EditorAPI`/`Transaction`/`CommandManager` 계층을 그대로 상속.

## 6.3 신규 부채 — 다중 GL 컨텍스트 혼선 (2026-08-18 발견)

ROADMAP.md §6(버그 4)에서 발견: 같은 프로세스 안에 `EngineViewport`(=`Engine` 인스턴스, 자기 GL 컨텍스트)가 여러 개 존재(Scene Editor용 + Play Mode용, Motion Editor는 뷰포트 공유라 별도 컨텍스트 없음)하는데, `wglMakeCurrent`가 스레드 단위 상태라서 한쪽이 초기화되면 다른 쪽이 그리는 시점에 엉뚱한 컨텍스트가 "현재"로 걸려 있을 수 있다. `glViewport` 문제는 매 프레임 재적용으로 증상만 막았지만(`Renderer::SetViewportSize`), **셰이더/텍스처/VAO 바인딩 등 다른 컨텍스트별 상태도 같은 방식으로 오염될 수 있다는 근본 문제는 그대로**다. Play Mode·Motion Editor를 동시에/연속으로 쓰는 시나리오가 늘어날수록(P1의 "플레이 디버그" 작업과 직접 충돌) 이 문제를 먼저 정리하는 게 맞다 — P0-a(기반 정비) 또는 P1 착수 전에 재검토 권장.

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- ROADMAP.md 전체(§1~§6, 특히 §6 — RenderSystem/InstancedBatchManager 실측 검증 기록)
- `docs/archive/legacy/FINAL_SINGLE_GUI_REPORT.md` (GUI 단일화 결정 기록)
- `engine/asset/AssetHandle.h` (프리팹 구현 현황 확인)
- `engine/ecs/ScriptSystem.cpp` (Python 핫 리로드 기존 구현 확인)
- `engine/ARCHIVE_INFO.md` (레거시 GLFW 메인 제거 경위)
