# pyflow 사용/미사용 비교: 게임 에디터 버그 점검 보고서

2026-10-16

## 요약

게임 에디터에는 **재현되는 실제 버그가 4건** 있다. 그중 1건은 저장하지 않은 편집이 경고 없이 사라지는 데이터 손실이다. 사소한 문제 2건(표시 오류, 테스트 인코딩)도 있다.

- PyFlow 없이 점검해서 3건(#1·#2·#3)을 찾았고, PyFlow가 가리킨 복잡도 상위 함수에서 1건(#4)을 더 찾았다.
- PyFlow는 검토 후보 4,218개를 냈지만, 어느 후보도 이 버그들을 직접 가리키지는 않았다. 쓸모가 있었던 것은 "아직 안 읽은 복잡한 코드"로 안내하는 것과 호출 관계 조회(누가 이 함수를 부르는가)였다.
- 기존 테스트는 모두 통과한다(에디터 Python 테스트 14개, C++ 단위 테스트 643개). 위 버그는 모두 기존 테스트가 지키지 않는 경로에 있다.

대상: `C:\QuarterFlying` master, HEAD `35429a4` + 커밋 안 된 작업 트리(2026-10-16 기준).

## PyFlow와 실행 방법

`C:\QuarterFlying\pyflow - 복사본 (2)\pyflow - 복사본`에 있는 PyFlow는 Python·C·C++ 코드의 호출 관계 그래프, 검토 후보, AI용 브리핑(INDEX/STRUCTURE/CANDIDATES.md, analysis.json)을 만드는 정적 분석 도구다. 스스로 "후보는 결함 판정이 아니다"라고 밝힌다.

- **실행**: GUI 없이 PyFlow 테스트(`tests/test_briefing.py`)와 같은 순서로 호출했다 — `ProjectHierarchicalParser("engine/editor").parse()` → `export_briefing` → `export_review`. 패키지 경로는 `.runtime-paths.json`의 `runtime-three-languages`를 썼다.
- **결과**: 4.9초, 파일 79개(성공 78, 실패 1), 함수 785·클래스 130, 호출 1,668, 검토 후보 4,218개.
- **후보 구성**: 호출 대상 미해결 3,121 · 수신 객체 추정 654 · 간접/멤버 호출 166 · 순환/재귀 88 · include 해석 실패 68 · 동적 동작 58 · 높은 복잡도 27 · 기타 36.
- **PyFlow 자체 실패**: `EditorAPI.cpp`에서 C/C++ 해석 프로세스가 `3221225477`(0xC0000005, access violation)로 죽는다. 이 파일만 따로 분석해도 2회 모두 같은 실패가 났다. 엔진은 이 파일을 정상 컴파일하므로 PyFlow 쪽 결함이다. 에디터 C++ API의 핵심 파일이 분석에서 빠졌다.

브리핑 출력은 저장소 밖(세션 scratchpad의 `pyflow_editor\`)에 두었다.

## 비교 방법

같은 대상(`engine/editor`, Python 53개 파일 약 13,800줄 + 에디터 C++ 명령 약 1,500줄, 2026-10-16 작업 트리)을 두 방식으로 차례로 점검했다. PyFlow 결과가 판단을 끌고 가지 않도록 **미사용 점검을 먼저** 끝내고 기록한 뒤 PyFlow를 돌렸다.

1. **PyFlow 미사용** — 기존 테스트 14개 실행 → 큰 모듈(main.py, inspector.py, scene_hierarchy.py, project_actions.py, action_data.py, motion_editor.py) 직접 읽기 → 의심 지점마다 C++ 쪽(World.cpp, SceneIO.cpp, 바인딩) 확인 → 재현 스크립트.
2. **PyFlow 사용** — PyFlow로 같은 폴더를 분석해 브리핑(INDEX/CANDIDATES)을 만들고, 검토 후보를 따라 원문 확인 → 재현 스크립트.
3. 두 결과를 합쳐, 버그라고 적은 항목은 모두 재현 스크립트로 확인했다. 검증 단계는 CLAUDE.md 관례 2의 3단계(컴파일 / 단위 테스트 / 실제 실행)로 표시한다.

한계: 같은 사람(Claude)이 두 방식을 차례로 했으므로 두 번째 점검은 첫 번째에서 이미 본 코드의 영향을 받는다. 독립된 두 검토자의 비교가 아니다.

## 확인된 버그

> **2026-10-22 갱신**: #1~#4는 수정했다. 내용과 검증 단계는 [EDITOR_BUGFIX_PLAN_2026-10-22.md](EDITOR_BUGFIX_PLAN_2026-10-22.md)에 있다. #5·#6은 남아 있다.

| # | 심각도 | 증상 | 위치 | 찾은 방식 | 검증 단계 |
| --- | --- | --- | --- | --- | --- |
| 1 | 높음 | Inspector에서 position·parent 외 필드(rotation, scale, 색, 메시 경로 등)를 바꿔도 Undo가 안 되고, 창 제목 `*`·종료 시 저장 확인에 잡히지 않는다 → 저장 안 하고 닫으면 경고 없이 사라진다 | `engine/editor/panels/inspector.py` `_on_field_change()`, `engine/editor/project_actions.py:44` | 미사용 | 실제 실행(에디터 창, 엔진 연결, 코드로 입력) |
| 2 | 중간 | Play 중에도 Inspector 입력칸과 "＋ Component" 버튼이 켜져 있다. 고친 값은 Stop 때 안내 없이 사라진다 | `engine/editor/main.py` `_on_play()`, `engine/editor/panels/inspector.py` `set_play_mode()` | 미사용 (PyFlow 호출 그래프로도 확인) | 실제 실행 |
| 3 | 중간 | 저장된 액션을 다른 폴더로 Save As 하면 Sound 이벤트의 클립이 없는 파일을 가리킨다 → 미리듣기·Scene Play 무음, Build Game "액션이 참조하는 클립이 없습니다" | `engine/editor/core/action_data.py` `relativize_clip_paths()`·`save_action()` | 미사용 | 단위 재현(한글 경로) |
| 4 | 중간 | UTF-8 BOM이 붙은 게임 스크립트는 Play에서는 돌지만 Build Game이 "스크립트 문법 오류 … invalid non-printable character U+FEFF"로 빌드를 멈춘다 | `engine/editor/gamebuild/game_builder.py:417` `_collect_script_imports()` | PyFlow (복잡도 1위 함수) | 단위 재현(한글 경로) |
| 5 | 낮음 | 상태바가 선택한 엔티티 이름 대신 항상 `Selected: Entity_1 [1]`처럼 id로 만든 이름을 보여 준다 | `engine/editor/main.py:624` | 미사용 | 코드 확인만 |
| 6 | 낮음(테스트) | `test_demo_scene.py`·`test_viewport_compatibility.py`가 한국어 콘솔(cp949)에서 `✓`/`✗` 출력 중 `UnicodeEncodeError`로 멈춘다. `PYTHONIOENCODING=utf-8`이면 통과 | 두 테스트 파일 | 미사용 | 실제 실행 |

### #1 원인과 재현

"저장 안 된 변경"은 `CommandManager` 변경 카운터 하나로만 판단한다. 그런데 Inspector는 `TransformComponent.position`(→ `move_entity`)과 `HierarchyComponent.parent`(→ `set_parent`)만 명령으로 보내고, 나머지는 `registry.SetComponentFieldJson`으로 직접 쓴다. `project_actions.py:44` 주석은 명령을 안 거치는 편집이 "editor_api가 없을 때의 폴백"뿐이라고 가정하지만, 실제로는 엔진이 연결된 평상시에 대부분의 필드가 이 경로로 간다.

- 실제 에디터(샘플 프로젝트, Demo.scene.json)에서 Main Camera의 rotation Y를 0 → 30으로 바꾼 뒤: `_is_scene_dirty() = False`, 창 제목 `… — Demo.scene.json`(`*` 없음), 종료 시 물을 목록 `[]`.
- 같은 엔티티에서 position→rotation→scale을 차례로 바꾸면 change_count는 position에서만 오른다(2→3, 이후 3→3). Undo 1회는 position만 되돌리고 rotation 45·scale 3은 남는다.
- 바인딩에는 `rotate_entity`·`scale_entity`가 이미 있다. Inspector만 쓰지 않는다.

### #2 원인

`InspectorPanel.set_play_mode()`와 `DynamicComponentWidget.set_read_only()`는 구현돼 있지만, `set_play_mode(True)`를 부르는 곳이 없다. `_on_stop()`의 `set_play_mode(False)`만 있다. git 이력(`git log -S`)에도 `set_play_mode(True)`가 들어간 적이 없다. 즉 이 기능은 한 번도 켜진 적이 없다. 실제 에디터 Play 중 측정값: position 입력칸 `enabled=True`, ＋Component `enabled=True`, `_play_mode=False`.

### #3 원인

`save_action()`은 저장 시점에 **메모리의** clip 경로를 새 폴더 기준 상대 경로로 바꾼다. `relativize_clip_paths()`는 절대 경로만 바꾸고 이미 상대인 경로는 건너뛴다. 그래서 한 번 저장했거나 파일에서 불러온 액션을 다른 폴더로 저장하면 옛 폴더 기준 상대 경로가 그대로 쓰이고, `source_path`만 새 폴더로 바뀐다. 재현: `assets/actions/Slash.action.json`(clip `../sounds/hit.wav`) → `assets/actions/보스/`로 Save As → clip이 `assets/actions/sounds/hit.wav`(없음)로 풀린다.

### #4 원인

의존성 수집이 스크립트를 `read_text(encoding="utf-8")`로 읽어 `ast.parse`에 넘긴다. 이 경로는 BOM(U+FEFF)을 문자로 남기고, `ast.parse`는 그것을 문법 오류로 본다. 반면 Python import(에디터 Play·Player)는 BOM을 허용한다. 재현: 한글 폴더 프로젝트의 BOM 스크립트 — `import` 성공, `_collect_script_imports` errors 1건 → `build_game`은 `BuildError`로 중단한다. `_collect_material_files`도 같은 방식(`read_text` + `json.loads`)이라 BOM 머티리얼에서 같은 일이 날 수 있다(미재현).

### 의심했지만 버그가 아니었던 것

- Play 아닐 때 Stop → `World::Stop()`이 Edit 상태를 확인해 아무것도 하지 않는다.
- 씬 전환 후 이전 씬의 Undo → `LoadScene`·`NewScene`이 Undo 기록을 비운다.
- 씬 불러오기 실패 시 월드가 반쯤 지워짐 → 빈 레지스트리에 먼저 불러 본 뒤 교체한다.
- position 입력 중 Ctrl+Z로 Inspector가 다시 그려질 때 병합 세션이 남음 → 포커스가 떠나면 정상 종료되고, 다른 엔티티 이동은 따로 Undo된다(실제 에디터에서 확인).
- 에디터가 부르는 바인딩 메서드 24개는 실제 모듈에 모두 있다. 정의 없는 private 메서드 호출은 0건이다.

## PyFlow 사용/미사용 비교

두 방식은 서로 다른 버그를 찾았다. 버그 자체는 두 방식 모두 원문을 읽고 실행해 본 뒤에야 확정됐다.

| 항목 | PyFlow 미사용 | PyFlow 사용 |
| --- | --- | --- |
| 출발점 | 기존 테스트 14개, 큰 파일 6개를 직접 읽음 | 4.9초 분석, 후보 4,218개 + 복잡도 상위 15개 함수 |
| 찾은 버그 | #1, #2, #3, #5, #6 | #4 |
| 찾은 버그의 종류 | "값이 어느 경로로 가는가", "이 가정이 맞는가" 같은 의미 버그 | 안 읽은 모듈(Build Game)의 입력 경계 버그 |
| 놓친 것 | `game_builder.py`를 읽지 않아 #4를 놓침 | #1·#2·#3은 어느 후보에도 없음(호출은 존재하고 구문도 멀쩡하므로) |
| 실제로 도움이 된 기능 | grep, git 이력, 재현 스크립트 | 복잡도 순위(무엇부터 읽을지), 호출자 조회(`set_play_mode ← _on_stop` 하나뿐) |
| 잡음 | 없음(대신 읽는 범위가 좁음) | 미해결 호출 3,121개 대부분이 Qt 상속 메서드·표준 라이브러리·pybind 메서드. 순환 88개 대부분이 시그널 연결. "호출자 없음" 110개 중 실제 고아는 4개(`apply_ai_sections`, `editor_engine_dll`, `get_selected_entity_id`, `qt_engine_viewport.py`) |
| 오탐 유형 | — | `project/__init__.py` 재수출(`current_project` 등을 호출자 0으로 표시), 시그널에 연결된 슬롯, 프로퍼티 접근 |
| 도구 자체 문제 | — | `EditorAPI.cpp` 분석 실패(매번 재현), C++ 쪽은 상위 폴더 include를 못 찾아 include 해석 실패 68개 |

평가: 이 코드베이스의 버그는 대부분 "컴파일도 되고 호출도 연결돼 있지만 값이 엉뚱한 경로로 간다"는 종류다(CLAUDE.md 관례 3의 사례들과 같은 계열). 호출 그래프 기반 후보로는 이 종류가 잡히지 않는다. PyFlow는 버그를 찾아 주는 도구로는 약하고, 어디를 읽을지 정하고 호출 관계를 빠르게 확인하는 지도로는 쓸모가 있었다. 표본이 한 번의 점검이므로 일반화하기에는 작다.

## 권장 조치

수정은 아직 하지 않았다(이번 작업은 점검만). 우선순위 순서:

- [ ] **#1** — Inspector의 rotation·scale은 이미 있는 `rotate_entity`·`scale_entity`로 보낸다. 나머지 필드는 "필드 설정" 명령(EditorAPI, 이전 값 저장)을 추가해 같은 카운터를 올린다. `project_actions.py:44`의 잘못된 전제 주석도 고친다. 회귀 테스트: Inspector로 rotation을 바꾸면 `_unsaved_items()`에 씬이 나오고 Undo로 되돌아감.
- [ ] **#2** — `_on_play()`에서 `self.inspector.set_play_mode(True)`를 부른다. 계층 패널의 ＋ Entity·✕·컨텍스트 메뉴도 Play 중 동작을 정한다(현재는 단축키만 꺼짐).
- [ ] **#3** — 메모리의 clip을 절대 경로로 들고 있다가 파일에 쓸 때만 상대화하거나, 상대 경로도 이전 `source_path` 기준으로 푼 뒤 새 폴더 기준으로 다시 계산한다. 회귀 테스트: 하위 폴더로 Save As 후 clip이 같은 파일로 풀림.
- [ ] **#4** — 스크립트·머티리얼을 `utf-8-sig`로 읽는다(또는 스크립트는 `ast.parse(bytes)`). 회귀 테스트를 `test_game_builder.py`에 추가한다.
- [ ] **#5·#6** — 상태바에 `registry.GetEntityName` 결과를 넘긴다. 두 테스트는 다른 테스트처럼 ASCII `PASS/FAIL`로 출력하거나 `sys.stdout.reconfigure(encoding="utf-8")`를 쓴다.
- [ ] **PyFlow** — `EditorAPI.cpp` 해석 실패(0xC0000005)를 PyFlow 쪽에서 고친다. C++ include는 `engine/` 루트를 검색 경로로 주거나 `compile_commands.json`을 주면 잡음이 줄 것이다.

참고: `pyflow - 복사본 (2)` 폴더와 zip은 저장소 루트에 untracked로 있다. 커밋에 섞이지 않게 저장소 밖으로 옮기거나 `.gitignore`에 넣는 것이 좋다.
