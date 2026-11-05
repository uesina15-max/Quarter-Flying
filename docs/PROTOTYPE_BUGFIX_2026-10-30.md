# 프로토타입 편집 상태 버그 수정 (2026-10-30)

기준 문서: [PROTOTYPE_ITERATION_2026-10-28.md](PROTOTYPE_ITERATION_2026-10-28.md).
문서의 기존 완료 기록과 별개로, 현재 코드의 실제 엔진 모듈 + Qt 위젯에서 아래 문제를 재현했다.

## 수정

1. **미적용 속도가 있는 상태에서 키보드 연결 해제 → Play 차단**
   - 연결 해제로 속도 입력·적용 버튼은 꺼지지만 `_speed_dirty`가 남아 Inspector의 공통 Play 검사에 걸렸다.
   - 연결 해제 후 저장된 속도를 다시 표시하고 미적용 상태·버튼 문구를 초기화한다. 사용자에게 미적용 속도 취소를 알린다.
   - 연결된 상태의 갱신과 읽기 전용 전환에서는 미적용 입력을 유지한다. Undo는 연결 상태와 기존 저장 속도를 복원한다.
2. **카메라 값을 원래대로 돌려도 Play 차단·다른 좌표 정밀도 손실**
   - `_edited_fields`가 현재 값 차이가 아닌 변경 이력을 쌓았다. 원래 값으로 복귀해도 Play를 막았고, 다른 필드를 적용하면 복귀한 필드까지 화면의 소수점 3자리 값으로 덮어썼다.
   - 로드한 화면 값과 현재 값을 비교하여 실제 바뀐 필드만 적용한다. 높이만 바꾸면 기존 수평 오프셋을, 거리만 바꾸면 기존 높이를 그대로 유지한다.

## 범위·호환성·진단

- 수정: `engine/editor/panels/keyboard_movement.py`, `engine/editor/panels/follow_camera.py`.
- 영향: Inspector 미적용 검사, 툴바/패널 Play 허용 상태, 기존 EditorAPI transaction의 입력 값.
- 엔진 C++·바인딩·저장 형식 변경 없음. Player 템플릿에 포함되지 않는 에디터 UI 변경이므로 템플릿 재빌드는 불필요하다.
- 원인 구분: `_speed_dirty`, `_offset_edited`, `_edited_fields`와 `InspectorPanel.pending_authoring_message()`를 확인한다. 저장값은 기존 `GetComponentJson`에서 확인한다.

## 검증

- `engine/editor/test_prototype_authoring.py`: 실제 Release 엔진 모듈과 Qt Inspector/버튼을 사용하는 회귀 검사 10개.
- 수정 전 실패를 확인한 뒤 수정본에서 10개 모두 통과. 연결 해제·재연결 상태, Undo/Redo, 읽기 전용, float32 표시값, 카메라 좌표 정밀도, 불필요한 Undo 생성 방지를 검사했다.
- 엔진은 표시하지 않은 네이티브 Qt 위젯에 연결했다. 에디터 전체 창의 수동 조작·렌더링 화면 검증은 수행하지 않았다. C++ 코드는 수정·재컴파일하지 않았다.

실행: `PYTHONPATH`에 `engine/build/Release`를 추가한 Windows Python + PySide6 환경에서 `python -B engine/editor/test_prototype_authoring.py`.
