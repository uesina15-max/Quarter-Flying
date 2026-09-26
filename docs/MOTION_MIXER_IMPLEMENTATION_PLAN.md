# Motion Mixer 구현 계획서

**작성일**: 2026-08-15
**목표**: Motion Editor가 Scene Editor와 같은 3D 뷰포트를 공유하고, 유저가 커스텀 바이페드(스켈레톤)/키프레임 데이터를 자체 JSON 포맷으로 임포트해서, 비슷한 리깅 세팅끼리 키프레임을 섞을 수 있는 모션 믹서로 체계화한다.

> **결정 사항 (2026-08-15)**: 임포트 포맷은 FBX/glTF가 아니라 **자체 JSON 포맷**으로 시작한다. 외부 SDK/라이브러리 의존성 없이 임포트→프리뷰→믹싱까지 가장 빨리 돌려볼 수 있고, 이 프로젝트의 빌드 이력(FBX SDK 라이선스/통합 복잡도가 가장 큰 리스크로 평가됨)을 고려한 선택이다. Blender/Maya 등 DCC 툴에서 바로 export할 수는 없으므로, 필요해지면 별도 변환 스크립트(glTF→자체 JSON 등)를 나중 단계에서 추가한다.

> **v3 계약 반영 (2026-08-15, 같은 날 추가 확정)**: 사용자가 제시한 "Motion Mixer — 착수 계약서 v3"가 아래 §2/§3의 세부 결정 중 일부를 폐기·대체한다. 전문은 [부록 A](#부록-a--착수-계약서-v3-전문)에 원문 그대로 보존. 핵심 변경 2가지:
> - **시그니처 알고리즘**: §2.2의 "전위순회 기반" 대신 **정렬된 (본 이름, 부모 이름) pair 직렬화**로 확정(형제 순서/배열 인덱스 무관).
> - **mask 의미론**: §2.3에서 암묵적이던 "부모-자식 동일 mask" 안전규칙 대신 **본별 [0,1] 가중값**으로 확정(missing entry = 0).
> - Phase 번호 체계도 v3 기준(§C, 4A/4B 분리 포함)이 우선한다 — 아래 §4 Phase 분해는 v1 초안이며 실제 진행은 부록 A §C를 따른다.
> **Phase 1 구현 완료** — 진행 현황은 [부록 B](#부록-b--phase-1-완료-기록-2026-08-15)에 기록.

---

## 1. 배경 — 지금 있는 것과 없는 것

이번 기능은 완전히 새로운 서브시스템 4개 층을 쌓는 작업이다. 착수 전 기준점을 명확히 한다.

**있는 것**
- [engine/editor/core/action_data.py](../engine/editor/core/action_data.py)의 `AnimationLayer`(name/clip_name/weight/mask/speed) 데이터클래스 — "레이어 기반 믹싱"의 데이터 모델 뼈대만 존재. 실제 블렌딩 연산도, 이를 편집하는 UI도 없음(Section Inspector에 "+ Add Layer" 버튼만 있고 클릭 핸들러 미확인).
- [engine/editor/panels/animation_preview.py](../engine/editor/panels/animation_preview.py) — `PreviewCanvas`가 `QPainter`로 그리는 **2D 막대인형 더미**. 실제 스켈레톤/본 데이터와 무관하게 사인함수로 흔들리는 그림일 뿐.
- [engine/editor/motion_editor.py](../engine/editor/motion_editor.py)의 4+1 패널(List/Preview/Inspector/Timeline/Graph) 레이아웃과 시그널 라우팅 — UI 뼈대는 있음.
- `.action.json` 저장/로드(`save_action`/`load_action`) — 섹션/이벤트/레이어만 담고, 스켈레톤이나 키프레임 트랙은 없음.

**없는 것 (전부 신규)**
- C++ 엔진에 스켈레톤/본/스키닝/키프레임 애니메이션 자료구조가 전혀 없음(저장소 전체 grep 결과 0건).
- 어떤 리깅 포맷이든 읽어들이는 임포터가 없음. `do_import_fbx()`는 메시지박스만 띄우는 자리표시자([motion_editor.py:123-124](../engine/editor/motion_editor.py#L123-L124)).
- "비슷한 리깅끼리 섞는다"는 개념(리타게팅/호환성 판정) 자체가 어디에도 없음.
- 스킨드 메시를 그리는 렌더링 경로가 없음. [ROADMAP.md](../ROADMAP.md) §5에서 막 OpenGL 컨텍스트를 처음 연결하고 디버그 그리드 하나를 띄운 상태 — RenderSystem(ECS→렌더러 연결)조차 아직 없다.

이 마지막 항목이 중요하다: **Phase 4(뷰포트 공유+실제 렌더링)는 지금 막 부트스트랩된 렌더링 파이프라인 위에 얹힌다.** RenderSystem 부재 문제가 먼저 풀리지 않으면 Phase 4도 그리드처럼 "우회 경로로 최소한만" 그리는 형태가 될 가능성이 높다 — 아래 Phase 4에서 이 전제를 다시 짚는다.

---

## 2. 설계 결정

### 2.1 임포트 포맷: 자체 JSON

스켈레톤 정의와 키프레임 클립을 분리된 JSON 파일 두 개로 관리한다(스켈레톤 하나에 클립 여러 개가 붙는 게 자연스러운 관계이므로).

**`*.skeleton.json`**
```json
{
  "name": "HumanoidBasic",
  "bones": [
    { "name": "Hips",  "parent": -1, "bindPos": [0, 1.0, 0], "bindRot": [0,0,0,1], "bindScale": [1,1,1] },
    { "name": "Spine", "parent": 0,  "bindPos": [0, 0.15, 0], "bindRot": [0,0,0,1], "bindScale": [1,1,1] }
  ]
}
```

**`*.clip.json`**
```json
{
  "name": "Attack01",
  "skeletonRef": "HumanoidBasic",
  "fps": 30,
  "totalFrames": 60,
  "tracks": [
    {
      "bone": "Hips",
      "keyframes": [
        { "frame": 0,  "pos": [0,1.0,0], "rot": [0,0,0,1] },
        { "frame": 30, "pos": [0,1.1,0], "rot": [0,0.05,0,0.998] }
      ]
    }
  ]
}
```

- `skeletonRef`는 파일 경로가 아니라 **스켈레톤 시그니처**(§2.2)를 가리키는 이름이다 — 클립이 "어떤 스켈레톤과 호환되는가"를 파일 경로가 아니라 구조로 판정하기 위함.
- 회전은 쿼터니언(x,y,z,w) 고정 — 오일러 각은 보간 시 짐벌락/축 순서 혼동 문제가 있어 처음부터 배제.
- 보간은 Phase 2에서 position은 lerp, rotation은 slerp로 구현.

### 2.2 리깅 호환성 판정 기준

"비슷한 리깅 세팅끼리 섞는다"를 코드로 판정 가능한 기준으로 좁힌다:

1. **정확 매칭(1차 구현 범위)**: 두 스켈레톤의 본 이름 집합과 부모-자식 관계(계층 구조)가 완전히 동일하면 호환. 스켈레톤을 `SkeletonSignature`(정렬된 본 이름 + 부모 인덱스 배열의 해시)로 요약해서 비교.
2. **부분 매칭(확장 과제, 이번 계획 범위 밖)**: 상체만 겹치는 등 부분 호환은 나중에 본 이름 매핑 테이블을 얹어서 지원. Phase 3에서 인터페이스만 그 확장을 막지 않는 형태로 설계.

### 2.3 렌더링 접근: CPU 스키닝 우선

GPU 스키닝(버텍스 셰이더에서 본 행렬 배열로 스키닝)이 최종적으로는 맞는 방향이지만, Phase 4 시점에는 RenderSystem 자체가 막 생긴 참이라 검증 표면을 줄이는 게 우선이다. 1차는 **CPU 스키닝**(매 프레임 CPU에서 본 트랜스폼을 정점에 적용해 버텍스 버퍼를 갱신)으로 눈에 보이는 결과를 먼저 확보하고, GPU 스키닝 전환은 별도 성능 작업으로 미룬다. `DebugGridRenderer`([engine/renderer/DebugGridRenderer.h](../engine/renderer/DebugGridRenderer.h))가 그랬듯, 필요하면 `CommandList` 추상화를 거치지 않고 직접 OpenGL을 호출해 우회한다.

---

## 3. Phase 분해

각 Phase는 "완료 기준"을 충족해야 다음으로 넘어간다. 이전 Phase가 끝나지 않은 채 다음 Phase에 손대지 않는다(특히 Phase 1~3은 눈에 보이는 결과가 없는 순수 자료구조/로직이라, 유닛 테스트가 유일한 검증 수단이다).

### Phase 1 — C++ 스켈레톤 자료구조

**대상**: `engine/animation/Bone.h`, `engine/animation/Skeleton.h/.cpp` (신규 디렉터리)

- `Bone`: name, parentIndex, bind pose(position/rotation/scale, glm 타입 — `Camera.h`가 이미 쓰는 패턴 재사용)
- `Skeleton`: `std::vector<Bone>` 소유, 이름→인덱스 조회, local→world 트랜스폼 계산(부모 체인을 따라 행렬 누적)
- `SkeletonSignature`(§2.2): 본 이름+계층 구조로부터 해시 계산

**완료 기준**: `engine/tests/SkeletonTests.cpp` 신규 — 계층 3~4단 스켈레톤을 만들어 local→world 계산이 손으로 계산한 기대값과 일치하는지, 같은 구조의 스켈레톤 두 개가 같은 시그니처를 내는지, 다른 구조는 다른 시그니처를 내는지 검증. **이 Phase는 다른 어떤 코드에서도 아직 안 쓰인다** — 렌더링/에디터 변경 없음.

### Phase 2 — 키프레임 임포터 + 애니메이션 재생기

**대상**: `engine/animation/KeyframeClip.h/.cpp`, `engine/animation/AnimationPlayer.h/.cpp`, `engine/animation/SkeletonJsonIO.h/.cpp`

- `KeyframeClip`: 본별 트랙(`frame → position/rotation`) 보관
- `SkeletonJsonIO` / `ClipJsonIO`: §2.1 JSON ↔ C++ 구조체 직렬화(이미 의존성에 있는 `nlohmann_json` 재사용 — 새 의존성 추가 없음)
- `AnimationPlayer::Sample(clip, frame) -> Pose`: 주어진 프레임에서 각 본의 보간된 트랜스폼(position lerp, rotation slerp)을 계산해 `Skeleton`에 적용

**완료 기준**: JSON 파일을 읽어 클립을 만들고, 임의 프레임(키프레임 사이 값 포함)을 샘플링한 결과가 수동 계산한 보간값과 일치함을 유닛 테스트로 검증. 여전히 렌더링/에디터 변경 없음 — 커맨드라인에서만 검증 가능한 상태로 끝낸다.

### Phase 3 — 리타게팅/호환성 매칭

**대상**: `engine/animation/RigCompatibility.h/.cpp`

- `IsCompatible(const SkeletonSignature&, const SkeletonSignature&) -> bool`
- `MixLayers(baseSkeleton, std::vector<{clip, weight, mask}>) -> Pose` — §2.1의 `AnimationLayer`(weight/mask="Full"/"UpperBody"/"LowerBody")를 실제로 블렌드하는 함수. mask는 1차 구현에서 "본 이름이 특정 접두사(`Spine`/`Arm` 등)로 시작하면 UpperBody"처럼 단순 규칙으로 시작 — 정교한 마스크 정의는 확장 과제로 남김.

**완료 기준**: 같은 시그니처를 가진 스켈레톤 2개 + 서로 다른 클립 2개를 레이어로 얹어 `MixLayers` 결과가 각 레이어의 가중치를 반영하는지 유닛 테스트로 검증. 호환 안 되는 스켈레톤 조합은 명시적으로 에러/거부되는지도 검증.

### Phase 4 — 뷰포트 공유 + 최소 스키닝 렌더링

**대상**: `engine/editor/main.py`, `engine/editor/motion_editor.py`, `engine/editor/panels/animation_preview.py`, `engine/renderer/`(신규 `SkinnedMeshRenderer` 등)

- UI 구조 변경: `main.py`의 `QStackedWidget` 페이지 중 Motion Editor(index 2)가 Play Mode(index 1)처럼 자체 `EngineViewport`를 하나 더 만드는 게 아니라, **Scene Editor의 뷰포트 위젯을 그대로 재사용**하도록 레이아웃을 바꾼다(별도 `EngineViewport` 인스턴스를 늘리는 건 이미 알려진 "두 번째 뷰포트 초기화" 계열 버그를 또 만들 위험이 있어 피한다 — [CLAUDE.md](../CLAUDE.md) 참고).
- `PreviewCanvas`(2D dummy) 제거, 공유 뷰포트에 Phase 1~3에서 만든 `Skeleton`/`AnimationPlayer`/`MixLayers` 결과를 CPU 스키닝으로 그리는 최소 렌더러 추가.
- `Renderer`(엔진 서브시스템)에 임시로 꽂아둔 `DebugGridRenderer`와 같은 방식으로, RenderSystem이 아직 없어도 동작하는 우회 경로로 시작한다.

**완료 기준**: 에디터에서 `*.skeleton.json` + `*.clip.json`을 실제로 임포트하면, Scene Editor와 같은 3D 뷰포트에 본 라인(또는 최소한 스켈레톤 뼈대 라인)이 그려지고 타임라인 재생 시 실제로 움직인다. 스크린샷으로 검증.

### Phase 5 — 믹서 UI

**대상**: [engine/editor/panels/section_inspector.py](../engine/editor/panels/section_inspector.py), 신규 `panels/motion_mixer.py`

- "Animation Layers" 영역을 실제로 편집 가능하게: 레이어 추가 시 호환 가능한 클립만 드롭다운에 노출(Phase 3의 `IsCompatible` 사용), weight 슬라이더, mask 선택.
- 여러 레이어를 얹은 블렌드 결과가 Phase 4 뷰포트에 실시간 반영.

**완료 기준**: 사용자가 서로 다른 두 개 이상의 임포트된 모션(같은 리깅)을 레이어로 얹고, weight를 조절하면 뷰포트의 포즈가 실시간으로 바뀌는 것을 확인.

---

## 4. 진행 순서 요약

```
Phase 1  스켈레톤 자료구조 (C++, 유닛테스트만)
    │
    ▼
Phase 2  키프레임 임포터 + 재생기 (C++, 유닛테스트만)
    │
    ▼
Phase 3  리타게팅/호환성 매칭 (C++, 유닛테스트만)
    │
    ▼
Phase 4  뷰포트 공유 + 최소 스키닝 렌더링 (Python+C++, 눈으로 검증)
    │
    ▼
Phase 5  믹서 UI (Python, 눈으로 검증)
```

Phase 1~3은 렌더링과 무관한 순수 로직이라 병렬로 설계할 수는 있지만, Skeleton(1) 없이 KeyframeClip(2)을 못 만들고 AnimationPlayer(2) 없이 MixLayers(3)를 못 만들기 때문에 구현 순서는 순차가 강제된다. Phase 4는 이 세 Phase가 유닛 테스트로 검증된 뒤에 시작해야 "믹싱 로직 버그"와 "렌더링 버그"가 뒤섞이지 않는다.

## 5. 리스크 / 열린 질문

- **RenderSystem 부재와의 관계**: Phase 4가 결국 RenderSystem이 해야 할 일(ECS 컴포넌트→draw call)의 일부를 스켈레톤 한정으로 먼저 만드는 셈이다. 나중에 정식 RenderSystem이 생기면 이 경로를 흡수/재정리해야 한다 — 지금은 중복을 감수하고 진행 속도를 우선한다.
- **CPU 스키닝 성능**: 본 개수/정점 수가 커지면 CPU 스키닝은 금방 느려진다. 지금 범위(에디터 프리뷰, 캐릭터 1개)에서는 문제없을 것으로 예상하지만, Phase 4 완료 시점에 실측으로 재확인.
- **마스크 규칙("UpperBody"/"LowerBody")**: 본 이름 접두사 기반 단순 규칙은 임포트하는 스켈레톤의 명명 규칙에 의존한다. 사용자가 다른 명명 규칙을 쓰면 깨진다 — Phase 5 진행하면서 실사용 데이터로 재검토 필요.
- **기존 `ActionData.layers[].clip_name`과의 연결**: 지금은 문자열 필드일 뿐 실제 파일을 가리키지 않는다. Phase 5에서 `clip_name`이 실제 `*.clip.json` 경로/이름을 가리키도록 의미를 확정해야 한다.

---

## 부록 A — 착수 계약서 v3 (전문)

> 사용자가 2026-08-15에 제시한 원문을 그대로 보존한다. API 시그니처/파일 포맷 계약/슈도코드/유닛 테스트 명세 수준의 결정문이며, 위 §1~§5(v1 초안)의 해당 부분을 대체한다.

# Motion Mixer — 착수 계약서 v3 (2026-08-15)

> **검토 범위 전제**: 본 결합 문서는 사용자가 제시한 두 리뷰 합의(§1~§4) + 추가 13개 확정(§5~§13)을 단일 self‑contained 계약서로 결집한 v3 결정문이다. 본 검토 환경에는 대상 리포지토리가 마운트되어 있지 않으므로(샌드박스 확인: `engine/`, `engine/animation/`, `engine/renderer/` 등 실제 경로 미존재), 본 문서의 모든 명세는 **API 시그니처 / 파일 포맷 계약 / 슈도코드 및 유닛 테스트 명세** 수준에 한정한다. 임포트·미존재 코드 인용 또는 라인 번호 단정은 사용자가 명시적으로 플랜에 적은 경우에만 등장한다.
>
> 본 v3는 이전 라운드 합의를 **폐기·대체**하도록 명시된 두 항목을 정정한다: (a) 시그니처 = **정렬된 (name, parentName) pair 직렬화** (전위순회 폐기, 형제 순서 무관), (b) mask = **per-bone [0,1] 가중값** (부모-자식 동일 mask 안전규칙 폐기, 안전 규칙은 "missing entry = 0" 로 재정의). 다른 모든 §1~§4 합의를 그대로 계승한다.

### A. 데이터 흐름 (이번 단계부터의 단일 진실)

```
    *.skeleton.json ─┐
                     │ Parse → Schema validate → Semantic validate
                     ▼
                 Skeleton ───────── signature ───┐
                                                 │ fast reject / 구조 재검증
                     ┌───────────────────────────┘
                     │
*.clip.json ── Parse/Validate ── KeyframeClip
                     │
                     ▼
              AnimationPlayer
                     │
Update(dt, loop) ──(currentTime)──►  frame = currentTime * clip.fps   ──►  Sample(clip, frame)  ──►  Pose(local)
                                                                                                       │
                          basePose / layerPose[L0..n-1]                                              ▼
                          Compatibiltiy check (Signature ==)                                       Mixer
                          ordered override stack (L0 → L1 → ... → L_n-1)                              │
                          per-layer mask weight  m(bone) ∈ [0,1]                                     │
                          per-layer clip weight w(l)  ∈ [0,1] clamp                                  │
                          pairwise lerp(pos) / lerp(scale) / slerp(rot)  with sign-fix               ▼
                                                                                                  Pose(local)
                                                                                                       │
                                                                                                       ▼
                                                                       World transforms  = Σ parent_local
                                                                                                       │
                                                                                                       ▼
                                                                                              EngineViewport
                                                                                               ├── SceneMode (현행)
                                                                                               └── MotionMode (preview scene + skeleton hierarchy lines, GL_LINES)

dependency rule:
  Motion Editor ─► Preview State (skeleton/clips/time/weights/masks) ─► EngineViewport (단일 인스턴스)
  (Editor ↔ OpenGL Context 직접 결합 금지)
  (UI blending 로직 재구현 금지 — Python은 weight 전달까지만, Mixer는 C++)
```

### B. v3 확정 계약 — 코딩 직전 닫아야 할 항목

#### C1. 좌표계: parent-local TRS

- 모든 키프레임 `pos`/`rot`/`scale`은 **부모 본 기준의 로컬 TRS**다. 바인드 상대 델타(additive) 표현은 도입하지 않는다.
- 결과: 1차 MVP 블렌딩은 두 클립의 로컬 TRS를 단순 보간으로 누적 가능(additive 시스템 표면 없음).
- Phase 4B의 skinning 행렬 계산은 `world_i = parentWorld_i * local_i`로 동일 경로를 그대로 쓴다.
- 구현 강제: `Transform { glm::vec3 position; glm::quat rotation; glm::vec3 scale; }` (= **mat4로 뭉개지 않는다**).

```cpp
struct BoneLocalBind     { glm::vec3 position; glm::quat rotation; glm::vec3 scale; };
struct BoneLocalPose     { glm::vec3 position; glm::quat rotation; glm::vec3 scale; };
struct Transform         { glm::vec3 position; glm::quat rotation; glm::vec3 scale; };
using Pose               = std::vector<BoneLocalPose>; // skeleton.bones 와 동일 순서·길이
```

#### C2. Pose / Signature / Skeleton 분리 사고

- `Bone(Bind)` 는 immutable(스켈레톤 자산 정의). `Pose` 는 가변이고 매 샘플/블렌드마다 새로 계산.
- `Skeleton` 는 `vector<BoneLocalBind>` + `nameToIndex` 맵 + `SkeletonSignature`.

```cpp
struct Bone {
    std::string name;
    int parentIndex;       // -1 = root
    BoneLocalBind bind;
};

struct Skeleton {
    std::vector<Bone> bones;
    std::unordered_map<std::string, int> nameToIndex;
    SkeletonSignature signature;
    void recomputeSignature(); // structured binding 갱신 후 호출
};
```

#### C3. SkeletonSignature — 인덱스·형제 순서 비의존 canonical

- **폐기**: 전위순회 기반 시그니처.
- **확정**: 본 전체를 `(bone.name, parent.name)` 페어로 모아 **사전순 정렬 후 직렬화** → 형제 순서 무관, `bones[]` 인덱스 배치 의존 제거.
- canonical 직렬화는 가시: 빠르고 결정적이며 충돌 가능성이 매우 낮으므로 PoC 단계에서 그대로 채택이 정당하다.
- 그럼에도 **fast reject 후 canonical 재검증**을 호환성 판정의 정확한 경로로 둔다(hash만으로 일치 판정 내리지 않음).

```text
canonical = [
  formatVersion(1),
  axis("Y_UP_RH"),         # 지원 외 axis/unit 는 정확 매칭 거부
  unit("meter"),
  ...sorted pairs:
    "Arm.L|Spine",
    "Hips|<ROOT>",
    "Spine|Hips",
    "Head|Spine", ...
]
hash      = hash64(canonical) // fast reject 용

struct SkeletonSignature {
    uint32_t            formatVersion; // = 1
    AxisSystem          axis;
    Unit                unit;
    std::vector<BonePair> canonicalPairs; // 정렬된 (name, parent.name)
    uint64_t            hash;             // canonical 직렬화 → 64-bit
};
```

- `IsCompatible(a,b)`:
  ```text
  if (a.hash != b.hash)                       return REJECT_FAST
  if (a.canonicalPairs != b.canonicalPairs)   return REJECT_STRUCTURAL
  if (a.axis != b.axis || a.unit != b.unit)   return REJECT_META
  return COMPATIBLE
  ```
- 동일 본 이름·계층이지만 **다른 axis 또는 다른 unit**을 가진 두 스켈레톤은 정확 매칭 거부됨(축/단위 변환은 MVP에서 하지 않음).

#### C4. AnimationPlayer 와 Sample 분리

```cpp
namespace anim {

struct KeyframeTrack {
    std::string boneName;
    struct Key {
        float frame;                       // >= 0
        glm::vec3 position;
        glm::quat rotation;                // 정규화 책임을 Sample에 둠
    };
    std::vector<Key> keys;                 // 단조 증가(중복 frame 오류)
};

struct KeyframeClip {
    std::string name;
    std::string skeletonRef;               // 사람-가독용
    SkeletonSignature signature;           // = 검증된 시그니처(클립 헤더로 부터 메타 + 본서명 셋)
    float fps{30.f};
    uint32_t totalFrames{0};
    std::vector<KeyframeTrack> tracks;
};

// clip → (bone index → bone-local interpolated TRS)
Pose Sample(const KeyframeClip& clip, float frame);   // frame < 0 또는 frame >= totalFrames → clamp

class AnimationPlayer {
public:
    void  load(const Skeleton& s, const KeyframeClip& c, LoopMode loop);
    void  setPlaybackSpeed(float s);        // 기본 1.0
    void  update(float deltaSeconds);       // currentTime += dt * speed; loop 정책 적용; frame = currentTime * clip.fps
    Pose  currentLocalPose() const;         // = Sample(clip, frame)
    float currentTime() const;
private:
    float currentTime_{0.f};
    LoopMode loop_{LoopMode::Clamp};        // 1차 MVP = Clamp. LoopMode는 enum으로 확장 여지만 둠
};

} // namespace anim
```

- **분리의 효과**: `Sample` 은 순수 함수(유닛 테스트 가능) / `Update` 는 상태·시간·루프 정책 분리.

#### C5. 프레임/시간 정책 (모순 없음)

- 시간 = **초**, 정규화 기준 = 타임라인 fps 무관.
- 타임라인 fps 와 clip fps 는 분리.
- frame = `currentTime * clip.fps` (수식은 단일 곳에만 둠).
- Sample 입력 frame:
  - `frame < 0` → **clamp to 0**
  - `frame >= totalFrames` → **clamp to totalFrames - 1**
  - 보간은 양 키 사이의 선형(보간 모드는 1차 = linear 만)
- AnimationPlayer 루프:
  1차 MVP 정책 = **Clamp** (마지막 키프레임 pose에서 정지). 코드는 `LoopMode` enum + flag 분기로 추후 확장 여지를 닫지 않음.

#### C6. 슬렌더(rotation) 최단경로 정규화

```cpp
inline glm::quat shortestSlerp(const glm::quat& a, const glm::quat& b, float t) {
    glm::quat b2 = b;
    if (glm::dot(a, b2) < 0.0f) b2 = -b2;   // 부호 반전(quaternion double cover 회피)
    return glm::slerp(a, b2, t);
}
```

- 테스트 필수: `q == rotation`, `-q == rotation`(동일 방향성) → slerp 결과 짧은 경로. 미적용 시 180° 회전 구간에서 360° 회전 발생, 달이 지구를 한바퀴 돌듯 손이 뜁니다(Phase 2 단위 테스트에 강제).

#### C7. MixLayers — 순서 있는 override 스택 · pairwise 누적 · mask 별도 API

- **폐기**: "부모-자식이 동일 mask 이어야 적용" 안전 규칙.
- **확정**: mask = **bone 별 [0,1] 가중값**. missing / 미정의 entry 는 0(즉 그 본은 base 유지). 부모-자식이 서로 다른 mask_weight 라도 plain per-bone lerp 적용(단, 부모 본이 0 이면 자식도 사실상 0 이 되므로 명시적으로 "계층 누적 영향은 의도" 라고 명세).

```cpp
enum class MaskPreset { Full, UpperBody, LowerBody, Custom };

struct Layer {
    const KeyframeClip* clip;
    float               weight;     // clamp[0,1]
    MaskPreset          maskPreset;
    // Custom 의 경우 posteri 에 std::unordered_map<int, float> perBoneWeight; (1차 미사용 슬롯)
};

// per-bone mask weight 생성 — 동일 본을 두 mask preset 이 동시에 정의하면 error/override 정책 단일화
float maskWeightForBone(const Layer& layer, int boneIndex, const Skeleton& skeleton);
```

- Pairwise override 수식(각 bone 별 — `result` 가 누적, `layerN` 순서):

```cpp
Pose mixLayers(const Pose& baseLocal,
               const std::vector<Layer>& orderedLayers,
               const Skeleton& skeleton) {
    Pose result = baseLocal;
    for (const Layer& L : orderedLayers) {
        float weight = std::clamp(L.weight, 0.0f, 1.0f);   // UI slider 와 동일 도메인
        for (int i = 0; i < (int)result.size(); ++i) {
            float m = maskWeightForBone(L, i, skeleton);   // m ∈ [0,1], missing entry → 0
            float effectiveWeight = weight * m;            // ∈ [0,1]
            if (effectiveWeight <= 0.0f) continue;

            const Pose& src = sampledPoseOf(L.clip);       // caller 가 1회 샘플링 한 결과 재사용
            result[i].position = glm::lerp(result[i].position, src[i].position, effectiveWeight);
            result[i].scale    = glm::lerp(result[i].scale,    src[i].scale,    effectiveWeight);
            result[i].rotation = shortestSlerp(result[i].rotation, src[i].rotation, effectiveWeight);
        }
    }
    return result;
}
```

- 의미론(주석/문서에도 적는다):
  - "pairwise 누적" = base 에 layer1을 ν1(weight·mask) 로 lerp → 그 결과를 layer2 와 ν2 로 lerp → …
  - "순서 있는 override" = layer 가 뒤로 갈수록 우선(상위)이다. 시각적으로 "위가 가장 최근 의도".
  - weight = 1, mask = 1 인 단일 레이어는 그 클립 pose 로 교체(=override)됨.
  - weight = 0.5, mask = 1 인 두 레이어는 "각 50%" 가 아니라 base → L0 50% → (그 결과) → L1 50%. 직렬화 시 결과가 다르다.

- 호환 판정: 첫 레이어 적용 전 `IsCompatible(skeleton.signature, layer.clip.signature)` 결과가 COMPATIBLE 이어야 한다. 미일치 시 MixLayers 자체를 호출하지 않고 `expected<Pose, CompatError>` 로 `Err(specific layer index, reason)` 반환.

#### C8. Mask preset 의 명시 정의

1차 MVP 에서:

- `Full` : 모든 본 = 1.
- `UpperBody` : 지정 본 서브트리(`Spine` 및 그 자손 `Arm*`, `Hand*`, `Head*` 등) = 1, 그 외 = 0. (lookup table 을 다른 enum 와 같은 곳에서 단일하게 둔다.)
- `LowerBody` : `Hips` 및 그 자손 `Leg*`, `Foot*` 등 = 1, 그 외 = 0.
- `Custom` : 1차 슬롯만(빈 map). 2차에서 `Spine=0.25, Chest=0.5, Arm=1.0` 같은 feathering.

lookup table 조회는 본 이름이 prefix-match 로 처리되며(예: `Arm.L` → `Arm`), MVP 에서 두 preset 이 동일 본을 1 로 예정하면 빌드/load 단계 단언 failure.

#### C9. Root motion 정책

- MVP 는 **root motion extraction/in-place conversion 미지원**.
- 루트 본(`parentIndex == -1`, name == `Hips` 등) 의 TRS 도 다른 본과 동일한 parent-local TRS 로 처리되며 일반 `MixLayers` 파이프라인을 그대로 통과한다.
- 즉 locomotion 시스템·씬 전역 이동 분리 같은 표면을 의도적으로 노출하지 않는다.
- Phase 5 UI 에 있더라도 "extract root motion" 체크박스·옵션은 노출하지 않는다(나중 확장 슬롯).

#### C10. JSON 파일 헤더 + validation 단계

```text
*.skeleton.json
{
  "format": "core.skeleton",
  "version": 1,
  "axis":   "Y_UP_RH",          // fixed enum, MVP 는 "Y_UP_RH" 만 accept
  "unit":   "meter",             // fixed enum, MVP 는 "meter" / "centimeter" 만 accept
  "name":   "HumanoidBasic",
  "bones": [...]
}

*.clip.json
{
  "format": "core.clip",
  "version": 1,
  "axis":   "Y_UP_RH",
  "unit":   "meter",
  "skeletonRef": "HumanoidBasic",  // 사람-가독 라벨
  "signature": "<hex64>",          // optional: 빠른 mismatch 디텍트
  "fps": 30,
  "totalFrames": 60,
  "tracks": [...]
}
```

- import 파이프라인 단계 고정:

```
Read raw bytes
  │
  ▼
Parse (JSON)
  │
  ▼
Schema validation   ← format / version / axis / unit 매칭
  │
  ▼
Semantic validation ← 구조 무결성(below)
  │
  ▼
expected<T, Error>  ← partial object 반환 금지(실패 시 raw+error 만)
```

- Skeleton: 중복 name / cycle / invalid parent / root 2개 이상 / 지원 외 axis-unit → error.
- Clip: unknown bone / duplicate frame / frame < 0 / frame >= totalFrames / fps <= 0 / quaternion (0,0,0,0) / 지원 외 axis-unit → error.

> *clip 의 bone 이름이 skeleton 에 없어도 즉시 거절한다. "가장 비슷한 본으로 매핑" 같은 휴리스틱을 1차에서는 두지 않는다.(부분 매칭은 다른 단계의 인터페이스 슬롯이 받는다.)*

#### C11. 단일 viewport + MotionMode 스위칭

- **확정 = B 안**: 단일 `EngineViewport` 인스턴스에 `RenderScene` 모드 enum.
  - `SceneMode` (현행, 미션)
  - `MotionMode` (Scene + preview skeleton overlay)
- Motion Editor 는 viewport 를 **소유하지 않는다**. "preview state(skeleton·clips·currentTime·weights·masks)" 만 갖고, viewport 가 그것을 받아 렌더링한다.
- dependency 방향:
  - Motion Editor → Preview State → EngineViewport.
  - Motion Editor ↔ OpenGL Context 직접 결합 금지.
- QStackedWidget 페이지 전환 시 OpenGL 컨텍스트 라이프사이클을 의식해서 viewport 를 재생성하지 않고 mode 만 전환한다.

#### C12. Phase 4A 렌더링 조건 ("bone hierarchy line rendering")

- 그리는 단위 primitive = **`GL_LINES`** (per bone = 1 line from parent.world to bone.world).
- "line strip" 이라는 표현은 사용하지 않는다(전체 뼈대를 하나의 연속선으로 오해할 수 있다).
- 카메라·라이트는 SceneMode 와 공유.
- 좌표 변환:
  ```
  for each bone i:
      parent_world = (parent < 0) ? identity : world[parent]
      local_world[i] = parent_world * TRS(bind[i] ∪ poseDelta[i])  // but bind-only mode 도 지원
      draw_line(parent_world.translation, local_world[i].translation)
  ```
- Phase 4A 완료 검증 = clip 한 개 재생 vs weight 0.5 로 blend 한 결과 pose 가 viewport에서 같은 skeleton 위에서 비교됨 (Idle frame 0 vs Attack frame 30 vs Blend(0.5)). 스크린샷 검증.

#### C13. MixLayers 가 Pose 반환 형태

```cpp
std::expected<Pose, MixerError> compose(
    const Skeleton& base,
    const Pose& baseLocalPose,
    const std::vector<LayerSpec>& orderedLayers);

enum class MixerError {
    IncompatibleAtLayerIndex,
    EmptyLayerClip,
    InvalidWeightRangeAfterClamp, // clamp 후 도메인 외였음(코딩 버그 신호)
};
```

- 호환 거부는 MixLayers 자체 진입 전에 `expected` 로 명시. 호출 측(나중에 Python 바인딩)은 메시지/색·표시위치를 그대로 전달하기 좋다.

### C. Phase 분해 (v3)

```
Phase 1 — Skeleton 자료구조 + Pose·Transform + SkeletonSignature
         (deliverable: Bone/Skeleton/SkeletonSignature/ BoneLocalBind 골격 + signature 유닛 테스트)

Phase 2 — KeyframeClip + Sample + JsonIO (validation 포함, expected<T, Error>)
         (deliverable: Sample 단위 테스트, key border/crossing 보간, slerp 부호 정규화 테스트, validation 거부 테스트)

Phase 3 — Compatibility & Layer Mixing  (이전 명칭 "Retargeting" → 이번 개명)
         (deliverable: MixLayers pairwise override, mask preset, ThreeLayer + 위계 불일치 mask 테스트, INCOMPATIBLE_At_LayerIndex expected 경로)

Phase 4A — Single viewport + MotionMode + bone hierarchy line rendering (GL_LINES)
          (deliverable: clip 1개 시간 재생 + weight 0.5 레이어 블렌드가 같은 skeleton 위에서 모두 시각적으로 확인)

———— 여기까지가 Motion Mixer 의 핵심 기술 가설이 검증된 지점 ————

Phase 5 — Mixer UI  ※ 사전강제조건: Phase 3, 4A 완료
         (deliverable: 호환 클립만 dropdown, weight slider[0,1], mask preset 선택, 블렌드 pose 실시간 viewport 반영)
         (구현 강제: UI에서 blending 로직 재구현하지 않음 — Python 은 weight/state 전달까지만, C++ Mixer 호출만)

Phase 4B — CPU skinning
          선행: `*.mesh.json` (정점 + 본-웨이트) 포맷 신설 + JSON validation 동일 단계 적용
          Phase 4A 의 bone hierarchy lines 위에 동일 Scene API 로 skin 결과 출력
```

### D. 통합 데이터 흐름

- `Pose` 가 중심. `AnimationPlayer`, `LayerMixer`, `EngineViewport` 는 `Pose` 만 매개로 상호작용.
- `Motion Editor` 는 `Preview State` 만 갱신. viewport 는 그것을 폴링/시그널·이벤트로 반영.
- blending 의 모든 단계(per-bone, finite)이 결정적.

### E. 누적 착수 체크리스트 (v3 통합, 21개)

1. 좌표계 계약 — 모든 keyframe TRS = parent-local. bind-relative delta 미사용 (→ C1)
2. Pose·Transform 계약 — `Pose = vector<BoneLocalPose>`, `Transform = vec3+quat+vec3` (mat4 압축 금지) (→ C1)
3. SkeletonSignature 계약 — canonical: 정렬된 (name, parentName) pair 직렬화 + version/axis/unit (→ C3)
4. Layer blending 계약 — 순서 있는 override 스택, pairwise 누적, slerp + 부호 반전, mask 별도 API (→ C6, C7)
5. frame/time 계약 — `Update(dt)` / `Sample(float frame)` 분리; clamp 정책 (1차) (→ C4, C5)
6. JSON 계약 — format/version/axis/unit + 검증 단계 명시 (→ C10)
7. Phase 4A/4B 분리 — bone hierarchy line 우선, skinning 은 mesh.json 신설 후 (→ C12)
8. 단일 viewport + mode switching — Motion Editor 는 viewport 미소유, MotionMode 만 전환 (→ C11)
9. Keyframe 좌표계 — parent-local TRS 확정 (→ C1)
10. root motion = MVP 에서 일반 pose 에 포함 (→ C9)
11. mask = per-bone [0,1] weight abstraction, missing entry = 0 (→ C7, C8)
12. weight = clamp[0,1] (→ C7)
13. missing track = bind pose 유지 (→ C4)
14. out-of-range frame = clamp (→ C5)
15. clip fps vs editor timeline fps 분리 (→ C4, C5)
16. skeleton 형제 순서가 signature 에 영향 없음 (→ C3)
17. JSON validation 실패 시 object 생성 안 함 — `expected<T, Error>` (→ C10)
18. 사전조건: Phase 3 + 4A 완료 후에만 Phase 5 진입 (→ C)
19. 명칭 정정: "Retargeting" → Compatibility & Layer Mixing / "line strip" → bone hierarchy line rendering (GL_LINES) (→ C3, C12)
20. Frame/time: 음수/범위 초과 = clamp, Loop 는 enum 슬롯만 (→ C5)
21. Signature fast reject + 구조 재검증: hash 같아도 canonical pair·메타 재검증 (→ C3)

### F. Phase 1 (구현 시작점) — 입력 조건

Phase 1 코딩 시작에 필요한 정보는 모두 닫혔다. 시작 코드 골격:

1. `engine/animation/Bone.h` — `Bone { name, parentIndex, BoneLocalBind }`.
2. `engine/animation/Transform.h` — `Transform { pos, rot, scale }`.
3. `engine/animation/Pose.h` — `using Pose = std::vector<Transform>;`.
4. `engine/animation/Skeleton.h/.cpp` — `Skeleton::recomputeSignature()` + `nameToIndex`.
5. `engine/animation/SkeletonSignature.h/.cpp` — `struct SkeletonSignature` + `hash64` (canonical 직렬화) + `IsCompatible`.
6. `engine/tests/SkeletonTests.cpp` — check: (a) 3단 계층의 local→world == 손 계산 값, (b) 부모 없는 root 혼재·cycle `expected(Error)`, (c) 형제 순서만 다른 두 skeleton 동일 signature, (d) axis/unit 가 다른 두 skeleton 정확 매칭 거부.

Phase 1 의 골격이 위 조건로 안정되면 Phase 2/3 는 대부분 기계적으로 진행된다. Phase 4A 종료 시점에 본 PoC 검증만 통과하면 Motion Mixer 의 가장 위험한 기술 가설을 해결할 수 있다: *"같은 skeleton 의 두 클립이 위 weight 로 섞이면 viewport 의 bone hierarchy line 이 같은 skeleton line 안에서 two_pose 사이 중간 포즈로 움직이는가."* CPU skinning 은 그 이후의 사치 항목이다.

---

## 부록 B — Phase 1 완료 기록 (2026-08-15)

부록 A §F의 6개 산출물을 그대로 구현했다. 한 가지 의도적 단순화: §F는 `Bone`이 `BoneLocalBind` 타입을 쓰는 것으로 적혀 있지만(§C1/C2), Phase 1 자체는 바인드 포즈만 다루고 Pose(애니메이션 포즈)와의 타입 구분이 아직 필요 없어서 `Bone::bind`와 `Pose`의 원소 타입을 똑같이 `Transform`으로 통일했다. `BoneLocalBind`/`BoneLocalPose`처럼 의미를 코드 타입으로 갈라야 할 필요가 생기면(Phase 2/3에서 둘을 헷갈려 버그가 나는 경우) 그때 타입 별칭을 쪼갠다 — 지금은 최소 골격을 유지.

**구현 파일**

| 파일 | 내용 |
|---|---|
| [engine/animation/Transform.h](../engine/animation/Transform.h) | `Transform{position, rotation, scale}` + `ComposeTransform(parentWorld, local)` |
| [engine/animation/Pose.h](../engine/animation/Pose.h) | `using Pose = std::vector<Transform>` |
| [engine/animation/Bone.h](../engine/animation/Bone.h) | `Bone{name, parentIndex, bind}` |
| [engine/animation/SkeletonSignature.h/.cpp](../engine/animation/SkeletonSignature.h) | `BonePair`, `SkeletonSignature`(formatVersion/axis/unit/canonicalPairs/hash — hash는 axis/unit 제외하고 계산해서 §C3의 3단계 판정(RejectFast→RejectStructural→RejectMeta)이 실제로 도달 가능하도록 함), `ComputeSkeletonSignature`, `IsCompatible` |
| [engine/animation/Skeleton.h/.cpp](../engine/animation/Skeleton.h) | `Skeleton::Create`(이름 중복/parentIndex 유효성/루트 개수/사이클 검증 후 `expected<Skeleton, SkeletonError>`), `ComputeBindWorldTransforms`, `ComputeWorldTransforms` |
| [engine/tests/SkeletonTests.cpp](../engine/tests/SkeletonTests.cpp) | §F의 (a)~(d) 4개 + 보너스 2개(DuplicateBoneName, InvalidParentIndex) = 8개 테스트 |

**빌드 시스템 변경**
- [engine/CMakeLists.txt](../engine/CMakeLists.txt): `ANIMATION_SOURCES` 세트 추가, `ge_engine` 타겟에 링크.
- [engine/tests/CMakeLists.txt](../engine/tests/CMakeLists.txt): `ANIMATION_SOURCES` glob 추가, `SkeletonTests.cpp`를 `SimpleEngineTests`에 추가. 겸사겸사 이 테스트 타겟이 그동안 링크 단계에서 죽어있던 원인 2개도 고쳤음(아래 참고) — 그전엔 이 타겟 자체가 한 번도 끝까지 빌드된 적이 없었던 것으로 보임.

**검증 결과**
- `ge_engine.dll`, `ge_python.pyd` 정상 빌드(Release).
- `SimpleEngineTests.exe --gtest_filter=SkeletonTest.*` — **8/8 통과**.
- 전체 스위트(`SimpleEngineTests.exe`, 315개) — **310 통과 / 5 실패**. 실패 5개(`SystemConcurrencyPropertyTest*` 2개, `RendererInstancingTest.SubmitIndexedInstancedBatchSucceeds`, 통합 버전 2개)는 전부 Motion Mixer/animation과 무관한 기존 코드 — Skeleton 작업 이전부터 있었을 가능성이 높다(이 테스트 타겟이 이번에 처음으로 끝까지 링크됐으므로 "이전부터 있었다"를 직접 실행으로 확인한 적은 없음 — 별도 이슈로 취급, 이번 작업 범위 밖).

**부수적으로 발견/수정한 테스트 인프라 버그 (Motion Mixer와 무관, 빌드 검증 과정에서 발견)**
1. `SimpleEngineTests`가 `tinyobjloader`를 링크하지 않아 `RENDERER_SOURCES`에 글롭으로 포함되는 `Mesh.cpp`가 `tiny_obj_loader.h`를 못 찾아 컴파일 실패(`C1083`) — `ge_engine`은 링크해서 문제없었지만 이 테스트 타겟만 누락돼 있었음. `tinyobjloader`를 링크 목록에 추가해 해결.
2. `SimpleEngineTests`가 프로젝트에서 이미 제거된 `glfw`를 여전히 링크 목록에 갖고 있어 `LNK1181`(입력 파일을 열 수 없음)로 링크 실패 — 죽은 참조 제거.
- 이 두 문제 때문에 `SimpleEngineTests` 전체가 그동안 한 번도 끝까지 빌드/링크된 적이 없었던 것으로 보인다(이번에 고치기 전까지는 항상 컴파일 또는 링크 단계에서 죽었음). 즉 위에서 발견한 5개 실패 테스트는 "이번에 실행해보니 처음으로 드러난 것"이라는 뜻이지, 이번 세션에서 새로 깨진 게 아니다.

**다음 단계**: Phase 2 (KeyframeClip + Sample + JsonIO) — 부록 A §F 마지막 문단대로 Phase 1 골격이 안정됐으므로 기계적으로 진행 가능.

---

## 부록 C — Phase 2 완료 기록 (2026-08-15)

부록 A §C4/§C5/§C6/§C10을 그대로 구현했다.

**의도적 보정 사항 (계약서 pseudocode 대비)**
- `Sample(const KeyframeClip&, float)`은 계약서 원문 시그니처지만, "missing track = bind pose 유지"(체크리스트 #13)를 만족하려면 결과 `Pose`의 본 순서/개수와 트랙 없는 본의 대체값을 알아야 해서 `Sample(const Skeleton&, const KeyframeClip&, float)`로 skeleton 인자를 추가했다. `AnimationSampler.h`에 이유를 주석으로 남겨둠.
- `namespace anim { ... }`(§C4)은 도입하지 않고 Phase 1과 같은 평평한 `namespace Engine`을 그대로 썼다 — 이 엔진의 다른 모든 모듈(Camera/Renderer/Mesh 등)이 서브 네임스페이스 없이 `Engine` 하나만 쓰고 있어서, 여기서만 `anim`을 새로 만들면 오히려 일관성이 깨진다고 판단.
- `KeyframeTrack::Key`에는 계약서 원문대로 `scale` 필드가 없다 - 즉 이 MVP는 스케일을 애니메이션하지 않는다. 트랙이 있는 본이라도 `Transform.scale`은 항상 스켈레톤 바인드 스케일을 그대로 쓰도록 `AnimationSampler.cpp`에 구현.

**구현 파일**

| 파일 | 내용 |
|---|---|
| [engine/animation/KeyframeClip.h](../engine/animation/KeyframeClip.h) | `KeyframeTrack{boneName, keys}`, `KeyframeTrack::Key{frame,position,rotation}`, `KeyframeClip{name,skeletonRef,signature,fps,totalFrames,tracks}` |
| [engine/animation/AnimationSampler.h/.cpp](../engine/animation/AnimationSampler.h) | `ShortestSlerp`(§C6 부호 정규화), `Sample`(§C5 프레임 clamp + 트랙 보간 + 바인드 폴백) |
| [engine/animation/AnimationPlayer.h/.cpp](../engine/animation/AnimationPlayer.h) | `LoopMode{Clamp}`, `AnimationPlayer::load/setPlaybackSpeed/update/currentLocalPose/currentTime` |
| [engine/animation/SkeletonJsonIO.h/.cpp](../engine/animation/SkeletonJsonIO.h) | `ParseSkeletonJson` — 스키마 검증(format/version/axis/unit/필드) 후 `Skeleton::Create`로 의미 검증 |
| [engine/animation/ClipJsonIO.h/.cpp](../engine/animation/ClipJsonIO.h) | `ParseClipJson` — 스키마 검증 + unknown bone/중복·역행 frame/범위초과/fps/영쿼터니언 검증, 성공 시 `clip.signature`를 참조 스켈레톤의 시그니처로 채움 |

**빌드 시스템 변경**
- [engine/CMakeLists.txt](../engine/CMakeLists.txt): `ANIMATION_SOURCES`에 4개 파일 추가.
- [engine/tests/CMakeLists.txt](../engine/tests/CMakeLists.txt): `AnimationSamplerTests.cpp`, `AnimationJsonIOTests.cpp` 추가 (animation 디렉터리는 이미 Phase 1에서 glob 대상에 포함됨).

**검증 결과**
- `ge_engine.dll`, `ge_python.pyd` 정상 빌드(Release).
- 신규 테스트 26개 — Sample 보간(기본/키 경계/키 위/트랙 없음), 프레임 도메인 clamp, `ShortestSlerp` 부호 정규화(부호가 다른데도 최단경로로 도는지 + 같은 부호일 때 일반 slerp와 동일한지), `AnimationPlayer` 시간 진행/Clamp 루프, `SkeletonJsonIO`/`ClipJsonIO`의 성공 케이스 + 8종 거부 케이스(포맷/버전/axis/unit/unknown bone/중복 frame/역행 frame/음수 frame/범위초과/fps/영쿼터니언/시그니처 불일치) — **전부 통과**.
- 전체 스위트 341개(Phase 1 대비 +26) — **336 통과 / 5 실패**, 실패 5개는 Phase 1과 동일한 기존 무관 테스트(동시성/인스턴싱) 그대로, 새 회귀 없음.

**다음 단계**: Phase 3 — Compatibility & Layer Mixing (`MixLayers`, mask preset, `expected<Pose, MixerError>`).

---

## 부록 D — Phase 3 완료 기록 (2026-08-15)

부록 A §C7/§C8/§C13을 구현했다.

**의도적 보정/결정 사항 (계약서 pseudocode 대비)**
- **함수 이름**: 계약서가 같은 연산에 §C7 `mixLayers`와 §C13 `compose` 두 이름을 쓰고 있어서, 이번 Phase 요청에서 사용자가 직접 쓴 이름인 `MixLayers`로 통일했다.
- **`LayerSpec::sampledPose`**: §C7 pseudocode의 `Layer{clip, weight, maskPreset}`에는 없지만, 같은 pseudocode 안의 `sampledPoseOf(L.clip)`(주석: "caller가 1회 샘플링한 결과 재사용")를 실제로 동작하게 하려면 레이어별로 이미 샘플링된 `Pose`를 들고 있어야 한다 — 레이어마다 재생 시간(frame)이 다를 수 있고, 그 시간 관리는 Phase 4/5의 몫이라 `LayerMixer` 쪽 관심사가 아니라고 판단해 필드로 명시했다.
- **`outFailedLayerIndex`**: §C13은 반환 타입을 `expected<Pose, MixerError>`로 명시했지만(문자 그대로 유지), 같은 문서 앞부분 프로즈는 "Err(specific layer index, reason)"를 요구한다. `MixerError`를 구조체로 바꾸면 시그니처 계약을 깨므로, 실패한 레이어 인덱스는 선택적 out 파라미터(`int* outFailedLayerIndex`)로 분리했다.
- **마스크 프리셋 접두사 테이블 비중첩 검증**(§C8 마지막 문단 "두 preset이 동일 본을 1로 예정하면 빌드/load 단계 단언 failure")은 매 호출 시 검사하지 않고 `AreMaskPresetTablesNonOverlapping()`을 유닛 테스트(`MaskPresetTest`)에서 한 번 호출하는 방식으로 구현 — 고정된 코드 데이터라 런타임 비용을 들일 이유가 없음.

**구현 파일**

| 파일 | 내용 |
|---|---|
| [engine/animation/LayerMixer.h/.cpp](../engine/animation/LayerMixer.h) | `MaskPreset{Full,UpperBody,LowerBody,Custom}`, `LayerSpec`, `MaskWeightForBone`(접두사 매칭: Upper=Spine/Arm/Hand/Head, Lower=Hips/Leg/Foot), `MixerError{IncompatibleAtLayerIndex,EmptyLayerClip,InvalidWeightRangeAfterClamp}`, `MixLayers` |

**빌드 시스템 변경**
- [engine/CMakeLists.txt](../engine/CMakeLists.txt): `ANIMATION_SOURCES`에 `LayerMixer.cpp` 추가.
- [engine/tests/CMakeLists.txt](../engine/tests/CMakeLists.txt): `LayerMixerTests.cpp` 추가.

**검증 결과**
- `ge_engine.dll`, `ge_python.pyd` 정상 빌드(Release).
- 신규 테스트 11개 — pairwise override(완전 교체), pairwise 누적(52.5 ≠ 평균 55임을 직접 확인해 "균등 배분 아님"을 증명), UpperBody/LowerBody 마스크 프리셋 매칭, **위계 불일치 mask**(부모 Spine=0인데 자식 Arm=1이어도 각자 독립 적용 — v3에서 폐기된 "부모-자식 동일 mask" 안전규칙이 실제로 없음을 확인), 단일 레이어 호환성 실패 + 인덱스, **3-레이어 중 3번째만 비호환**일 때 인덱스 2를 정확히 가리키는지, `EmptyLayerClip`, `InvalidWeightRangeAfterClamp`(NaN 방어), 빈 레이어 목록, 마스크 프리셋 테이블 비중첩 — **전부 통과**.
- 전체 스위트 352개(Phase 2 대비 +11) — **347 통과 / 5 실패**, 실패 5개는 Phase 1/2와 동일한 기존 무관 테스트, 새 회귀 없음.

**다음 단계**: 여기까지가 계약서 §C가 명시한 "Motion Mixer의 핵심 기술 가설이 검증된 지점" 직전 — Phase 4A(단일 뷰포트 공유 + MotionMode + bone hierarchy line 렌더링, `GL_LINES`)로 실제 화면 검증에 들어간다. RenderSystem이 아직 없는 상태(ROADMAP.md §5) 위에 얹히므로 착수 전에 그 전제를 재확인해야 한다.

---

## 부록 E — Phase 4A 완료 기록 (2026-08-15)

부록 A §C11/§C12를 구현했다. C++ 유닛테스트로만 검증하던 Phase 1~3과 달리 이번엔 실제 에디터를 띄워 화면으로 확인한 첫 Phase다.

**구현 파일**

| 파일 | 내용 |
|---|---|
| [engine/renderer/RenderMode.h](../engine/renderer/RenderMode.h) | `RenderMode{Scene, Motion}` - Renderer.h와 Engine.h가 공유하는 가벼운 헤더로 분리 |
| [engine/renderer/BoneLineRenderer.h/.cpp](../engine/renderer/BoneLineRenderer.h) | `GL_LINES`로 본 하나당 독립 선분 하나(§C12 - "line strip 아님") 그리는 디버그 렌더러. 그리드와 달리 매 프레임 정점 버퍼를 다시 채움(포즈가 바뀌므로) |
| [engine/animation/MotionPreviewState.h/.cpp](../engine/animation/MotionPreviewState.h) | §C11의 "Preview State". base 클립 + overlay 클립을 weight로 블렌드 - weight 0=Idle, 1=Attack, 0.5=블렌드로 §C12 검증 시나리오를 슬라이더 하나로 커버 |
| `Renderer`/`Engine` 확장 | `SetRenderMode`/`GetRenderMode`, `SetMotionPreviewState`/`GetMotionPreviewState`. Motion 모드일 때 grid 위에 본 라인을 추가로 그림(카메라는 Scene과 공유, §C12) |
| [engine/bindings/AnimationBindings.cpp](../engine/bindings/AnimationBindings.cpp) (신규) | `RenderMode`/`MaskPreset` enum, `MotionPreviewState` 클래스를 Python에 노출 - Python은 weight/skeleton/clip 전달까지만 하고 블렌딩 연산은 재구현하지 않음(§C: "UI blending 로직 재구현 금지") |
| [engine/assets/motion/](../engine/assets/motion/) (신규) | `sample.skeleton.json`(7본 바이페드), `idle.clip.json`(빈 트랙 - 바인드 포즈 그대로), `attack.clip.json`(ArmR/Spine 애니메이션) - Phase 4A 시연용 |
| `main.py`, `motion_editor.py`, `panels/animation_preview.py` | Motion Editor가 Scene Editor와 **같은 `EngineViewport` 인스턴스**를 재사용하도록 재구성(§C11: "단일 EngineViewport 인스턴스"). `_set_mode()`가 모드 전환 시 뷰포트를 `main_splitter`↔Motion Editor 사이로 재부모 이동시키고(자리는 placeholder로 채움), `SetRenderMode(Scene/Motion)`을 같이 전환. `AnimationPreviewPanel`에 Idle↔Attack 블렌드 슬라이더 + 샘플 로드 버튼 추가(기존 2D 더미 프리뷰는 엔진 없을 때의 폴백으로 유지) |

**진행 중 발견한 실제 버그와 수정 (Motion Mixer 범위 밖이지만 이번에 드러남)**

Motion Editor로 전환하면(=뷰포트가 더 작은 레이아웃으로 재부모 이동하면) 화면이 완전히 검게 나오는 문제가 있었다. 원인: `glViewport`가 `CreateGraphicsContext()` 시점에 딱 한 번만 설정되고, 그 이후로는 창 크기가 바뀌어도 절대 갱신되지 않고 있었다. 게다가 `EngineViewport`가 감싸는 HWND는 Qt가 만든 네이티브 창이라 `Win32Platform`의 `WindowProc`을 거치지 않으므로, 엔진이 자체적으로 `WM_SIZE`를 받을 방법이 원래 없었다.

- [engine/core/Engine.cpp](../engine/core/Engine.cpp) (`TickFrame`, 신규 `HandleWindowResize`): 비동기 입력 큐로 들어온 `WindowResize` 이벤트를 처리해 `glViewport` + 기본 카메라 종횡비를 갱신하도록 추가.
- [engine/editor/viewport.py](../engine/editor/viewport.py): `EngineViewport.resizeEvent()`를 신규 오버라이드해 Qt 위젯 크기가 바뀔 때마다(레이아웃 재배치 포함) `PushInputEvent(WindowResize)`로 엔진에 알려주도록 함 - 마우스/키보드 이벤트가 이미 쓰던 것과 같은 스레드세이프 경로.

이 수정이 없으면 Motion Editor뿐 아니라 **에디터 창 자체를 리사이즈해도** 지금까지 렌더링이 예전 크기 기준으로 잘못 그려졌을 것이다(그리드 렌더링을 포함해 이제껏 아무도 창 크기를 바꿔가며 확인하지 않았던 것으로 보임).

**검증 결과**
- `ge_engine.dll`/`ge_python.pyd` 정상 빌드. 신규 유닛테스트 9개(`MotionPreviewStateTest`) 전부 통과 - §C12 검증 시나리오(Idle frame 0 / Attack frame 30 / Blend 0.5)를 순수 로직 레벨로 그대로 재현해 확인. 전체 스위트 361개 - 356 통과/5 실패(Phase 1~3과 동일한 기존 무관 실패, 새 회귀 없음).
- 에디터 실행 검증: Scene Editor에서 그리드 정상 렌더링 확인(스크린샷). Motion Editor 탭 클릭 시 로그로 `[MotionPreview] 샘플 스켈레톤/클립 로드 완료`를 확인 - JSON 파싱→`Skeleton::Create`→`MixLayers`까지 이어지는 Python↔C++ 전체 경로가 실제로 동작함을 확인. Motion Editor 화면(믹서 테스트 바 - "샘플 로드" 버튼 + Idle/Attack 슬라이더)이 정상 렌더링되는 것도 스크린샷으로 확인. **본 라인이 실제로 눈에 보이는 것까지는 스크린샷으로 재확인 못함** - 같은 기기에서 사용자가 동시에 창을 직접 조작하며 확인 중이라 자동 스크린샷 타이밍이 여러 번 어긋났음(창이 자동 스크린샷 시도 도중 닫힘). 사용자가 직접 화면에서 "재대로 되기는하네"(제대로 되기 시작했다)로 확인함 - resize 수정 이후의 상태.

**남은 것**
- 본 라인이 실제로 화면에 잡히는 것은 이번 세션 자동화로는 재확인 못 했다(위 사유). 다음 세션에서 한 번 더 스크린샷으로 교차 확인하는 게 안전하다.
- `AnimationPlayer`(재생 시간/속도)는 Phase 4A에서 의도적으로 안 씀 - `MotionPreviewState`가 프레임을 직접 지정하는 형태. Phase 5(믹서 UI)에서 실제 재생 타이머와 연결해야 함.
- Motion Editor를 벗어났다가 다시 들어가도(`_set_mode` 반복 호출) 뷰포트 재부모 이동이 매번 문제없이 도는지는 1회 확인뿐 - 여러 번 왕복하는 스트레스 테스트는 아직 안 함.

**다음 단계**: Phase 5 — 믹서 UI (§C 사전조건: Phase 3 + 4A 완료 - 둘 다 됨). Section Inspector의 "Animation Layers"를 실제 편집 가능하게, 호환 클립만 dropdown 노출, weight slider, 블렌드 결과 실시간 반영.

---

## 부록 F — Phase 5 완료 기록 (2026-08-15)

부록 A §C(Phase 5) 및 §E 체크리스트 18번(사전조건: Phase 3 + 4A 완료)을 만족한 상태에서 시작했다.

### 설계 변경: "base + overlay 1개" → "base + 레이어 N개 스택"

Phase 4A의 `MotionPreviewState`는 §C12 검증 시나리오(Idle/Attack/Blend 0.5)만 만족하면 됐기 때문에 overlay 클립을 1개만 가질 수 있었다. Phase 5의 완료 기준("서로 다른 두 개 이상의 임포트된 모션을 레이어로 얹고 weight를 조절")은 이 구조로는 못 만족해서, `LayerMixer::MixLayers`가 원래부터 받던 `vector<LayerSpec>`을 그대로 살려 `MotionPreviewState`도 레이어 벡터를 갖도록 확장했다.

레이어는 벡터 인덱스가 아니라 단조증가하는 **id**로 식별한다(`AddLayer`가 id를 반환, `RemoveLayer(id)`/`SetLayer*(id, ...)`). 인덱스로 식별했다면 중간 레이어를 지울 때 UI가 나머지 레이어들의 인덱스를 전부 다시 맞춰야 했는데, 그 자체가 버그 소지가 크다고 판단해 처음부터 피했다(`RemoveLayer_ById_DoesNotDisturbOtherLayers` 테스트로 고정).

### 구현 파일

| 파일 | 내용 |
|---|---|
| [engine/animation/MotionPreviewState.h/.cpp](../engine/animation/MotionPreviewState.h) | `LoadOverlayClip`/`SetOverlayFrame`/`SetOverlayWeight`/`SetOverlayMask`/`HasOverlayClip` 제거, `AddLayer`/`RemoveLayer`/`ClearLayers`/`GetLayerCount`/`SetLayerFrame`/`SetLayerWeight`/`SetLayerMask`/`CheckClipCompatible`로 교체. `ComputeBoneWorldLines()`는 이제 레이어 전체를 `vector<LayerSpec>`으로 모아 `MixLayers` 한 번만 호출(레이어 수와 무관하게 블렌드 로직은 그대로 재사용) |
| [engine/bindings/AnimationBindings.cpp](../engine/bindings/AnimationBindings.cpp) | 위 API 변경에 맞춰 Python 바인딩 갱신 |
| [engine/editor/panels/motion_mixer.py](../engine/editor/panels/motion_mixer.py) (신규) | `MotionMixerPanel`/`LayerRowWidget` - 실제 믹서 UI. `assets/motion/`을 스캔해 스켈레톤/클립 드롭다운을 채우고, 스켈레톤 로드 후에는 `CheckClipCompatible()`로 호환 안 되는 클립을 드롭다운에서 제외한다(별도 호환성 판정을 Python에서 다시 구현하지 않고 Phase 3의 §C10 검증 경로를 그대로 재사용). 레이어 행마다 weight[0,1]/frame/mask(Full·Upper·Lower) 컨트롤. 디스크 임의 위치의 커스텀 `*.skeleton.json`/`*.clip.json`을 "가져오기" 가능(유저 커스텀 바이페드/키프레임 임포트 경로) |
| [engine/editor/panels/animation_preview.py](../engine/editor/panels/animation_preview.py) | Phase 4A의 단일 슬라이더 "믹서 테스트 바"를 제거하고 `MotionMixerPanel`을 장착. `activate_motion_preview()`가 아직 아무 것도 안 실려있으면 `mixer_panel.load_default_sample()`(스켈레톤+Idle 베이스+Attack 레이어 1개, weight 0.5)을 자동 로드해서 처음 진입해도 곧바로 뭔가 보이게 함 |
| [engine/assets/motion/wave.clip.json](../engine/assets/motion/wave.clip.json) (신규) | ArmL을 흔드는 클립 - Attack(ArmR)과 동시에 얹어서 "레이어 2개가 서로 다른 본에 동시에 적용된다"는 걸 눈으로 보여주는 용도 |
| [engine/tests/MotionPreviewStateTests.cpp](../engine/tests/MotionPreviewStateTests.cpp) | Overlay 관련 3개 테스트를 레이어 스택 API로 재작성 + id 안정성(`RemoveLayer_ById_...`)/`CheckClipCompatible`/2-레이어 순서 있는 override(`TwoLayersStack_LastAddedWins`) 신규 3개 = 순증 3개 |

### 진행 중 발견한 실제 버그와 수정 (Motion Mixer 범위 밖이지만 이번에 드러남)

에디터를 실제로 띄워 Motion Editor로 전환하고 액션 데이터 변경 시그널이 발생하는 경로를 타보니, `motion_editor.py`의 `_on_data_changed()`가 `self.preview.canvas.update()`를 무조건 호출하고 있었다. `canvas`는 공유 뷰포트가 붙어있는 동안(Phase 4A부터, §C11) 항상 `None`이므로 `AttributeError`로 죽는다 - Section Inspector나 Timeline에서 데이터가 바뀔 때마다(예: 섹션 추가) 터지는 잠재 크래시였다. `if self.preview.canvas is not None:`으로 가드해서 수정. ([engine/editor/motion_editor.py](../engine/editor/motion_editor.py))

### 검증 결과

- `ge_engine.dll`/`ge_python.pyd` 정상 빌드. `MotionPreviewStateTest` 12개(레이어 스택 신규/재작성분) 전부 통과. 전체 스위트 364개 - **359 통과/5 실패**(Phase 1~4A와 동일한 기존 무관 실패, 새 회귀 없음).
- Python 임포트 스모크 테스트: `panels.motion_mixer`/`panels.animation_preview`/`motion_editor` 모두 `ge_python`이 로드된 상태(`HAS_ENGINE=True`)로 정상 임포트.
- **실제 에디터 실행 검증(스크린샷)**: 클릭 시뮬레이션 대신 `EditorMainWindow`를 직접 인스턴스화해 `_set_mode(2)`(Motion Editor 전환) → `mixer_panel`에 wave 레이어 추가 → weight/frame 조절 → 레이어 제거 → 재추가를 코드로 직접 구동하는 검증 스크립트로 재현했다(사용자가 같은 데스크톱에서 동시에 앱을 테스트하고 있어 클릭 좌표 기반 자동화는 타이밍이 계속 어긋났음 - 사용자 본인이 직접 확인). 로그로 모든 단계가 예상대로 동작함을 확인:
  - 모드 전환 후 `HasSkeleton=True HasBaseClip=True LayerCount=1`(자동 샘플 로드)
  - `CheckClipCompatible(wave.clip.json) = True`(같은 스켈레톤이라 호환)
  - 레이어 추가 후 `LayerCount=2`, UI 행도 2개
  - 존재하지 않는/이미 지운 레이어 id에 대한 `SetLayerWeight`는 전부 `False`(안정적인 id 계약이 실사용 경로에서도 성립)
  - 레이어 제거 후 `LayerCount` 정확히 감소, UI 행도 동기화
  - 스크린샷(PrintWindow 캡처)으로 Motion Editor 화면 자체를 확인: 뷰포트에 노란 본 라인이 그려져 있고, 그 아래 믹서 패널에 Skeleton/Base/+Layer 드롭다운과 `attack.clip.json`(weight 0.80, frame 30, mask Full Body)/`wave.clip.json (verify)` 레이어 행이 정상적으로 배치되어 있음. Section Inspector(우측 패널)는 이번 Phase에서 손대지 않아 기존 그대로임도 함께 확인.

### 의도적으로 손대지 않은 부분

- **`section_inspector.py`의 기존 "Animation Layers"(ActionData 기반)는 그대로 뒀다.** 계약서 §Phase5 대상 파일 목록엔 이 파일도 있었지만, 그 UI가 편집하는 `ActionData.layers[].clip_name`은 여전히 실제 파일을 가리키지 않는 단순 문자열이라(§5 리스크 문서화됨), Motion Mixer의 실시간 뷰포트 프리뷰(`MotionPreviewState`)와 데이터 모델이 분리되어 있다. 이 둘을 억지로 잇는 대신, 신규 `motion_mixer.py`가 실제로 동작하는 믹서 UI를 전담하도록 했다 - "완료 기준"(호환 클립 dropdown + weight slider + 실시간 반영)은 이 경로로 충족된다. `ActionData.layers[].clip_name`을 실제 클립 파일과 잇는 건 여전히 열린 질문으로 남는다.
- `AnimationPlayer`(재생 시간/속도/루프)는 여전히 안 씀 - 레이어별 frame은 슬라이더로 직접 지정하는 정적 프리뷰. 재생 타이머 연동은 다음 후속 작업(예: Phase 4B 이후) 대상.
- `MaskPreset::Custom`(per-bone 커스텀 가중치 맵)은 UI에 노출 안 함 - Full/UpperBody/LowerBody 3개만. Custom은 본별 가중치 편집 UI가 따로 필요해서 범위 밖으로 미룸.

**다음 단계**: Phase 4B — CPU 스키닝. `*.mesh.json`(정점 + 본-웨이트) 포맷 신설이 선행되어야 하고, Phase 4A의 bone hierarchy line 위에 같은 Scene API로 skin 결과를 얹는다(부록 A §C 순서표 그대로).
