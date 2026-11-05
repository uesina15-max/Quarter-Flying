# 에디터 버그 수정 계획 (2026-10-22)

출처: [PYFLOW_COMPARISON_BUG_REPORT_2026-10-16.md](PYFLOW_COMPARISON_BUG_REPORT_2026-10-16.md)의 확인된 버그 #1~#4. 항목마다 CLAUDE.md 관례 4의 다섯 가지를 적는다. 구현이 끝나면 실제와 다른 곳을 고친다.

## F1. Inspector 필드 편집을 명령(Undo·저장 안 된 변경)으로 보낸다 (#1)

증상: Inspector에서 position·parent 외 필드(rotation, scale, 색, 메시 경로 등)를 바꾸면 Undo가 안 된다. 창 제목 `*`와 종료 시 저장 확인에도 잡히지 않아, 저장하지 않고 닫으면 경고 없이 사라진다.

1. **바뀌는 곳**
    - 새 명령 `editor/commands/SetComponentFieldCommand.{h,cpp}`: 이전 필드 값을 직렬화해 두고, `patchField`로 적용·되돌린다. 같은 엔티티·컴포넌트·필드면 병합 세션 안에서 합친다.
    - `editor/EditorAPI.{h,cpp}` `SetComponentField`, `bindings/EditorBindings.cpp` `set_component_field`.
    - `editor/panels/inspector.py` `_on_field_change`: position(`move_entity`)·parent(`set_parent`)를 뺀 필드를 `set_component_field`로 보낸다. 입력칸 포커스 동안의 병합 세션을 모든 입력칸으로 넓힌다.
    - `editor/project_actions.py:44` 주석의 잘못된 전제("Inspector 폴백만")를 고친다.
2. **영향받는 곳**
    - CommandManager 변경 카운터 → 창 제목 `*`, 종료·씬 전환 확인(`project_actions._unsaved_items`).
    - Undo/Redo 메뉴, 트랜잭션(`Dispatch`).
    - 리플렉션의 `serialize`/`patchField`(모든 등록 컴포넌트).
    - CMake(`EDITOR_SOURCES`, 테스트 소스 목록), Python 바인딩, Player 템플릿(엔진 DLL에 포함되므로 다시 빌드 필요).
3. **호환성**: 저장 형식 변화 없음(값을 쓰는 경로만 바뀐다).
4. **고장 시 갈리는 지점**
    - `set_component_field`가 예외를 내면 Inspector가 `[Inspector] … 실패` 로그를 남긴다(경계 값: 컴포넌트·필드 이름, JSON 값).
    - 명령 쪽 오류 메시지는 출처 `SetComponentFieldCommand`로 남는다.
    - 값이 바뀌었는데 `get_change_count`가 그대로면, 명령을 거치지 않는 경로가 남아 있다는 뜻이다.
5. **회귀 확인**
    - C++ `SetComponentFieldCommandTests`: 적용·Undo·Redo, 병합, 없는 컴포넌트·필드, 스키마 버전 불일치, EulerRotation·EntityRef 왕복.
    - 실제 에디터 재현: rotation 편집 → 저장 안 된 변경에 잡힘 + Undo로 되돌아감.

## F2. Play 중 Inspector를 읽기 전용으로 (#2)

증상: Play 중에도 Inspector 입력칸과 "＋ Component"가 켜져 있다. 고친 값은 Stop 때 안내 없이 사라진다.

1. **바뀌는 곳**: `editor/main.py` `_on_play`(`set_play_mode(True)`), `_refresh_play_controls`. 계층 패널의 생성·삭제 버튼과 컨텍스트 메뉴도 Play 중에 끈다(`scene_hierarchy.set_edit_shortcuts_enabled`를 넓힘).
2. **영향받는 곳**: Inspector 위젯 활성 상태, 계층 패널 버튼. Pause 중에도 Play 세션이므로 읽기 전용이다.
3. **호환성**: 해당 없음.
4. **고장 시 갈리는 지점**: `inspector._play_mode`와 `main.is_playing`이 다르면 연결 누락이다.
5. **회귀 확인**: 실제 에디터 재현(Play 중 입력칸 비활성, Stop 후 활성). 단위 테스트: Play/Stop에서 `set_play_mode` 호출.

## F3. 액션 Save As의 Sound clip 경로 (#3)

증상: 저장된 액션을 다른 폴더로 Save As 하면 Sound 이벤트 clip이 없는 파일을 가리킨다.

1. **바뀌는 곳**: `editor/core/action_data.py` `save_action`/`relativize_clip_paths`. 이미 상대 경로인 clip은 이전 `source_path` 기준으로 절대 경로를 구한 뒤, 새 폴더 기준으로 다시 상대화한다.
2. **영향받는 곳**: Motion Editor 미리듣기, Scene Play 액션 재생, Build Game 클립 수집. 모두 `source_path` 기준으로 푼다.
3. **호환성**: 파일 형식 변화 없음. 이미 깨진 채 저장된 파일은 자동으로 고치지 않는다.
4. **고장 시 갈리는 지점**: 저장 전후 `resolve_clip_path(clip, dirname(source_path))`가 같은 파일인지.
5. **회귀 확인**: `test_sound_event_editing.py`(또는 새 테스트)에 하위 폴더 Save As 후 같은 파일로 풀리는지(한글 경로).

## F4. Build Game이 BOM 스크립트·머티리얼을 읽도록 (#4)

증상: UTF-8 BOM이 붙은 스크립트는 Play에서 돌지만, Build Game이 "스크립트 문법 오류 … U+FEFF"로 멈춘다.

1. **바뀌는 곳**: `editor/gamebuild/game_builder.py` `_collect_script_imports`, `_collect_material_files`. 둘 다 `utf-8-sig`로 읽는다.
2. **영향받는 곳**: Build Game 의존성 수집. Player 템플릿은 무관하다(빌드 도구 쪽).
3. **호환성**: 해당 없음.
4. **고장 시 갈리는 지점**: `deps.errors`의 "스크립트 문법 오류" 메시지에 U+FEFF가 있는지.
5. **회귀 확인**: `gamebuild/test_game_builder.py`에 BOM 스크립트·머티리얼 테스트(한글 경로).

## 결과 (2026-10-22)

| 항목 | 상태 | 검증 단계(CLAUDE.md 관례 2) |
| --- | --- | --- |
| F1 Inspector 필드 편집 → 명령 | 완료 | **실제 실행**: 에디터 창, 엔진 연결, 코드로 입력. 단위 테스트(C++ 6개) |
| F2 Play 중 읽기 전용 | 완료 | **실제 실행** |
| F3 Save As clip 경로 | 완료 | 단위 테스트(한글 경로, 쓰기 실패 시 복원 확인) |
| F4 BOM 스크립트·머티리얼·씬 | 완료 | 단위 테스트(한글 경로) + 엔진이 BOM 씬을 읽는지 실제 엔진 모듈로 확인 |

**실제 에디터 확인** — 샘플 프로젝트에서 `EditorMainWindow`를 `HAS_ENGINE=True`로 띄웠다. 13개 항목 모두 통과.

- F1
    - Main Camera rotation Y 0 → 30: 변경 카운터 0 → 1, 저장 안 된 변경 `['씬 Demo.scene.json']`, 창 제목 `*Demo.scene.json`.
    - Edit > Undo가 켜지고, Undo로 0으로 돌아왔다.
    - 값 그대로인 문자열 칸의 `editingFinished`는 카운터를 올리지 않았다.
- F2
    - Play 중: 입력칸, ＋Component, 계층 ＋Entity·삭제가 모두 비활성이다. 일시정지 중에도 읽기 전용이다.
    - Stop 뒤 다시 활성이 된다.

**계획에 없던 수정 2건** (구현 중 발견)

- **Undo 메뉴 갱신**: `project_actions._poll_scene_dirty`가 변경 카운터가 바뀌면 Undo/Redo 메뉴도 갱신한다.
    - 증상: Inspector만으로 편집하면 Edit > Undo(Ctrl+Z)가 꺼진 채 남았다.
    - 수정 전 동작을 재현해 확인했다: position 편집 뒤에도 `_act_undo.isEnabled() == False`였다. 즉 F1 이전에도, 명령을 거치던 position 편집조차 메뉴로는 되돌릴 수 없었다.
- **값이 같으면 명령을 만들지 않음**: F1 이후 문자열 칸의 `editingFinished`가 포커스만 옮겨도 명령을 만들어 헛 `*`를 켜는 문제를 막았다(`inspector._on_field_change`).

**회귀 확인**

- C++ `SimpleEngineTests`: 649개 통과(새 `SetComponentFieldCommandTests` 6개 포함).
- 에디터 Python 테스트 14개 파일: 모두 통과.
    - `test_sound_event_editing` 8개(새 Save As 테스트 포함), `gamebuild/test_game_builder` 22개(새 BOM 테스트 포함).
    - `test_viewport_compatibility`는 `PYTHONPATH=engine/build/Release`로 5/5.
- Player 템플릿을 다시 빌드했고, `ctest -R player_template_smoke`가 통과했다.

**영향 범위의 실제** (계획과 다른 점만)

- F2에서 계층 패널은 버튼·컨텍스트 메뉴·드래그 재부모화를 막았다. 프리팹 브라우저의 Instantiate는 막지 않았다(Play 중 인스턴스화는 Stop 때 사라지므로, 필요하면 별도 항목으로).
- 기존 position(`move_entity`)·parent(`set_parent`) 경로는 그대로 두었다.
