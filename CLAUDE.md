# CLAUDE.md

## 작업 경로 (중요)

**이 저장소의 정본(canonical) 작업 경로는 `C:\QuarterFlying`입니다.**

`D:\Quarter Flying` (공백 포함, USB/외장 드라이브)는 2026-08-10에 `C:\QuarterFlying`으로 복사(`build*` 제외)하기 전의 **구버전 사본**입니다. `D:\Quarter Flying`에는 git이 없고, 이후의 모든 커밋·수정 이력은 `C:\QuarterFlying`에만 존재합니다.

- **앞으로 이 프로젝트에 대한 모든 읽기/쓰기/빌드는 `C:\QuarterFlying` 기준으로 하십시오.**
- `D:\Quarter Flying`을 참조하지 마십시오 — 코드가 어긋나기 시작하면 어느 쪽이 최신인지 판단할 근거(git 이력)가 D에는 없습니다.
- 세션의 기본 작업 디렉터리가 `D:\Quarter Flying`으로 설정되어 열리더라도, 이 프로젝트 관련 작업은 반드시 `C:\QuarterFlying` 절대 경로를 명시해서 진행하십시오.
- CMake 빌드 디렉터리는 `C:\QuarterFlying\engine\build` 입니다 (Visual Studio 17 2022 / x64, `--config Release` 사용 — Debug는 `python3XX_d.lib`가 보통 설치돼 있지 않아 링크 단계에서 실패합니다).

## 빌드 환경 메모

- CMake 4.3.3, Git, VS Build Tools 2022/2026, Python 3.14는 이미 설치되어 있으나 기본 PATH에는 없었습니다. 사용자 PATH에 `C:\Program Files\CMake\bin`을 추가해 두었습니다(새 터미널부터 적용). Git은 GitHub Desktop 번들(`...\GitHubDesktop\app-*\resources\app\git\cmd`)을 사용 중이라 GitHub Desktop이 업데이트되면 경로가 바뀔 수 있습니다 — 독립 Git for Windows 설치를 권장합니다.
- pybind11 확장 모듈 `quarterflying`(CMake 타겟명, `.pyd` 파일명도 동일)은 `quarterflying_engine`(렌더러 포함 엔진 전체, CMake 공유 라이브러리 타겟)을 링크하므로, 렌더러 모듈이 컴파일되지 않으면 바인딩도 빌드할 수 없습니다. (2026-08-20: 옛 이름 `ge_python`/`ge_engine`에서 개명 — ROADMAP.md P1 Phase 6 참고. 에디터 파이썬 코드는 `engine/editor/engine_binding.py` 한 곳에서만 실제 모듈명을 참조하므로 다른 파일은 영향받지 않는다.)
- **Smart App Control 차단 (2026-09-30)**: 새로 빌드한 `quarterflying.*.pyd`가 import 시 `DLL load failed ... 애플리케이션 제어 정책에서 이 파일을 차단했습니다`로 막힐 때가 있다. 이벤트 로그 `Microsoft-Windows-CodeIntegrity/Operational`에 3033/3077("Enterprise signing level")로 남는다. 바이너리별 평판 판정이라 소스 변경 없이 다시 링크하면(예: `touch engine/bindings/PythonModule.cpp` 후 `quarterflying` 타겟 재빌드) 새 해시로 통과한 적이 있다. 보안 설정 자체를 끄지는 않는다.
- 엔진 코어(특히 `renderer/`)는 오랫동안 컴파일된 적이 없었던 것으로 보이는 다수의 선존재 버그를 포함하고 있었습니다. 자세한 내용은 `docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md`의 Phase 1 기록과 git 커밋 로그를 참고하십시오.

## 코드 품질 관례 (2026-08-29 확정)

이 세션에서 프리팹 시스템 버그 헌팅(Phase 4) 중 겪은 일 — Inspector가 실제 엔티티에서
"한 번도 정상 동작한 적이 없었다"는 사실이 컴파일도 되고 크래시도 없는 상태로 몇 커밋 동안
숨어 있었던 것 — 을 계기로, 앞으로 이 코드베이스 전체에 다음 3가지 관례를 표준으로 적용한다.

### 1. 버그 수정 주석: "무엇을 고쳤다"가 아니라 "왜 틀렸었는지 + 어떤 증상이었는지"

버그를 고칠 때 코드에 남기는 주석은 최소한 아래 두 가지를 포함해야 한다 ("고친 방법"만
적는 것은 불충분함 — git diff만 봐도 무엇을 고쳤는지는 이미 보이기 때문):

1. **증상** — 실제로 무엇이 잘못 보였는가. 에러 메시지가 있었다면 검색 가능하도록 **원문
   그대로** 인용한다. "왜 발견하기 어려웠는지"(예: "컴파일도 되고 크래시도 안 남")까지
   적으면, 다음 사람이 비슷한 증상을 볼 때 이 코드부터 의심할 수 있다.
2. **원인** — 코드가 실제로 무엇을 잘못 가정/처리하고 있었는가.

커밋 메시지가 아니라 **코드 안**에 남긴다 — git blame/log를 뒤지지 않아도 파일만 열면
보이게 하는 것이 목적이다.

이미 이 관례로 작성된 실제 사례(그대로 참고용 템플릿으로 삼을 것):

- [engine/renderer/Mesh.cpp](engine/renderer/Mesh.cpp) `drawInstanced()` — "예전엔 여기서 m_vao로
  다시 bind해버려서 그 인스턴스 attribute들이 전부 안 잡힌 상태로 그려지고 있었다. draw call
  자체는 GL 에러 없이 '성공'하기 때문에 화면에 아무 것도 안 보이는데도 원인 추적이 어려웠던
  버그."
- [engine/renderer/InstancedBatchManager.h](engine/renderer/InstancedBatchManager.h) `InstanceData` —
  "92는 16의 배수가 아니다(다음 경계는 96, 104가 아니다) — 원래 `_padding[3]`(12바이트)는
  104로 오버슈트해서 아래 static_assert를 실제로 평가할 수 있게 되자마자 걸렸다."
- [engine/editor/panels/scene_hierarchy.py](engine/editor/panels/scene_hierarchy.py) `refresh_from_engine()` —
  "`GetAllEntities()`는 `ge_python.Entity` 객체를 돌려주는데, 그걸 그대로 `EntityItem(entity_id: int, ...)`에
  넣으면 `Signal(int)`로 emit될 때 pybind11 객체가 조용히 0으로 뭉개진다(예외 없음, 발견하기
  매우 어려움)."
- [engine/editor/panels/inspector.py](engine/editor/panels/inspector.py) `_refresh_inspector()` —
  "entity_id(raw int)를 감싸지 않고 그대로 넘기면 pybind11이 'incompatible function arguments'로
  예외를 던져서, 아래 프리팹 헤더를 포함한 이 try 블록 전체가 항상 여기서 조용히 중단됐다 —
  즉 Inspector가 실제 엔티티에 대해 한 번도 정상 동작한 적이 없었다."

### 2. "컴파일 통과"와 "실제 실행 검증"은 문서에서 항상 구분한다

`ROADMAP.md`, `docs/*_PLAN.md`, PR/커밋 설명 등에서 작업 상태를 적을 때 아래 3단계를
섞지 않는다 — 특히 "컴파일된다"를 "동작한다"의 증거로 쓰지 않는다:

1. **컴파일/링크 성공** — 빌드만 통과한 상태. 실행해본 적 없음을 명시.
2. **단위 테스트 통과** — 격리된 로직은 검증됐지만, 실제 엔진/에디터에 연결해 돌려본 적은
   없음을 명시.
3. **실제 실행 검증** — 에디터를 `HAS_ENGINE=True`로 띄우거나 스크린샷 등으로 화면에서
   직접 확인한 상태. "크래시 없음"과 "의도대로 그려짐/동작함"도 서로 다른 주장이므로 구분한다.

이 구분은 이미 `CLAUDE.md`의 "알려진 버그"/"알려진 미검증 항목" 절과 이번 세션의 프리팹
Phase 1~4 기록(컴파일 → 유닛 테스트 → 실제 에디터 실행 3단계)에서 쓰던 방식을 그대로
표준으로 승격한 것이다. 새 기능을 "완료"로 표시하기 전에 위 3단계 중 어디까지 확인했는지
반드시 한 문장으로 남긴다.

### 3. 위험한 가정이 깨지는 지점에 방어적 assert/명확한 에러 메시지를 미리 심는다

"컴파일도 되고 크래시도 안 나지만 조용히 틀리게 동작하는" 버그(위 Inspector 사례,
`Mesh::drawInstanced` 사례)는 전부 실행 시점에 어떤 가정이 조용히 깨졌기 때문에
생겼다. 이런 가정이 있는 지점(GL 컨텍스트/바인딩 상태, ECS 컴포넌트 등록 순서, GPU 자원이
정상 생성됐다는 전제 등)에는 값을 그냥 쓰기 전에 확인하고, 깨졌을 때 (a) 조용히 잘못된
결과를 내지 말고 (b) 어떤 가정이 왜 깨졌는지 알 수 있는 메시지를 남긴다.

실제로 이번에 이 원칙을 적용해 고친 지점: [engine/renderer/InstancedBatchManager.cpp](engine/renderer/InstancedBatchManager.cpp)
`RenderBatch()` — 이전에는 `batch->mesh`와 `shader`만 null 체크하고 `batch->instanceVAO`가
0인지는 확인하지 않았다. `CreateBatch()`가 GPU 자원 생성에 실패하면 `Result` 에러를
반환하긴 하지만, 호출자가 그 `Result`를 무시하면(또는 배치가 `ReleaseBatchResources()` 이후
재사용되면) `instanceVAO == 0`인 배치가 그대로 맵에 남을 수 있었다. 이 상태에서
`RenderBatch()`가 호출되면 `glBindVertexArray(0)`(기본 VAO)을 조용히 바인딩한 채
`drawInstanced()`가 진행되어, `Mesh::drawInstanced()` 버그와 같은 종류의 "GL 에러 없이
잘못 그려짐" 증상을 냈을 것이다 — 지금은 이 경우를 감지해 로그를 남기고 그리기를
건너뛴다.

## 알려진 버그 (2026-08-10, 에디터 실제 실행으로 발견)

> 아래 기록은 2026-08-10 시점 그대로 보존한다(`ge_python.pyd`는 당시 이름 — 2026-08-20에 `quarterflying.pyd`로 개명됨, 위 "빌드 환경 메모" 참고). 이 버그를 재현할 때는 현재 파일명(`quarterflying.*.pyd`)을 기준으로 확인할 것.

- **Play Mode 뷰포트의 두 번째 `EngineViewport` 초기화 실패**: `ge_python.pyd` 빌드 후 처음으로 에디터를 `HAS_ENGINE=True`로 실제 실행해본 결과, Scene Editor 탭의 첫 `EngineViewport`는 `InitializeFromWindowHandle`이 정상 동작하지만(로그: `Engine initialized from handle successfully`, TickFrame 정상 반복), Motion Editor/Play Mode 진입 시 만들어지는 두 번째 `EngineViewport`가 `Failed to register window class (Error: 1410 / ERROR_CLASS_ALREADY_EXISTS)` → `Failed to initialize platform`로 실패한다. `Win32Platform`이 윈도우 클래스 등록 시 "이미 등록됨" 케이스를 처리하지 않는 것으로 보임(같은 프로세스 안에서 `RegisterClass`를 두 번 호출). 앱은 크래시 없이 우아하게 폴백하지만(상태바에 `Play mode (no engine)` 표시), Play Mode의 실제 엔진 렌더링은 항상 비활성 상태다.
  - **2026-09-25 해결 (실제 실행 검증: 크래시 없음까지, 화면 렌더링은 미확인)**: 윈도우 클래스 재등록은 `Win32Platform`이 `ERROR_CLASS_ALREADY_EXISTS`를 재사용으로 처리하도록 이미 고쳐져 있었다(커밋 `5c951c4`에 포함). 그런데 두 번째 뷰포트가 초기화에 성공하자, 우아한 폴백에 가려져 있던 **크래시**가 드러났다. Play Mode 전환이나 Play 버튼 클릭 시 `Windows fatal exception: access violation`이 났다. 원인은 두 가지였다. (1) `Win32Platform::PollEvents()`가 임베드 모드에서도 스레드 메시지 큐 전체를 펌프해서, 엔진 프레임 도중 Qt 핸들러가 실행되고 `TickFrame`이 재진입하거나 다른 Engine이 GL 컨텍스트를 바꿨다. (2) 각 Engine이 매 프레임 자기 GL 컨텍스트를 current로 만들지 않았다. 수정: 임베드 모드에서는 펌프하지 않음(`hostOwnsMessageLoop`), `IPlatform::MakeGraphicsContextCurrent()`를 매 프레임 호출, `TickFrame` 재진입 가드 추가. 상세는 `Win32Platform::PollEvents()`와 `Engine::TickFrame()`의 주석에 있다.
- **Scene Editor 탭의 3D 뷰포트가 검은 화면**: HWND 임베딩 자체(엔진 초기화, TickFrame 반복)는 성공하지만, 실제로 지오메트리가 렌더링되는지는 이 실행에서 확인되지 않았다(패널이 계속 검은 화면). 크래시가 없다는 기준상 "통과"로 기록하지만, 아래 `InstanceData` GPU 레이아웃 미검증 항목과 함께 렌더링 파이프라인 자체의 실제 동작 여부는 별도로 검증이 필요하다.
  - **2026-09-30 해결 (실제 실행 검증: 화면 캡처로 확인)**: 에디터를 `HAS_ENGINE=True`로 띄워 Scene 뷰포트를 캡처했다. `scene.json`의 10×10 큐브 그리드(`cube.obj`), 피라미드(`pyramid.obj`), VFX 파티클, 디버그 그리드가 모두 그려진다. 메시 브리지(`RenderableComponent.meshPath`, `editor/scene_instantiation.py`)는 개선안 P0-2로 추가했다.

## 알려진 미검증 항목

- **`InstanceData`(renderer/InstancedBatchManager.h) GPU 레이아웃 미검증**: 컴파일 에러 수정 과정에서 `_padding[3]`(12바이트, 104바이트 총합 → 16의 배수 아님)를 `_padding[1]`(4바이트, 96바이트 → 16의 배수)로 바꿨습니다. `SetupInstanceVAO()`의 stride/offset은 `sizeof`/`offsetof` 기반이라 이 변경에 자동으로 맞으므로 컴파일·링크 관점에서는 안전합니다. 다만 이 렌더러 코드 자체가 **실행되어 검증된 적이 없어 보이므로**(빌드조차 최근까지 안 됐음), 인스턴스 렌더링을 실제로 화면에 띄워보기 전까지는 GPU 쪽 레이아웃이 의도대로 맞는지 확인된 상태가 아닙니다. 나중에 인스턴스 렌더링이 이상하게 보이면 이 지점부터 의심하십시오.
  - **2026-09-30 부분 검증**: 인스턴스 100개(큐브 그리드)가 각자의 model 행렬 위치에 올바르게 그려지는 것을 화면에서 확인했다. 따라서 `model` 필드 레이아웃은 맞다. 다만 `color`/`roughness`/`metallic`은 모든 인스턴스가 같은 고정값이라, 인스턴스별 값이 제대로 전달되는지는 아직 미검증이다.
