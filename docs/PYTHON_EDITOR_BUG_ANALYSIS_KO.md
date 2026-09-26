# Quarter Flying — Python 에디터(engine/editor) 버그 분석 보고서

**작성일**: 2026-08-16
**분석 대상**: `engine/editor/` 이하 Python 소스 전체 (PySide6 기반 에디터, 약 3,800줄)
**분석 방법**: 정적 코드 리뷰 (전체 `.py` 파일 통독) + 저장소에 남아있는 부산물(config.json 사본 등)을 근거로 한 재현성 검증
**분석 범위**: `main.py`, `config_manager.py`, `demo_scene_integration.py`, `ge_transaction.py`, `motion_editor.py`, `viewport.py`, `qt_engine_viewport.py`, `core/action_data.py`, `panels/*.py`, `style/theme.py`, `test_*.py`

---

## 📋 요약

| # | 버그 | 파일 | 심각도 | 재현 난이도 |
|---|------|------|--------|--------------|
| 1 | 설정 파일 경로가 실행 시 작업 디렉터리(cwd) 기준으로 풀려 `config.json`이 여러 곳에 중복 생성됨 | `config_manager.py` | 🔴 높음 | 낮음 (이미 저장소에 증거 존재) |
| 2 | 상태 바 로그 메시지가 의도한 3초보다 먼저 사라짐 (타이머 경합) | `panels/status_bar.py` | 🟠 중간 | 낮음 |
| 3 | 씬 로드 후 엔티티 ID 카운터가 갱신되지 않아 신규 생성 엔티티가 기존 엔티티와 ID 충돌 가능 | `panels/scene_hierarchy.py` | 🟠 중간 | 중간 (씬 규모/반복 생성에 의존) |
| 4 | 타임라인 이벤트 마커의 클릭 판정 영역이 실제 렌더링 위치와 4px 어긋남 | `panels/event_timeline.py` | 🟡 낮음 | 낮음 |
| 5 (참고) | 사용되지 않는 `qt_engine_viewport.py`가 이미 고친 리사이즈 버그를 재현한 상태로 남아있음 | `qt_engine_viewport.py` | ℹ️ 정보 | — |

---

## 1. 설정 파일 경로가 cwd 기준 상대경로 — `config.json` 중복 생성

**위치**: [config_manager.py:16-19](../engine/editor/config_manager.py#L16-L19), 호출부 [main.py:106](../engine/editor/main.py#L106)

### 현상

```python
class ConfigManager:
    def __init__(self, config_path="config.json"):
        self.config_path = config_path   # cwd 기준 상대경로 그대로 사용
        ...
```

`main.py`는 인자 없이 `ConfigManager()`를 생성한다(`main.py:106`). `open(self.config_path, ...)` 은 **프로세스를 실행한 현재 작업 디렉터리(cwd)** 기준으로 풀리므로, 에디터를 어디서 실행하느냐에 따라 서로 다른 `config.json`을 읽고 쓴다.

### 이미 알려져 고쳐진 문제 패턴과 동일

`main.py`의 `_load_default_scene()`은 정확히 같은 종류의 버그를 이미 한 번 겪고 고친 흔적이 코드에 남아있다:

```python
# main.py:610-619
def _load_default_scene(self):
    """기본 씬 로드"""
    # cwd(현재 작업 디렉터리)가 아니라 이 파일 위치(_EDITOR_DIR) 기준 상대경로로
    # 계산해야 한다 — "engine/editor"에서 실행하든 다른 곳에서 실행하든
    # 항상 "engine/assets/scene.json"을 가리키게 함.
    scene_path = os.path.join(os.path.dirname(_EDITOR_DIR), "assets", "scene.json")
```

즉 `scene.json`은 `_EDITOR_DIR` 기준 절대경로로 고정했지만, **`ConfigManager`는 같은 처방이 적용되지 않은 채 남아있다.**

### 실제 증거 (저장소에서 확인됨)

`ConfigManager`가 만드는 것과 동일한 스키마(`window/paths/engine` 키 + 기본값)를 가진 `config.json`이 저장소 안에 **서로 다른 세 위치**에 존재한다:

| 파일 | 수정일 | 내용 |
|------|--------|------|
| `C:\QuarterFlying\config.json` | 2026-08-04 | ConfigManager 기본값 스키마와 동일 |
| `C:\QuarterFlying\engine\editor\config.json` | 2026-08-15 | ConfigManager 기본값 스키마와 동일 |
| `C:\QuarterFlying\engine\config.json` | 2026-07-24 | 다른 스키마(`version`, 720p) — 별도 C++ 엔진 설정으로 추정, 참고용 |

루트의 `config.json`과 `engine/editor/config.json`은 **내용이 완전히 동일한 기본값**이며 작성일만 다르다 — 즉 에디터를 서로 다른 cwd에서 두 번 실행했고, 그때마다 새 기본 설정 파일이 별도로 만들어져 저장됐다는 뜻이다.

### 영향

- 사용자가 창 크기/위치, `assetRoot`, `shaderRoot` 등을 바꿔 저장해도, 다음 실행 시 cwd가 달라지면(IDE에서 실행 vs 터미널에서 `cd engine/editor && python main.py` 등) **설정이 조용히 초기화된 것처럼 보인다.**
- 여러 개의 `config.json`이 저장소 루트/`engine/`/`engine/editor/`에 흩어져 쌓이며, `.gitignore`에 없으면 실수로 커밋될 위험도 있다.

### 권장 수정

`scene.json`과 동일한 방식으로 `_EDITOR_DIR` 기준 절대경로를 기본값으로 사용:

```python
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))

class ConfigManager:
    def __init__(self, config_path=None):
        self.config_path = config_path or os.path.join(_EDITOR_DIR, "config.json")
```

그리고 저장소에 이미 흩어진 3개의 `config.json` 중 어느 것이 "진짜" 사용자 설정인지 확인 후 나머지는 정리(삭제 또는 `.gitignore` 처리)할 것을 권장.

---

## 2. 상태 바 로그 메시지가 조기에 사라짐 — `QTimer.singleShot` 경합

**위치**: [panels/status_bar.py:166-176](../engine/editor/panels/status_bar.py#L166-L176)

### 현상

```python
def log(self, message: str):
    """짧은 로그 메시지를 상태 바에 표시 (3초 후 자동 소거)"""
    self.lbl_log.setText(f"▸ {message}")
    ...
    QTimer.singleShot(3000, self._clear_log)   # 호출할 때마다 새 타이머 예약

def _clear_log(self):
    self.lbl_log.setText("")
```

`log()`가 호출될 때마다 **독립적인** 1회성 타이머가 3초 뒤 `_clear_log()`를 예약한다. 이전 호출에서 예약된 타이머를 취소하거나 갱신하지 않기 때문에, 3초 이내에 `log()`가 다시 호출되면 **새 메시지가 화면에 표시된 직후, 이전 호출이 예약해 둔 타이머가 먼저 발화해 새 메시지를 지워버린다.**

### 재현 시나리오

`main.py`의 Play/Pause 흐름을 보면 실제로 3초 이내에 연달아 `log()`가 호출될 수 있다:

```python
# main.py:551  _on_play()
self.status.log("Play mode started")
...
# main.py:562-579  _on_pause()  (사용자가 Play 직후 3초 안에 Pause를 누르면)
self.status.log("Paused")
```

`Play` → (2.9초 후) `Pause`를 누르면: `Pause` 메시지가 표시된 지 0.1초 만에, `Play mode started`가 예약해둔 타이머가 발화하며 `Pause` 메시지를 지워버린다. 씬 로드/ECS 연결 로그, Undo/Redo 실패 메시지 등 `status.log()`를 호출하는 다른 모든 경로에서 동일하게 재현 가능하다.

### 영향

상태 바가 "짧게 깜빡였다가 곧바로 사라지는" 것처럼 보여, 사용자가 정작 확인해야 할 최신 메시지(에러 등)를 놓칠 수 있다.

### 권장 수정

매번 새 `singleShot`을 예약하는 대신, 인스턴스 소유의 `QTimer`를 하나 두고 `log()` 호출 시마다 `start(3000)`으로 재시작(진행 중이면 자동으로 리셋):

```python
def __init__(self, ...):
    ...
    self._log_clear_timer = QTimer(self)
    self._log_clear_timer.setSingleShot(True)
    self._log_clear_timer.timeout.connect(self._clear_log)

def log(self, message: str):
    self.lbl_log.setText(f"▸ {message}")
    ...
    self._log_clear_timer.start(3000)   # 이미 대기 중이면 자동으로 재시작됨
```

---

## 3. 씬 로드 후 엔티티 ID 카운터 미동기화 — ID 충돌 가능성

**위치**: [panels/scene_hierarchy.py:64-171](../engine/editor/panels/scene_hierarchy.py#L64-L171) (`_populate_dummy_scene`), [panels/scene_hierarchy.py:231-269](../engine/editor/panels/scene_hierarchy.py#L231-L269) (`load_from_scene_data`), [panels/scene_hierarchy.py:275-293](../engine/editor/panels/scene_hierarchy.py#L275-L293) (`create_entity`)

### 현상

엔진(`ge_python`)이 연결되지 않은 "더미 모드"에서 엔티티 ID를 발급하는 소스가 **두 군데로 나뉘어 있고 서로 동기화되지 않는다**:

1. `_populate_dummy_scene()`(생성자에서 항상 호출)이 `self._dummy_counter = 11`로 초기화.
2. `load_from_scene_data()`(실제 `scene.json`을 읽어 트리를 새로 구성할 때 호출, `main.py`가 시작 시 항상 호출)는 **자체 지역 카운터**를 사용한다: 오브젝트/인스턴스/그리드는 `entity_id = 1`부터, 라이트는 `entity_id = 100`부터 별도로 증가시킨다. 이 함수는 `self._dummy_counter`를 전혀 갱신하지 않는다.
3. 툴바의 "＋ Entity" 버튼(`create_entity()`)은 여전히 옛 `self._dummy_counter`(11에서 시작)를 사용해 새 엔티티 ID를 발급한다.

```python
def _load_objects_from_data(self, objects_data, parent_item):
    entity_id = 1                      # ← 지역 변수, self._dummy_counter와 무관
    for obj_data in objects_data:
        item = EntityItem(entity_id, ...)
        entity_id += 1
        ...

def create_entity(self):
    entity_id = self._dummy_counter    # ← 여전히 11에서 시작, 씬 로드로 갱신된 적 없음
    self._dummy_counter += 1
```

### 영향 및 재현 조건

- 엔진 미연결 상태(`self.registry is None`)에서 재현된다. `create_entity()`는 `self.registry`가 있을 때만 실제 엔진에서 ID를 받아오고, 없으면 이 로컬 카운터를 그대로 쓴다.
- 오브젝트(+인스턴스+그리드) 합계가 11개 이상인 씬을 로드한 뒤 "＋ Entity"를 누르면, 새로 만든 엔티티가 이미 트리에 있는 오브젝트와 **동일한 `entity_id`(예: 11)를 갖게 된다.** 현재 저장소의 `engine/assets/scene.json`은 오브젝트 파생 항목이 4개뿐이라 즉시 충돌하지는 않지만, 씬이 조금만 커지면(레벨 디자인상 매우 흔한 규모) 바로 재현된다.
- "＋ Entity"를 반복해서 누르면(카운터가 계속 증가하다가 100에 도달) 라이트용 ID 범위(100번대)와도 충돌한다.
- ID가 중복되면 `EntityItem.entity_id`로 엔티티를 식별하는 인스펙터 연결, 선택/삭제(`delete_selected_entity`) 로직이 **의도한 것과 다른 트리 항목**을 가리킬 수 있다(트리에서 같은 ID를 가진 항목이 둘 이상 존재하게 되므로).

### 권장 수정

씬 로드 시 사용한 최대 ID를 카운터에 반영하거나, 애초에 단일 카운터를 공유하도록 통합:

```python
def load_from_scene_data(self, scene_data):
    ...
    self._dummy_counter = max(self._dummy_counter, <이번에 발급한 최대 entity_id> + 1)
```

또는 `_load_objects_from_data`/`_load_lights_from_data`가 `self._dummy_counter`를 직접 증가시키도록 통합하는 편이 더 근본적인 해결책이다.

---

## 4. 타임라인 이벤트 마커 히트 테스트가 렌더링 위치와 4px 어긋남

**위치**: 렌더링 [panels/event_timeline.py:222-254](../engine/editor/panels/event_timeline.py#L222-L254) (`_draw_events`) vs 히트 테스트 [panels/event_timeline.py:403-414](../engine/editor/panels/event_timeline.py#L403-L414) (`_hit_test_event`)

### 현상

이벤트 마커(◆)를 그릴 때:

```python
half = MARKER_SIZE // 2                # 4  (MARKER_SIZE = 8)
y = y_base + row_idx * EVENT_ROW_H     # 다이아몬드의 "위쪽 꼭짓점" y좌표
path.moveTo(x, y)
path.lineTo(x + half, y + half)
path.lineTo(x, y + half * 2)
path.lineTo(x - half, y + half)
```

이 다이아몬드의 실제 중심은 `y + half` (= `y + 4`) 이다.

그런데 히트 테스트는:

```python
ey = y_base + row_idx * EVENT_ROW_H + MARKER_SIZE   # y + 8  ( half가 아니라 MARKER_SIZE를 더함 )
if abs(x - ex) < MARKER_SIZE and abs(y - ey) < MARKER_SIZE:
    return event
```

즉 판정 중심을 실제 렌더링 중심보다 **4px(= MARKER_SIZE의 절반) 아래**로 계산한다. 판정 허용 범위(`< MARKER_SIZE` = 8px)에 가려 항상 실패하지는 않지만, 시각적으로 마커 위쪽(특히 꼭짓점 부근)을 클릭하면 삭제 메뉴(우클릭)나 드래그 선택이 잘 걸리지 않고, 마커보다 살짝 아래를 클릭해야 더 잘 걸리는 등 클릭 정확도가 떨어진다. 이벤트 행 간격(`EVENT_ROW_H = 20px`)이 마커 크기보다 커서 치명적 오작동(다른 행의 마커를 잘못 집는 것)까지는 아니지만, 정밀한 편집 시 체감되는 UX 버그다.

### 권장 수정

```python
ey = y_base + row_idx * EVENT_ROW_H + (MARKER_SIZE // 2)   # half로 통일
```

---

## 5. (참고) 사용되지 않는 프로토타입 `qt_engine_viewport.py` — 회귀 위험

**위치**: [qt_engine_viewport.py:97-110](../engine/editor/qt_engine_viewport.py#L97-L110) vs [viewport.py:54-66](../engine/editor/viewport.py#L54-L66)

`qt_engine_viewport.py`는 저장소 어디에서도 import 되지 않는(자기 자신의 `__main__` 블록만 참조) 초기 프로토타입으로 보인다. 문제는 이 파일의 `resizeEvent`가 실제로는 아무 것도 하지 않는 스텁이라는 점이다:

```python
# qt_engine_viewport.py
def resizeEvent(self, event):
    if self.engine and self.initialized:
        try:
            platform = self.engine.GetPlatform()
            if platform:
                # platform.on_resize 메서드가 있다고 가정 (미구현)
                pass
        ...
```

반면 실제로 쓰이는 `viewport.py`의 `resizeEvent`는 이미 이 문제를 겪고 고친 뒤라 상세한 경고 주석까지 달려 있다:

```python
# viewport.py:54-66
def resizeEvent(self, event):
    super().resizeEvent(event)
    if self.initialized:
        # 엔진 쪽 glViewport/카메라 종횡비가 창 크기 변화를 전혀 모르고 있었다 - 특히
        # Motion Editor로 전환할 때처럼 Qt가 이 위젯을 다른(크기가 다른) 레이아웃으로
        # 재부모 이동시키는 경우 반드시 필요하다(그렇지 않으면 예전 크기 기준으로 그려져
        # 화면이 검게 보이거나 잘린다).
        e = ge_python.InputEvent()
        e.type = ge_python.InputEventType.WindowResize
        ...
        self.engine.PushInputEvent(e)
```

지금은 죽은 코드라 실행 경로에 영향은 없지만, 두 개의 "엔진 뷰포트 임베딩" 구현이 저장소에 공존하면서 하나는 고쳐진 버그를 갖고 하나는 안 갖고 있는 상태다. 나중에 누군가 `qt_engine_viewport.py`를 재사용하거나 참고하면 이미 한 번 고친 리사이즈 버그를 그대로 재도입할 위험이 있다. 삭제하거나 `viewport.py`와 동일한 리사이즈 처리로 통일할 것을 권장.

---

## 우선순위 제안

1. **#1 (config.json 경로)** — 사용자 설정이 실행할 때마다 조용히 리셋되는 것처럼 보이는 문제로, 이미 저장소에 중복 파일 증거가 남아 있다. 가장 먼저 수정 권장.
2. **#3 (엔티티 ID 충돌)** — 더미/오프라인 모드에서 데이터 정합성이 깨질 수 있어 우선순위가 높다.
3. **#2 (상태 바 로그)** — UX 문제이나 수정 비용이 매우 낮다(타이머 하나로 교체).
4. **#4 (마커 히트 테스트)** — 낮은 우선순위, 한 줄 수정.
5. **#5 (죽은 코드)** — 기능에는 영향 없음. 정리 시점에 함께 처리 권장.
