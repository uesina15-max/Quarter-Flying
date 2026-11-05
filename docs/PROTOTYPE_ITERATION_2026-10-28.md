# 프로토타입 반복 실험 1~3단계 구현 범위

사용자 선택: Play/Stop Undo 보존 → 키보드 조작 대상 연결 → 객체별 속도 조정·저장. 각 단계의 검증을 끝내고 다음 단계로 진행한다.

1단계는 명령의 UUID 재조회, 편집 중인 레지스트리에 한정한 Play 이력 잠금, UI 상태 갱신을 변경한다. 새 씬·씬 열기의 기록 초기화 정책은 유지한다. C++ 118개와 실제 엔진+Qt 18개 검사 통과.

2단계는 `KeyboardMovementComponent`와 `KeyboardMovementSystem`을 추가하고 Engine의 월드 생성에서 등록한다. Inspector에는 조작 대상 연결/해제와 Play 버튼을 추가한다. 기존 transaction을 사용하여 이전 대상 해제와 새 대상 연결을 한 번에 Undo한다. 이동 계산은 키보드 입력 → 카메라 기준 방향 → Transform 갱신 한 곳에 모은다. 부모·카메라·ScriptComponent가 있는 객체는 UI와 실행계층 모두 제외한다. 고정 WASD/방향키, 수평 이동, 충돌·점프 없음으로 범위를 제한한다.

3단계는 같은 컴포넌트에 speed(float, 기본 4)를 추가하고 전용 속도 UI를 연결한다. 기존 씬 저장·Undo·Play 스냅샷을 사용하므로 별도 설정 파일은 없다. 누락된 필드는 기본값으로 읽는다. 새 기능을 저장한 씬은 업데이트한 엔진/Player에서 사용한다. 일반 ScriptComponent에 이동 전용 필드를 섞거나 사용자 스크립트를 자동 덮어쓰지 않는다.

영향 범위: ECS 등록/리플렉션, Engine 월드 시스템 등록, Python Inspector와 작은 명령 헬퍼. 네트워크·프로젝트별 스크립트 복사·중간 JSON 파일·입력 액션 맵 수정 없음. 바인딩은 기존 리플렉션 API를 재사용한다. 기존 씬 형식 version 변경 없음. 빌드 게임은 새 컴포넌트를 실행할 Player가 필요하므로 마지막 단계에서 Player 템플릿을 갱신한다.

검수 후 수정(2026-10-28, 실제 엔진 모듈 + Qt 위젯으로 재현 후 수정, 에디터 화면 확인은 안 함):
- **"＋ Component"로 KeyboardMovementComponent를 붙일 수 있었다**: 기존 대상 해제·자격 검사를 건너뛰어 두 객체가 같이 움직였고, 카메라에 붙이면 Inspector 어디에도 보이지 않았다. 메뉴에서 빼고(`inspector._NOT_USER_ADDABLE`), 전용 위젯이 없는 엔티티에서는 일반 위젯으로 보인다.
- **속도 0.1이 계속 "미적용 변경"**: float32 왕복값(0.10000000149…)과 비교해 Play 버튼이 꺼졌다. 화면 값과 비교한다(`panels/keyboard_movement.py`).
- **병합 세션이 이전 세션의 명령에 합쳐짐**: 다른 입력칸 편집, Play 전 편집과 Stop 후 편집이 Undo 한 번에 같이 되돌아갔다. 이번 세션에서 쌓은 명령에만 합친다(`CommandManager::Execute`, C++ 테스트 `MergeSession_DoesNotMergeIntoEarlierSessionOrRedo`).

진단 경계: 연결 실패는 Inspector 상태 문구와 transaction 오류, 이동 오류는 KeyboardMovementSystem의 입력/좌표 변환, 저장 오류는 SceneIO의 컴포넌트 데이터로 구분한다. 고장 검증은 이동 프레임률·대각선·포커스 해제·카메라 방향·제외 객체·Edit/Pause/Stop·대상 전환 Undo·속도 저장 재열기로 제한한다. 전체 제품의 수동 QA나 경쟁 도구 비교 완료를 주장하지 않는다.
