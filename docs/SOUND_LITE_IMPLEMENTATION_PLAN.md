# Sound Lite 구현 계획서

**작성일**: 2026-09-10
**목표**: [docs/SOUND_LITE_PLAN.md](SOUND_LITE_PLAN.md)에서 확정한 5개 값짜리 사운드 시스템을, 이미 존재하는 이벤트 타임라인·액션 직렬화·재생 타이머 위에 얹는다. GUI에서 오디오 파일을 고르고, 액션의 특정 프레임에 붙이고, **Motion Editor 미리보기와 Scene Editor Play 양쪽에서** 들어본다.

> **선행 문서**: 범위·철학은 [docs/SOUND_LITE_PLAN.md](SOUND_LITE_PLAN.md)가 확정했고 이 문서는 그것을 바꾸지 않는다. 특히 §2의 5개 값과 §3의 제외 목록은 그대로 따른다.

> **확정된 결정 (범위 정의서 §6)**: (b) Scene Editor Play에서도 **파이썬이** 액션을 재생 / (b2) 전용 `ActionPlayerComponent`(필드 3개) 신설 / 게임 빌드 오디오는 **v1 제외** / **WAV만** / 클립 경로는 **`.action.json` 파일 위치 기준 상대 경로**.

---

## 1. 배경 — 이 계획이 새로 만드는 것은 "잇는 코드"뿐이다

범위 정의서 §1의 실측을 요약하면, 여섯 조각이 이미 있고 그것들이 서로 연결되어 있지 않다.

```text
[있음] EventType.SOUND + UI 마커         [없음] 이벤트 파라미터 편집 UI
[있음] ActionEvent.params (자유형 dict)   [없음] 클립 임포트(파일 선택)
[있음] .action.json 직렬화(params 포함)   [없음] 프레임 → 이벤트 발화 규칙
[있음] _on_tick() 재생 루프(Motion)       [없음] 오디오 재생 래퍼
[있음] _on_fps_tick() 프레임 틱(Scene)    [없음] "이 엔티티가 이 액션을 재생"
[있음] QSoundEffect (PySide6)             [없음] Scene Play의 액션 재생 루프
```

**자료구조도 파일 포맷도 바꾸지 않는다.** VFX Lite가 "새 `.vfx.json`을 발명하지 않는다"고 결정해서 얻었던 것이, 여기서는 이미 성립해 있다(범위 정의서 §4.1).

### 1.1 착수 전 확인된 사실 (실측)

- **`_on_tick()`** — [engine/editor/panels/animation_preview.py](../engine/editor/panels/animation_preview.py). Motion Editor 재생 타이머가 프레임을 전진시키는 **유일한 곳**. `next_frame > total_frames`면 0으로 되감고 `frame_changed`를 emit한다. `set_action()`이 액션의 `fps`로 타이머 간격을 맞춘다.
- **`_on_fps_tick()`** — [engine/editor/main.py](../engine/editor/main.py). 약 60fps로 도는 **파이썬 쪽 유일한 프레임 틱**이고, `viewport.tick()`과 상태바 갱신을 한다. `is_playing`/`_world`를 같은 클래스가 소유하므로 **Scene Editor Play의 액션 재생을 붙일 자리가 여기다.**
- **`ActionData.to_dict()/from_dict()`** — 이벤트의 `params`를 그대로 직렬화/복원한다. `save_action()`/`load_action()`도 있다.
- **`QFileDialog` 사용 관례** — `motion_editor.py`의 `do_save_action`/`do_load_action`이 `getSaveFileName`/`getOpenFileName`에 `"Action Files (*.action.json)"` 필터를 쓴다. 클립 임포트도 같은 형태로 간다.
- **파이썬 테스트 관례** — `engine/editor/test_*.py`에 `def test_*()`를 두고 `if __name__ == "__main__"`에서 직접 돌린다(pytest 미사용). Phase 1의 유닛테스트는 이 형식을 따른다.
- **컴포넌트 추가 경로** — `editor_api.add_component(entity, "ComponentName")`이 **이름 문자열**로 동작한다(리플렉션 등록만 하면 Inspector 폼도 자동 생성). VFX Lite Phase 1에서 확인된 그대로다.
- **`AIComponent`의 action 필드들** — `idle_action`/`attack_action`/`hit_action`과 `AIState`가 존재하지만 **읽는 시스템이 없다.** (b2) 확정에 따라 **이번 작업에서 건드리지 않는다.**

---

## 2. 설계 결정

### 2.1 발화 규칙은 재생 루프 밖의 순수 함수로 분리한다

범위 정의서 §6.2.2대로 재생 루프가 둘이 된다(Motion Editor / Scene Editor Play). **발화 지점이 둘이 되는 것이지 발화 규칙이 둘이 되면 안 된다.**

`core/action_playback.py`에 Qt도 오디오도 모르는 순수 로직을 둔다:

```python
class ActionPlaybackCursor:
    """액션 하나의 재생 위치를 들고, 프레임이 전진할 때 '이번에 발화할 이벤트'를 돌려준다.
    Qt/오디오/엔진을 전혀 모른다 - 그래서 유닛테스트로 전부 닫힌다."""
    def __init__(self, action: ActionData, loop: bool = True): ...
    def advance(self, frames: int = 1) -> list[ActionEvent]: ...
    def seek(self, frame: int) -> None:   # 소리를 내지 않는다(§5.4)
    def reset(self) -> None: ...
```

- **`advance()`만 이벤트를 돌려준다.** `seek()`은 절대 돌려주지 않는다 — 스크럽/버튼 이동에서 소리가 나지 않게 하는 규칙(§5.4)을 **자료구조 수준에서** 강제한다.
- 발화 조건은 `이전 프레임 < e.frame <= 새 프레임`. dt가 커서 프레임을 여러 개 건너뛰어도 사이에 낀 이벤트를 놓치지 않는다.
- 루프 되감기에서는 "끝까지 → 0부터 새 프레임까지" 두 구간을 이어 계산하고, **한 바퀴 안에서 같은 이벤트를 두 번 돌려주지 않는다**(§5.5).
- 커서는 `EventType`을 필터링하지 않는다. **필터는 호출부가 한다** — v1은 `Sound`만 소비하지만(§3), 커서 자체는 그 결정을 몰라도 된다.

### 2.2 `SoundPlayer` — 오디오 재생 래퍼 하나

`core/sound_player.py`. `QSoundEffect`를 감싸고 **재생 호출부를 이 한 곳으로 모은다** — 나중에 엔진 오디오가 생기면 이 안쪽만 바뀐다(범위 정의서 §4.3).

```python
class SoundPlayer:
    def preload(self, clip_paths: Iterable[str]) -> None: ...   # 액션 열 때 1회
    def play(self, clip: str, volume: float, pitch: float) -> bool: ...
    def stop_all(self) -> None: ...                             # Stop/모드 전환 시
```

- **프리로드**(§5.3): 액션을 열 때 그 액션이 참조하는 클립을 전부 로드해둔다. `play()` 안에서는 파일 I/O도 디코딩도 하지 않는다 — 재생 시점의 디스크 접근은 곧 "틀린 타이밍"이다.
- **보이스 상한**(§5.2): 동시에 울리는 소리 개수에 상한을 두고, 초과 시 **신규 재생을 거부**한다. 재생 중인 소리를 끊지 않는다. 상한은 엔진 정책 상수이고 §2의 5개 값에 넣지 않는다.
- **실패는 조용히 건너뛴다**(§5.6): 파일이 없거나 포맷이 안 맞으면 `False`를 돌려주고 로그만 남긴다. **액션 재생 자체는 계속된다.**
- `pitch`는 `QSoundEffect`에 직접 대응하는 속성이 없을 수 있다 — Phase 2에서 실제 확인하고, 없으면 §6에 적은 대로 처리한다.

### 2.3 `ActionPlayerComponent` — 필드 3개 (C++)

(b2) 확정. `ecs/Components.h`에 POD로 두고 `ecs/Reflection.cpp`에 등록한다(VFX Lite Phase 1과 같은 자리·같은 방식).

| 필드 | 타입 | 의미 |
|---|---|---|
| `action` | String | 재생할 `.action.json` 경로 |
| `playOnStart` | Bool | Play 진입 시 자동 재생 |
| `loop` | Bool | 끝나면 처음으로 되감기 |

- **`FieldType::Enum`을 쓰지 않는다** — 매크로가 만드는 직렬화 switch에 `Enum` case가 없어 조용히 누락된다(VFX Lite §7.3에서 확인). 3개 필드 모두 String/Bool이라 해당 없음.
- 등록만 하면 **Inspector 폼은 자동으로 나온다**(VFX Lite Phase 1에서 확인된 성질).
- `AIComponent`의 action 필드는 건드리지 않는다. 나중에 AI 시스템이 생기면 그때 연결을 정한다.

> 이 컴포넌트는 §2의 5개 **사운드 값이 아니라 별개 축**이다. 사운드 파라미터가 6개가 된 것이 아니다.

### 2.4 Scene Editor Play의 재생 루프

`main.py`의 `_on_fps_tick()`에 붙인다(§1.1).

```text
_on_fps_tick()                       # 이미 ~60fps로 돌고 있음
   └─ if is_playing:
        각 ActionPlayerComponent 엔티티의 커서를 dt만큼 advance()
            └─ 돌아온 Sound 이벤트 → SoundPlayer.play()
```

- **Play 진입 시**: `playOnStart`인 엔티티의 액션을 로드하고 커서를 만들고 클립을 프리로드한다.
- **Stop 시**: 커서를 버리고 `stop_all()`.
- **Pause 시**: 커서를 전진시키지 않는다(소리도 안 난다).
- `_on_fps_tick`은 60fps 고정이고 액션의 `fps`는 다를 수 있다. **실제 경과 시간(dt)으로 프레임을 전진**시켜 액션의 `fps`를 존중한다 — 타이머 틱 수를 프레임 수로 쓰면 30fps 액션이 두 배 빨리 재생된다.

### 2.5 클립 임포트 UI

이벤트 타임라인에서 Sound 이벤트를 선택하면 나오는 파라미터 편집부에 "Clip" 행을 두고, 버튼으로 `QFileDialog.getOpenFileName(..., "Sound Files (*.wav)")`를 연다(§6.3의 WAV 확정을 필터로 표현).

- 고른 경로는 **`.action.json` 위치 기준 상대 경로로 변환해서** `params.clip`에 넣는다(§6.4).
- 액션이 아직 저장된 적이 없으면 기준 폴더가 없으므로, 일단 절대 경로로 들고 있다가 **저장 시점에 상대 경로로 다시 계산**한다.
- **파일을 복사하지도 변환하지도 않는다**(§4.4).

### 2.6 이벤트 파라미터 편집

`ActionEvent.params`는 자유형 dict지만 UI는 **Sound 이벤트일 때 5개 값만** 보여준다(Clip/Volume/Pitch/Pitch ±/Volume ±). 다른 EventType의 params 편집은 이번 범위가 아니다(§3의 "다른 EventType의 런타임 발화"와 같은 선).

---

## 3. Phase 분해

각 Phase는 CLAUDE.md 관례 #2에 따라 **컴파일 / 유닛테스트 / 실제 실행 검증**을 구분해 기록한다.

### Phase 1 — `ActionPlaybackCursor` — ✅ 유닛테스트 통과 (2026-09-10)

> **검증 단계**: **유닛테스트 통과**까지(10개). 기존 파일 수정 0건 — 새 파일 2개만 추가했으므로 회귀 위험이 없다. `core/action_playback.py`, `test_action_playback.py`.
>
> **테스트가 구현 버그 2개를 잡았다** — Phase 1을 오디오/렌더링 없이 먼저 닫아두는 이유가 이것이다: (1) 끝 프레임에 정확히 서 있으면 남은 공간이 0이라 되감기 후에도 전진하지 못하고 **루프 두 번째 바퀴가 통째로 무음**이 됐다 — "소리가 가끔 안 난다"로 나타났을 종류다. (2) `loop=False`로 끝에 **정확히 딱 맞게** 도달하면 `finished`가 서지 않았다(분기마다 따로 세우다 그 경로만 빠졌다 — 루프 밖에서 한 번에 판정하도록 고쳤다).

아래는 착수 전 계획 원문이다.

§2.1. Qt도 오디오도 엔진도 필요 없다. **이 Phase가 발화 규칙 전체를 닫는다.**

- **유닛테스트**(`engine/editor/test_action_playback.py`, 기존 `test_*.py` 형식):
  - `advance(1)`이 그 프레임의 이벤트만 돌려준다
  - 여러 프레임을 한 번에 건너뛰어도 **사이에 낀 이벤트를 놓치지 않는다**
  - `seek()`은 **절대** 이벤트를 돌려주지 않는다(§5.4)
  - 루프 되감기에서 경계 이벤트가 **두 번 발화하지 않는다**(§5.5)
  - `loop=False`면 끝에서 멈추고 더 이상 이벤트를 돌려주지 않는다
  - 이벤트가 없는 액션, `total_frames=0`, 같은 프레임에 이벤트 여러 개

### Phase 2 — `SoundPlayer` — ✅ 유닛테스트 통과 (2026-09-10, 가청 검증은 Phase 3)

> **검증 단계**: **유닛테스트 통과**까지(8개). 기존 파일 수정 0건 — 새 파일 2개만 추가(`core/sound_player.py`, `test_sound_player.py`). 테스트용 WAV는 stdlib `wave`로 즉석 생성해 외부 에셋에 의존하지 않는다.
> 커버: WAV 프리로드/재생 / 없는 파일이 예외가 아니라 `False` / **WAV 아닌 파일이 `Status.Error`로 거부됨** / 보이스 상한이 신규만 거부(`[True, True, False, False]`) / 같은 클립 재트리거는 상한 검사 제외 / Volume ±가 0..1 유지 / `stop_all`·`clear` / `collect_clip_paths`의 상대 경로 해석(§6.4).
>
> **가청 검증은 못 했다** — §6의 노트대로 `play()`의 True는 "클립이 Ready였고 재생을 요청했다"는 뜻이지 스피커에서 소리가 났다는 확인이 아니다. Phase 3에서 사람이 듣고 확인해야 한다.

아래는 착수 전 계획 원문이다.

§2.2. **먼저 확인할 것**: `QSoundEffect`가 실제로 WAV만 받는지, `pitch`에 해당하는 속성이 있는지. 문서 기준으로만 알고 있으므로 **실측한다**(§6.3).

- 프리로드 / 보이스 상한 / 실패 시 `False` + 로그.
- **검증**: 테스트용 WAV 하나로 실제 소리가 나는지(스피커 확인). 없는 파일·잘못된 포맷에서 예외가 아니라 `False`가 나오는지.
- 이 Phase까지는 액션과 무관하다 — 단독으로 검증 가능하다.

### Phase 3 — Motion Editor 미리듣기 연결 — ✅ 통합 테스트 통과 (2026-09-10, **가청 확인은 사람 몫**)

> **검증 단계**: **통합 테스트 통과**까지(7개, `test_motion_sound_integration.py`). Phase 3의 관문 네 가지 중 **가청을 뺀 세 가지**를 패널을 직접 구동해 자동 확인했다 — (2) 스크럽/이전·다음 버튼으로는 재생 요청이 **0건** (3) 루프 3바퀴에서 이벤트 2개가 정확히 6번(경계 중복 없음) (4) 없는 클립·clip 없는 이벤트가 섞여도 재생이 끝까지 진행됨. 더불어 상대 경로 해석(§6.4)과 액션 교체 시 이전 클립 정리도 고정했다.
>
> **(1) 가청 확인 — ✅ 완료 (2026-09-10, 사용자가 직접 청취)**. 에디터를 `HAS_ENGINE=True`로 띄우고 `Motion → Load Action…`으로 `assets/actions/SoundTest.action.json`(30fps, 프레임 5·15·25에 Sound 이벤트)을 열어 재생한 결과 소리가 정상적으로 났다. 프로그램으로는 확인할 수 없는 항목이라 **사람의 청취가 유일한 검증 수단**이었다.
>
> **기존 파일 수정은 `panels/animation_preview.py` 한 곳**이다(import 3줄, `__init__` 2줄, `set_action`/`set_current_frame`/`_on_tick`/`_on_play_toggle`). `set_action(action, base_dir=None)`은 기본값이 있어 **기존 호출부가 그대로 동작한다.**
>
> **통합에서 Phase 1의 설계 오류가 드러났다**: 커서의 "한 바퀴"가 `total` 프레임이었는데, 에디터의 기존 `_on_tick()`은 `total`에서 다음 틱에 0으로 가며 **프레임 0을 실제로 표시**한다(한 바퀴 = `total + 1`틱). 커서가 `_current_frame`의 원천이 되므로 어긋나면 재생 위치 표시가 기존과 달라진다. 커서를 **"새로 도달한 프레임이 자기 이벤트를 발화시킨다"** 모델로 바꿔 기존 동작에 맞췄고, 프레임 순서가 기존 계산과 일치하는지 비교하는 테스트를 추가했다. 이 모델에서는 각 프레임을 한 바퀴에 한 번씩만 방문하므로 §5.5(루프 중복 발화 금지)가 **자동으로** 지켜진다.

아래는 착수 전 계획 원문이다.

`_on_tick()`이 `ActionPlaybackCursor.advance()`를 쓰고, 돌아온 Sound 이벤트를 `SoundPlayer.play()`로 넘긴다. 액션을 열 때 클립을 프리로드한다.

- **실제 실행 검증**: (1) 재생 중 해당 프레임에서 소리가 난다 (2) **타임라인을 드래그하면 소리가 나지 않는다**(§5.4) (3) 루프해도 경계에서 두 번 나지 않는다(§5.5) (4) 클립이 없는 이벤트가 있어도 재생이 멈추지 않는다.
- 이 시점에서 **"모션 에디터에서 사운드 적용"이 완성된다.**

### Phase 4 — 이벤트 파라미터 편집 + 클립 임포트 UI — ✅ 테스트 통과 (2026-09-10, **가청 확인 대기**)

> **새 패널을 만들지 않았다.** `SectionInspectorPanel`이 이미 섹션/레이어를 **행 위젯 목록**으로 편집하는 패턴을 갖고 있어, `SoundEventRowWidget` + "SOUND EVENTS" 블록을 같은 방식으로 붙였다. 덕분에 `motion_editor.py`의 배선은 **한 줄도 바뀌지 않았다**(`inspector.data_changed`가 이미 연결돼 있다). `event_selected` 시그널 같은 새 개념도 필요 없었다.
>
> **클립 경로 처리(§6.4)**: 파일을 고르는 시점에는 그 액션이 어느 폴더에 저장될지 모르므로 **절대 경로로 들고 있다가, 저장 시점에** `save_action()`이 `.action.json` 기준 상대 경로로 계산한다(`relativize_clip_paths`). 상대 경로를 만들 수 없으면(다른 드라이브 등) 절대 경로를 그대로 둔다 — 깨진 상대 경로보다 낫다.
>
> **배선을 없앤 결정**: `ActionData.source_path`(직렬화하지 않는 필드)를 추가해 **액션이 자기 출처를 들고 다니게** 했다. 그래서 `AnimationPreviewPanel.set_action(action)`이 `base_dir`를 넘겨받지 않아도 스스로 알아내고, 패널마다 경로를 배선할 필요가 사라졌다. 파일 안에 자기 경로를 적지 않는 이유는 파일을 옮기는 순간 거짓말이 되기 때문이다.
>
> **검증 단계**: **테스트 통과**까지(7개, `test_sound_event_editing.py`). 행 편집이 `params`에 반영되는지 / 다른 타입이 섞인 `events`에서 Sound만 **객체로** 지워지는지(인덱스로 지우면 엉뚱한 게 지워진다) / 저장 시 `../sfx/x.wav` 형태로 상대화되는지 / `source_path`가 직렬화되지 **않는지** / 저장→로드 후 프리뷰가 클립을 실제로 로드하는지.
>
> **가청 확인 — ✅ 완료 (2026-09-10, 사용자가 직접 청취)**. 확인용 샘플로 `engine/assets/sfx/Blip.wav`와 `engine/assets/actions/SoundTest.action.json`(30fps, 프레임 5·15·25에 Sound 이벤트, 클립은 `../sfx/Blip.wav` **상대 경로로 저장됨**)을 만들어 뒀고, 이를 열어 재생해 소리를 확인했다 — §6.4의 상대 경로 처리가 실제 파일에서 동작한다는 증거이기도 하다. `QFileDialog`로 파일을 고르는 순간 자체는 자동화하지 않았다.
>
> **함께 드러난 UI 문제(미해결)**: `SectionInspectorPanel`은 세로 공간이 좁은데(`right_v_splitter`가 40%만 배정) 이제 SECTIONS / ANIMATION LAYERS / **SOUND EVENTS** 세 블록이 들어간다. SOUND EVENTS는 스크롤하거나 스플리터를 드래그해야 보인다. 그 40% 비율은 이전 세션이 "모션 에디터는 뷰포트와 모션 그래프 위주로"라며 **의도적으로 정한 것**(커밋 `f064c8b`)이라 블록 하나 추가했다고 임의로 되돌리지 않았다. 선택지는 (1) 기본 스플리터 비율 조정 (2) SOUND EVENTS를 이벤트 타임라인 쪽으로 이동 (3) 그대로 두고 스크롤 — **아직 결정하지 않았다.**

아래는 착수 전 계획 원문이다.

§2.5/§2.6. Sound 이벤트의 5개 값을 편집하고 클립을 파일 선택으로 고른다. 상대 경로 변환 포함.

- **검증**: 클립을 고르고 저장한 뒤 `.action.json`을 열어 **상대 경로로 들어갔는지** 확인. 다시 로드해도 소리가 나는지. 액션 파일과 클립을 **함께 다른 폴더로 옮겨도** 동작하는지(§6.4의 목적).

### Phase 5 — `ActionPlayerComponent` — ✅ 유닛테스트 통과 (2026-09-10)

> **검증 단계**: **유닛테스트 통과**까지(9개, `tests/ActionPlayerComponentTests.cpp`). 등록 여부 / **필드 정확히 3개**(늘어나면 액션 재생 기능이 사운드 범위 밖으로 번지고 있다는 신호) / Enum 미사용 회귀 가드 / 라운드트립 / **Windows 경로의 역슬래시·공백이 JSON을 거쳐 살아남는지**(액션 경로는 사용자가 파일 대화상자로 고른 실제 경로다) / 부분 JSON에서 나머지 필드가 기본값 유지 / `patchField` / 회귀 게이트 / **`AIComponent`의 action 필드를 침범하지 않는지**(§6.2.1의 (b2) 결정이 실제로 지켜지는지).
>
> 변경: `ecs/Components.h`, `ecs/Reflection.cpp`, `tests/CMakeLists.txt` + 새 테스트 파일. VFX Lite Phase 1과 같은 자리·같은 방식이라 Inspector 폼은 자동으로 나온다(Phase 6에서 함께 확인).

아래는 착수 전 계획 원문이다.

§2.3. VFX Lite Phase 1과 같은 형태의 작업이다.

- `ecs/Components.h` + `ecs/Reflection.cpp` 등록, `tests/`에 직렬화 라운드트립 테스트.
- **회귀 게이트**: 컴포넌트가 없는 엔티티의 직렬화 결과에 나타나지 않는지.
- Inspector 폼이 자동으로 나오는지는 Phase 6에서 함께 확인한다.

### Phase 6 — Scene Editor Play 재생 — ✅ 구현 + 유닛테스트 통과 (2026-09-10, **Play 가청 확인 대기**)

> **검증 단계**: **유닛테스트 통과**까지(10개, `test_scene_action_playback.py`). `registry`를 가짜 객체로 대신해 **엔진 없이** 돈다. 에디터에 `Sound Test` 엔티티(`ActionPlayerComponent` → `actions/SoundTest.action.json`, `playOnStart`/`loop`)를 만들어 두고 실행까지 확인했지만, **Play를 눌렀을 때 실제로 소리가 나는지는 아직 사람이 확인하지 않았다.**
>
> **§2.4의 fps 함정을 테스트로 고정했다** — 30fps 액션에 1초를 주면 정확히 30프레임이 지나야 한다. 틱 수를 프레임 수로 썼다면 60프레임이 지나 **두 배 빨리 재생**된다. 소수 프레임 누적(60fps 틱 → 30fps 액션은 0.5프레임씩)도 함께 고정했다 — 매번 버림하면 재생이 아예 멈춘다.
>
> **`main.py`의 수정은 다섯 곳, 각각 몇 줄**이다(import / `__init__` / `_on_play` / `_on_pause` / `_on_stop` / `_on_fps_tick`). 재생 로직은 `core/scene_action_playback.py`에 두어 `main.py`의 diff를 작게 유지했고, 그 덕에 Qt 없이 유닛테스트가 가능하다.
>
> **엔진 변경 0건** — `GetAllEntities()` + `GetComponentJson()`만으로 컴포넌트를 훑는다. 범위 정의서 §6.2의 (b) 확정("엔진에 이벤트 런타임을 만들지 않는다")이 실제로 지켜졌다.
>
> **착수 중 겪은 실수(기록용)**: `ActionPlayerComponent`를 C++에 추가한 뒤 **`quarterflying` 파이썬 바인딩(`.pyd`)을 다시 빌드하지 않아** 에디터가 `Unknown component type: ActionPlayerComponent`로 실패했다. C++ 테스트 타겟(`SimpleEngineTests`)만 빌드하고 바인딩을 빠뜨린 것이다. **C++ 컴포넌트를 추가하면 `--target quarterflying`도 함께 빌드해야 에디터가 인식한다.**

아래는 착수 전 계획 원문이다.

§2.4. `_on_fps_tick()`에 재생 루프를 붙인다.

- **실제 실행 검증**: (1) `ActionPlayerComponent`를 붙인 엔티티가 Play에서 액션을 재생하며 소리를 낸다 (2) Stop하면 소리가 멈춘다 (3) Pause에서 전진하지 않는다 (4) **30fps 액션이 두 배 빨리 재생되지 않는다**(§2.4의 dt 처리) (5) Scene Editor 모드로 돌아왔을 때 소리가 남아있지 않다.
- 이 시점에서 **처음 요구("임포트 → 모션/에디터 뷰에서 적용 → 플레이에서 확인")가 전부 충족된다.**

---

## 4. 진행 순서 요약

```text
Phase 1  ActionPlaybackCursor      ✅ 유닛테스트 11개        (2026-09-10)
   ↓
Phase 2  SoundPlayer               ✅ 유닛테스트 8개 + §6 실측 (2026-09-10)
   ↓
Phase 3  Motion Editor 연결        ✅ 통합 7개 + 가청 확인    (2026-09-10)
   ↓
Phase 4  파라미터 편집 + 임포트 UI  ✅ 테스트 7개 + 가청 확인  (2026-09-10)
   ↓
Phase 5  ActionPlayerComponent     ✅ 유닛테스트 9개 (C++)    (2026-09-10)
   ↓
Phase 6  Scene Editor Play 재생    ✅ 유닛테스트 10개 / ⏳ Play 가청 확인 대기
```

> **현재 상태 (2026-09-10)**: Phase 1~6의 구현과 자동 검증은 끝났다. 파이썬 테스트 43개 + C++ 테스트 9개가 통과하고, Motion Editor에서의 **가청은 사람이 확인했다**. 남은 것은 **Scene Editor의 Play에서 소리가 나는지 한 번 들어보는 것** 하나다 — 프로그램으로 확인할 수 없는 항목이라 그것만 미확인으로 남는다.
>
> **범위 정정 이력**: 착수 직전 실측에서 `QSoundEffect`에 음정 속성이 없다는 것이 확인되어 §2의 값이 **5개 → 3개**(Clip/Volume/Volume ±)로 줄었다. 값을 늘리는 게 아니라 줄이는 방향이라 문서 철학과 맞다.

Phase 1과 2가 각각 **발화 규칙**과 **소리 내기**를 따로 닫으므로, Phase 3에서 소리가 이상하면 원인이 "연결" 쪽임을 알고 시작할 수 있다. VFX Lite에서 Phase 1·2를 렌더링 없이 닫아둔 덕에 Phase 3의 원인 분리가 쉬웠던 것과 같은 의도다.

---

## 5. 이 계획이 명시적으로 미루는 것

범위 정의서 §3의 제외 목록(3D 오디오, 믹서/버스, DSP, BGM/크로스페이드, 스트리밍, 포맷 변환, 사운드 큐, 페이드, Sound 외 EventType 발화, 우선순위/스틸링)을 그대로 유지하며, 추가로:

| 미루는 것 | 이유 | 선행 조건 |
|---|---|---|
| 게임 빌드 오디오 | §6.1 확정 — 엔진 오디오 백엔드를 만들지 않는다 | 에디터 없이 실행되는 빌드가 실제로 필요해질 때 |
| `AIComponent`의 action 필드 활용 | (b2) 확정 — `AIState` 상태 전이는 AI 시스템 작업이고 사운드와 무관 | AI 시스템을 실제로 만들 때 |
| 엔티티별 사운드 볼륨/뮤트 | §2의 5개 값 바깥 | 필요해지면 |
| 액션 재생 속도 배율 | `ActionSection.speed`가 이미 있으나 이번 범위에서 해석하지 않는다 | 섹션 기반 재생을 구현할 때 |
| Sound 이벤트의 프리뷰 버튼(한 번 들어보기) | 편의 기능. Phase 3이면 재생으로 확인 가능 | 저작 중 불편이 실제로 확인되면 |

---

## 6. 착수 시 확인할 미확정 사실 — ✅ **전부 실측 완료 (2026-09-10)**

문서 기준으로만 알고 있던 것들. 틀렸을 때 설계가 바뀌는 지점이라 착수 직전과 Phase 2에서 확인했다.

1. **`QSoundEffect`의 포맷 제약** — ✅ WAV가 아닌 파일을 주면 `status`가 **`Status.Error`**가 된다(크래시가 아니다). 즉 §6.3의 "WAV만" 결정이 안전하고, 실패를 프로그램이 감지할 수 있다. 파일이 아예 없으면 `Status.Null`로 구분된다.
2. **`pitch`에 대응하는 속성이 있는가** — ✅ **없다.** `QSoundEffect`가 가진 것은 `volume`/`loopCount`/`source`/`muted`/`status`뿐이다. 예상대로 **(a)를 택해 §2의 값을 5개에서 3개로 줄였다**(범위 정의서 §2의 개정 참고). `QMediaPlayer.setPlaybackRate()`는 실제로 존재하지만 미디어 재생용이라 지연이 커서 택하지 않았다.
3. **동시 재생 시 인스턴스 처리** — ✅ `isPlaying()`이 있어 **울리는 개수를 셀 수 있다.** v1은 **클립 하나당 인스턴스 하나**로 간다. 같은 클립을 다시 트리거하면 겹쳐 나지 않고 재시작되는데(QSoundEffect 동작), 클립마다 풀을 두면 "클립당 몇 개"라는 새 값과 프리로드 비용이 생기므로 v1은 이 한계를 받아들인다. 같은 클립 재트리거는 새 보이스를 쓰는 게 아니므로 **보이스 상한 검사에서 제외**한다 — 안 그러면 상한이 1일 때 같은 소리를 두 번째부터 아예 못 낸다.

> **Phase 2에서 검증한 것과 못한 것**: 위 항목들과 보이스 상한 동작(`max_voices=2`에서 4개 시도 → `[True, True, False, False]`)은 확인했다. 다만 **"실제로 스피커에서 소리가 나는가"는 프로그램으로 확인할 수 없다** — `play()`가 True를 돌려준 것은 클립이 `Ready`였고 재생을 요청했다는 뜻이지 가청 확인이 아니다. **가청 검증은 Phase 3에서 사람이 듣고 확인해야 한다.**

---

## 부록 — 이 문서 작성 시 참고한 실측 근거

- [docs/SOUND_LITE_PLAN.md](SOUND_LITE_PLAN.md) — 5개 값, 제외 목록, §6의 확정 사항 전부
- [engine/editor/panels/animation_preview.py](../engine/editor/panels/animation_preview.py) — `_on_tick()`의 프레임 전진/루프 되감기/`frame_changed` emit, `set_action()`의 `fps` 기반 타이머 간격
- [engine/editor/main.py](../engine/editor/main.py) — `_on_fps_tick()`이 ~60fps로 도는 파이썬 쪽 유일한 프레임 틱, `_on_play`/`_on_pause`/`_on_stop`이 `World::Play/Pause/Stop` 호출, `is_playing`/`_world` 소유
- [engine/editor/core/action_data.py](../engine/editor/core/action_data.py) — `EventType.SOUND`, `ActionEvent(frame/type/params)`, `to_dict()`가 `params` 직렬화, `save_action()`/`load_action()`
- [engine/editor/motion_editor.py](../engine/editor/motion_editor.py) — `QFileDialog` 사용 관례(`getOpenFileName` + 확장자 필터), `frame_changed` 양방향 배선
- [engine/ecs/Components.h](../engine/ecs/Components.h) / [engine/ecs/Reflection.cpp](../engine/ecs/Reflection.cpp) — POD 컴포넌트 정의·등록 위치, `AIComponent`의 action 필드가 읽히지 않는다는 사실
- `engine/editor/test_*.py` — 파이썬 테스트 형식(pytest 미사용, `if __name__ == "__main__"` 직접 실행)
- [docs/VFX_LITE_IMPLEMENTATION_PLAN.md](VFX_LITE_IMPLEMENTATION_PLAN.md) — Phase 분해와 검증 단계 기록 형식. Phase 1에서 확인된 "리플렉션 등록만 하면 Inspector 폼이 자동 생성된다"와 "`FieldType::Enum`은 조용히 누락된다"

---

## 부록: Phase 6 실제 가청 확인 (2026-09-30)

- Phase 6 커밋(`2080d3c`) 시점에는 Scene Play의 소리가 실제로는 나지 않았다. preload 키(액션 파일 기준 절대 경로)와 play 키(params.clip 원본 상대 경로)가 달라서 모든 Sound 이벤트가 `[SoundPlayer] 프리로드되지 않은 클립(재생 건너뜀)`으로 건너뛰어졌다. 유닛 테스트의 가짜 SoundPlayer는 preload 여부를 따지지 않아서 잡지 못했다.
- `2c9c929`에서 `sound_player.resolve_clip_path()` 하나로 해석 규칙을 모아 고쳤고, 계약을 지키는 `StrictFakeSound` 회귀 테스트를 추가했다.
- **실제 실행 검증:** 에디터 Scene Play에서 Sound Test 엔티티의 소리가 스피커로 들리는 것을 사용자가 직접 확인했다(2026-09-30).
