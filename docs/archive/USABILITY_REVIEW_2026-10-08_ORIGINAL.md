# [보관] 실사용 격상 개선점 — 원안 (2026-10-08)

> 2026-10-13 보관. 이 원안은 원래 `EDITOR_APP_BUILD_GUIDE.md` 뒤에 덧붙어 있었다. 아키텍처 규칙에 맞춰 고친 현행본은 [`docs/USABILITY_IMPROVEMENT_PLAN.md`](../USABILITY_IMPROVEMENT_PLAN.md)이고, 무엇을 왜 고쳤는지는 그 문서 §1에 있다. 아래는 원문 그대로다(원문이 밝힌 기준 커밋 `766b5f8`은 이 저장소에 없다).

---

실제 사용가능한 수준으로 격상시키기 위한 개선점


**데모 상태 · 임시 사운드 · 하드코딩 · 머티리얼** 중심 리뷰입니다. (커밋 `766b5f8` 기준)

---

## 1. 데모 시드 (`demo_scene_seed.py`)

에디터 엔진 연결 시 **검증용 엔티티를 자동 생성**하는 경로입니다. 단계별 실패 격리·경로를 프로젝트에서 가져오는 점(`current_project().assets_dir`)은 잘 되어 있습니다.

| 시드 | 목적 | 하드코딩된 값 | 평가 |
|------|------|----------------|------|
| Main Camera | RenderSystem 뷰 반영 | pos `(0,5,15)`, FOV 60 | 검증용으로 타당. 씬 기본 카메라와 역할이 겹칠 수 있음 |
| **Test Cube** | draw call / 텍스처 경로 | pos `(0,1.5,5)`, `checker.png` | 주석에 “메시 파이프라인 없음 → 절차적 큐브”라고 명시 |
| Barrel / Lamp / Fire | 프리팹·계층 검증 | 고정 위치 | 의도 명확 |
| **VFX Test** | 파티클 파라미터 전부 | spawnRate 20, 색 `[1,0.6,0.15]`→`[0.9,0.1,0]` 등 | **데모 수치가 코드에 박힘** |
| **Sound Test** | Play 모드 액션 사운드 | `actions/SoundTest.action.json`, loop true | 사운드 E2E용 임시 엔티티 |

**문제점**
1. **프로덕션 씬과 데모가 같은 World에 섞임** — 시작할 때마다 검증 엔티티가 들어감. “빈 프로젝트 / 데모 프로젝트” 분기가 없음.
2. VFX·카메라·위치가 **코드 상수** — 튜닝할 때마다 Python 수정.
3. `scene.json` 큐브 그리드와 Test Cube 위치가 **서로 다른 파이프라인**을 씀 (아래 §4).

**개선 제안**
- `config` 또는 `assets/demo/seed.json`으로 시드 목록·파라미터 외부화.
- `seed_demo_scene`을 플래그/`--no-demo-seed` / 프로젝트 설정으로 끄기.
- Sound Test·VFX Test는 `demo` 태그 또는 전용 레이어로 표시해 Hierarchy에서 구분.

---

## 2. 모션 에디터 데모 상태

### 2.1 기본 샘플 자동 로드 (`motion_mixer.py`)

```text
load_default_sample()
  → sample.skeleton.json + idle.clip.json + attack.clip.json
  → Attack 레이어 weight = 50% (하드코딩)
```

- Motion Editor 진입 시 **항상 번들 샘플**을 채움 → 데모·학습에는 좋고, “빈 모션으로 시작”은 불가.
- 파일명이 `sample` / `idle` / `attack`에 고정 → 에셋 이름 바꾸면 깨짐.

### 2.2 프리뷰 UI (`animation_preview.py`)

- 섹션 role이 `"Attack"`이면 **ATTACK PHASE** 문구·색을 그리는 **특수 케이스**.
- 다른 role(Idle, Hit 등)에 대한 일반화가 없고, 게임 디자인 용어가 UI에 직접 박혀 있음.

### 2.3 모션 에디터 vs Scene Play 사운드

| 경로 | 재생 주체 | 클립 |
|------|-----------|------|
| Motion Editor 타임라인 | (액션 이벤트 편집·미리듣기) | 액션 JSON의 clip |
| Scene Play | `ActionPlayerComponent` + `SoundPlayer` | Sound Test → `SoundTest.action.json` → **Blip.wav만** |

의도는 문서대로 분리되어 있으나, **실제 들어볼 수 있는 효과음이 Blip 하나**라 “사운드 시스템 데모” 수준에 머묾.

---

## 3. 게임 사운드 테스트용 임시 사운드

### 3.1 에셋

- `engine/assets/sfx/Blip.wav` — **유일한** sfx 파일.
- `SoundTest.action.json`: 프레임 5 / 15 / 25에 동일 Blip, volume 0.8, volumeVar 0.15, **총 30프레임 루프**.

### 3.2 `SoundPlayer` (구조는 양호, 범위는 v1)

강점:
- 재생 호출부 단일화, preload / voice 상한 / 실패 시 예외 없음.
- Player(비-Qt)용 `NullSoundPlayer` 분리 고려.

한계(코드·주석에 이미 명시):
- **클립당 QSoundEffect 1개** → 겹침 재생 불가(재시작).
- 백엔드가 **에디터 QtMultimedia만** — 게임 빌드(Player)에는 엔진 오디오 없음.
- 3D 위치·거리 감쇠·버스/믹서 없음 (Sound Test 엔티티에 Transform만 있고 공간화 없음).

**리뷰 결론:** “Play 누르면 소리 나는지” 검증용으로는 충분. **콘텐츠/프로덕션 사운드 파이프라인은 아직 없음.**  
임시임을 프로젝트 설정이나 Hierarchy 이름/`demo` 플래그로 더 분명히 하는 것이 좋음.

---

## 4. 머티리얼 · 렌더링 하드코딩 (핵심 기술 부채)

### 4.1 데이터 모델 vs 실제 사용

```cpp
// Components.h
struct RenderableComponent {
    uint32_t meshHandle;
    uint32_t materialHandle;  // 리플렉션·프리팹·테스트에만 존재
    std::string meshPath;
    std::string texturePath;  // 사실상 "머티리얼" 역할의 전부
};
```

- `materialHandle`은 **RenderSystem에서 읽지 않음**.
- 배치 키도 `(meshHandle, texture)` — 머티리얼 ID 없음.
- 주석 그대로: *“shaderId/passType/materialLayout/features는 전부 기본값 - 아직 머티리얼 시스템이 없다.”*

### 4.2 인스턴스당 고정 PBR 유사 값 (`RenderSystem.cpp`)

```cpp
data.color = texture != 0 ? glm::vec3(1.0f) : glm::vec3(0.75f, 0.75f, 0.78f);
data.roughness = 0.6f;   // 전 오브젝트 동일
data.metallic  = 0.0f;   // 전 오브젝트 동일
```

- 텍스처 없으면 **회색 큐브 색** 하드코딩.
- roughness/metallic은 `InstanceData`에 들어가도 **셰이더가 안 쓸 수 있음** (아래).

### 4.3 셰이더·조명 하드코딩 (`SceneMeshRenderer.cpp`)

| 항목 | 값 | 문제 |
|------|-----|------|
| 조명 | `lightDir = normalize(0.4, 1.0, 0.35)` | 씬 라이트/`scene.json` lights와 **무관** |
| ambient | `0.25` | 상수 |
| specular | `vec3(0.3)`, power `32` | Blinn-Phong 고정, roughness/roughness 미사용 |
| 선택 강조 | 노란 `mix(..., 0.4)` | 에디터 전용 하드코딩 (허용 범위) |
| 셰이더 소스 | `.cpp` 안 GLSL 문자열 | 핫리로드·변형 셰이더 어려움 |

### 4.4 `scene.json` 머티리얼은 “읽히지만 그려지지 않는” 데이터에 가깝다

```json
"material": { "color": [0.7,0.7,0.7], "roughness": 0.5, "metallic": 0.5 }
```

- `demo_scene_integration.py`가 dict에 `'material': ...` 로 **파싱만** 하는 흔적이 있음.
- ECS `RenderableComponent` / RenderSystem 경로로 **color·roughness·metallic이 전달된다는 증거가 약함**.
- 결과: JSON에 PBR 필드가 있어도 **화면은 고정 조명 + 텍스처/회색**.

### 4.5 메시 폴백

- `meshPath` 실패 또는 비어 있으면 **절차적 유닛 큐브** (`CreateUnitCubeMesh`).
- 에셋 누락이 크래시 대신 큐브로 보이므로 디버깅이 어려울 수 있음 (로그는 있음).

---

## 5. 하드코딩 맵 (우선순위)

| 우선순위 | 위치 | 내용 | 권장 |
|----------|------|------|------|
| **P0** | `RenderSystem` color/roughness/metallic | 전 엔티티 동일 재질 | `RenderableComponent`에 color 필드 또는 최소 Material 구조체 + 배치 키 반영 |
| **P0** | `SceneMeshRenderer` lightDir | 단일 directional 하드코딩 | `scene.json` lights 또는 DirectionalLight 컴포넌트 1개라도 연결 |
| **P1** | `materialHandle` 미사용 | 죽은 필드 | 쓰거나, 문서화 후 texturePath 중심으로 API 정리 |
| **P1** | `scene.json` material | 파싱과 렌더 불일치 | 연동하거나 JSON에서 제거해 혼란 방지 |
| **P1** | `demo_scene_seed` 항상 실행 | 검증 엔티티 강제 | 설정으로 on/off |
| **P2** | Motion `load_default_sample` | 샘플 자동 로드 | “Load sample” 버튼 / 설정 |
| **P2** | Sound = Blip only | 임시 클립 | 샘플 팩 또는 문서에 “placeholder only” |
| **P2** | Attack UI 특수 케이스 | role 하드코딩 | role→스타일 테이블 |

---

## 6. 종합 평가

| 영역 | 완성도 | 한 줄 평 |
|------|--------|----------|
| 데모 시드 구조 | 중상 | 단계 분리·로깅은 좋음. **항상 시딩**이 프로덕션과 섞임 |
| 모션 에디터 데모 | 중 | 샘플 자동 로드로 데모는 잘 됨. 빈 시작·에셋 교체 약함 |
| 사운드 테스트 | 하~중 | Play→Blip 루프 검증용. 엔진 오디오·다양성 없음 |
| 머티리얼 | **하** | 필드·JSON은 있으나 **렌더는 텍스처 경로 + 고정 셰이딩** |

의도가 “기능 검증용 데모”라면 주석과 구조는 정직합니다.  
다만 **이름·JSON·컴포넌트는 머티리얼/라이트가 있는 것처럼 보이는데, GPU 경로는 아직 데모 고정값**이라 이후 작업자가 “머티리얼이 안 먹는다”고 착각하기 쉽습니다.

---

원하시면 다음 중 하나를 바로 이어서 할 수 있습니다.

1. **최소 머티리얼**: `baseColor`/`roughness`를 컴포넌트→InstanceData→셰이더까지 연결하는 패치 초안  
2. **라이트 1개**: `scene.json` directional/point → `SceneMeshRenderer` 유니폼  
3. **데모 시드 분리**: 설정 플래그 + seed JSON 스키마 초안


**GUI · 모션 에디터 스텁 · 게임플레이 조작**을 “실사용 가능” 수준으로 올리기 위한 개선점입니다. (현재 코드 기준)

---

## 현황 한 줄 요약

| 영역 | 지금 | 실사용까지 남은 핵심 갭 |
|------|------|------------------------|
| GUI | 씬 New/Open/Save 연결됨, 다크 테마, 모드 전환 | 워크플로·피드백·단축키·미연결 UI |
| 모션 에디터 | Mixer 로드/저장, 믹서·타임라인 동작 | **FBX 스텁**, 그래프 더미 곡선, 샘플 강제 로드 |
| 게임 조작 | 입력 파이프라인 + **궤도 카메라(마우스)** | **캐릭터/WASD 이동 없음**, 키 리바인딩·Pause UX |

---

## 1. GUI 개선 (에디터 실사용)

### 1.1 즉시 체감 (P0)

| 항목 | 현재 | 개선 |
|------|------|------|
| **Pause** | 메뉴/툴바에 있으나 연결은 `_on_pause` 존재 — 단축키·버튼 활성 상태와 Play 중 표시가 약함 | `Space` 또는 `Ctrl+Space`로 Pause/Resume 토글, 툴바 아이콘 상태(Play 중만 Pause 활성) |
| **About** | 메뉴만 있고 동작 불명확 | 버전·빌드·라이선스 다이얼로그 |
| **포커스** | 뷰포트 키 입력이 포커스에 의존 | Play 진입 시 뷰포트 `setFocus()`, 포커스 아웃 시 `ReleaseAll()` (이미 InputState에 API 있음) |
| **상태바** | 로그성 메시지 | 모드(Edit/Play/Pause)·FPS·선택 엔티티·단축키 힌트 고정 영역 |
| **저장 피드백** | 저장 후 UI 변화 부족 | 창 제목 `*씬이름` dirty 표시, Ctrl+S 후 상태바 “Saved” |

### 1.2 워크플로 (P1)

- **씬 로드와 데모 시드 분리**: 열 때마다 DemoSeed가 끼어들면 실작업 씬이 오염됨 → 프로젝트 설정 `autoSeedDemo: false` 기본, “데모 씬 열기”만 시드.
- **Hierarchy / Inspector**
  - 다중 선택, 검색 필터, 컴포넌트 추가 메뉴(타입 목록).
  - Prefab **Apply 비활성**은 유지하되, 로드맵 단계와 툴팁에 “예정” 명시(이미 있음) + Inspector 상단에 프리팹 인스턴스 배지.
- **뷰포트 오버레이**: 모드 배지(Scene / Play / Motion), 조작 힌트(우클릭 궤도, F 리셋 — 에디터 카메라는 이미 있음).
- **레이아웃**: Reset Layout만 있음 → 도킹 저장/복원(QMainWindow saveState) 한 번만 해도 체감 큼.
- **단축키 표**: Help → Keyboard Shortcuts (아래 조작키와 공유 데이터).

### 1.3 UX 품질 (P2)

- 위험한 동작(New Scene, Stop 시 되돌리기) 확인 다이얼로그 일관화.
- 긴 작업(씬 로드, 프리팹 대량) 진행 표시.
- 한국어/영어 UI 문자열 한쪽으로 통일(메뉴는 영문, 로그는 한글이 섞임).

---

## 2. 모션 에디터 — 스텁을 실사용으로

### 2.1 명시적 스텁 (반드시 처리)

| 스텁 | 위치 | 실사용 경로 |
|------|------|-------------|
| **Import FBX** | `do_import_fbx()` → MessageBox “다음 단계” | **A)** 단기: 메뉴 비활성 + 툴팁 “미지원” / **B)** 중기: glTF/자체 JSON 임포트만 공식 지원하고 FBX는 외부 변환 문서화 / **C)** 장기: FBX SDK 또는 Assimp |
| **Motion Graph** | `sin` 기반 **더미 속도 곡선** | 섹션 `speed`를 계단·선형 키프레임으로 표시하거나, 본 속도는 “데이터 없음” 플레이스홀더로 정직하게 표시. “실제 FBX 본 속도” 문구 제거 또는 비활성 |
| **load_default_sample** | 진입 시 항상 sample/idle/attack | 첫 실행 마법사 또는 “Load Sample” 버튼만. 빈 액션으로 시작 가능해야 함 |
| **MaskPreset::Custom** | per-bone UI 없음 | 드롭다운에서 Custom 숨기거나 “준비 중” |

### 2.2 실사용 기능 격상 (우선순위)

**P0 — 콘텐츠 파이프라인**
1. 액션 New / Save / Save As / 최근 파일 (Motion List와 디스크 동기화).
2. 이벤트 타임라인: Sound / Camera 이벤트 **추가·이동·삭제**가 마우스만으로 가능(키보드 프레임 이동 ±1, ±).
3. 클립·스켈레톤 경로는 프로젝트 `assets/motion` 기준 상대 경로로만 저장 (이미 일부 정리됨 — 깨진 절대 경로 검증).

**P1 — 프리뷰 신뢰성**
1. 본 라인 프리뷰 + 재생 헤드가 타임라인과 1:1.
2. 루프 / 배속(0.5x, 1x, 2x) UI.
3. Attack 전용 하드코딩 UI → role→스타일 테이블.

**P2 — 임포트**
1. 공식 경로를 **JSON 스켈레톤/클립**(현재 포맷)으로 고정하고, DCC 익스포트 가이드 문서 1장.
2. FBX는 “지원 예정”으로 메뉴에서 분리해 **눌러도 되는 척** 하지 않기.

---

## 3. 게임플레이 조작키 — 실사용 격상

### 3.1 지금 되는 것

- Qt → `PushInputEvent` → `InputState` (키/마우스/휠/드래그).
- **Play 중 카메라**: `CameraOrbitControlComponent` — 우클릭 드래그 궤도, 휠 줌 (C6 완료).
- 에디터 카메라: 우클릭 궤도, 휠 줌, F 리셋 (`editor_camera.py`).
- Play/Stop 단축키: Ctrl+P, Ctrl+Shift+P.

### 3.2 안 되는 것 (실게임 조작의 구멍)

| 기능 | 상태 |
|------|------|
| WASD 캐릭터/플레이어 이동 | **없음** (입력은 오는데 읽는 System 없음) |
| 점프 / 상호작용 키 | 없음 |
| 키 리바인딩 / 액션 맵 | 없음 (`IsKeyDown(KeyCode::W)` 직접 참조 수준) |
| 게임패드 | 없음 |
| Player 실행 파일에서 동일 조작 | 입력은 플랫폼 루프에 달려 있으나, **이동 로직 공통 모듈 없음** |
| Pause 중 입력 | World Pause 시 시뮬레이션 정지 — UI와 입력 정책 문서화 필요 |

### 3.3 권장 아키텍처 (실사용 최소 세트)

```text
InputState (하드웨어)
    → InputActions (논리 액션: MoveX/Y, Look, Jump, Interact)
        → PlayerMoveSystem / CameraRigSystem
```

**Phase G0 — 입력 액션 맵 (1~2일 분량 설계)**  
- `actions.json` 예: `"MoveForward": ["W", "Up"]`, `"Jump": ["Space"]`.  
- C++ `InputActions::GetAxis("Move")`, `WasPressed("Jump")`.  
- 에디터/플레이어가 같은 맵 로드.

**Phase G1 — 플레이어 이동 (실플레이 최소)**  
- `PlayerControllerComponent` 또는 기존 Transform + `CharacterMoveComponent { speed, sprintMultiplier }`.  
- WASD → XZ 이동, Shift 스프린트, Space 점프(단순 수직 속도).  
- **메인 카메라 forward 기준** 이동 (Orbit 카메라 타깃을 플레이어에 붙이면 3인칭 완성).  
- DemoSeed에 “Player” 엔티티 + Orbit 카메라 target = Player 한 세트로 Play 즉시 조작 가능.

**Phase G2 — UX**  
- Play 모드 첫 프레임에 조작 안내 오버레이 (3초 후 페이드).  
- Settings → Key Bindings.  
- 포커스 없을 때 이동 입력 무시 + 화면 중앙 “Click to capture”.

**Phase G3 — Player 빌드**  
- `PlayerMain`이 동일 `InputActions` + MoveSystem 사용.  
- 창 포커스/커서 락(옵션) 정책 통일.

### 3.4 단축키 정리안 (에디터)

| 키 | 권장 동작 |
|----|-----------|
| Ctrl+P | Play |
| Ctrl+Shift+P / Esc | Stop |
| Space (Play 중) | Pause 토글 |
| F | 선택 오브젝트 포커스(에디터 카메라) — 현재는 원점 리셋에 가깝면 개선 |
| WASD+마우스 | **Play 중만** 플레이어/카메라 (Edit 모드와 충돌 방지: 뷰포트 포커스 + Play 상태일 때만 게임 액션) |
| Delete | 선택 엔티티 삭제 (Edit) |
| Ctrl+D | 복제 (있으면) |

Edit 모드 WASD를 카메라 이동에 쓸지, Play 전용으로 둘지 **한 가지로 고정**해야 실사용 혼선이 없습니다. 권장: **Edit = 기존 마우스 궤도, Play = WASD 이동 + 마우스 룩/궤도**.

---

## 4. 실행 로드맵 (실사용 “한 판” 기준)

목표: *에디터에서 씬 열고 → Play → WASD로 움직이고 → 마우스로 카메라 → Stop 후 저장*.

| 순서 | 작업 | 완료 기준 |
|------|------|-----------|
| 1 | 데모 시드 기본 off + Pause/포커스/창 제목 dirty | 작업 씬이 시드로 오염되지 않음 |
| 2 | 모션 FBX 메뉴 정직화 + 샘플 강제 로드 제거 | 빈 액션 작업 가능 |
| 3 | InputActions + PlayerMoveSystem + 데모 Player 세트 | Play 후 10초 이상 조작 가능 |
| 4 | Play 오버레이 조작 안내 + 키 설정 파일 | 문서 없이 조작 가능 |
| 5 | Motion 이벤트/타임라인 편집 마감 + 그래프 정직화 | 사운드/카메라 이벤트 실무 편집 |
| 6 | Player 빌드에 동일 조작 | 에디터 밖 실행 |

---

## 5. 하지 않는 편이 나은 것 (지금)

- FBX 풀 임포트를 GUI 개선과 동시에 밀기 → 일정만 늘어남. JSON/glTF 경로를 공식화하는 편이 빠름.
- 머티리얼/D3D12와 조작 개선을 한 묶음으로 처리 → 체감 동선이 다름. **조작·Play 루프를 먼저**.
- 에디터 WASD와 게임 WASD를 동시에 다르게 매핑하지 말 것 (모드로 분리).

---

원하시면 다음을 바로 코드 수준으로 쪼개 드리겠습니다.

1. **InputActions + PlayerMoveSystem** 최소 구현 스펙(컴포넌트 필드·틱 수식·데모 시드)  
2. **모션 에디터 스텁 제거 PR 목록** (메뉴/그래프/샘플 로드)  
3. **GUI P0 패치 목록** (Pause 단축키, dirty 제목, 뷰포트 포커스)