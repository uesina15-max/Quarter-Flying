# 제작 기능 검수 및 버그 수정 (2026-11-05)

기준: [MASTER_PLAN.md](MASTER_PLAN.md) 맨 위의 2026-11-04 제작 기능 표(캐릭터 충돌, 이동 애니메이션, 프리팹 Apply/Variant, 파티클 스타일, 제작 도구)와 [인게임 UI](INGAME_UI_CUSTOMIZATION_PLAN.md). 검수 전에는 C++ 테스트와 Python 테스트가 모두 통과하는 상태였다. 아래 버그는 전부 실제 엔진 모듈과 네이티브 창으로 재현한 뒤 고쳤다.

## 수정

| # | 증상 | 원인 | 수정 위치 |
|---|---|---|---|
| 1 | 캐릭터가 가끔 바닥을 뚫고 떨어지거나 타일 경계에서 영구히 멈춘다 | 충돌 허용 오차가 거리가 아니라 시간 비율이라, 이동량이 작은 고프레임에서 float 한 칸보다 좁아졌다 | `ecs/StaticCollision.cpp` (`kContactSkin`, 면에 위치 맞춤) |
| 2 | 수정하지 않은 인스턴스로 베리언트를 만들어도 샘플 프리팹 4종 모두 override가 기록되고(Fire는 8개 필드), 그 필드는 원본 변경을 상속하지 않는다 | 인스턴스 스냅샷을 파일 원문과 비교했다. float32 왕복값과 파일에 없는 기본값이 전부 "다름"이 됐다 | `core/prefab_variants.py`, 바인딩 `SnapshotPrefabFile` |
| 3 | EntityRef가 있는 인스턴스에서 Apply를 눌러도 대화상자 없이 아무 일도 안 일어난다 | `quarterflying.EngineError`는 `RuntimeError`의 자식이 아니라서 `except`에 걸리지 않았다 | `panels/inspector.py` `_write_prefab` |
| 4 | `on_ui_click`에서 `quarterflying.input.set_cursor_locked` 등을 부르면 "no engine frame is running"으로 실패한다 | UI 콜백이 입력 문맥(`ScopedCurrent`) 밖에서 불렸다. 커서 잠금 요청 처리도 콜백보다 앞에 있었다 | `core/Engine.cpp` `TickFrame` |
| 5 | 일시정지하면 게임 UI가 통째로 사라진다 | 오버레이를 Play 상태에서만 만들었다. 이제 Pause에서는 그리기만 하고 콜백을 부르지 않는다 | `core/Engine.cpp`, `renderer/GameOverlay` |
| 6 | 문서 순서(이펙트 → 스타일)로 추가하면 시트·트레일이 Edit·Play에 나오지 않고 Stop 뒤에야 나타난다. Inspector에서 이펙트 값을 바꿔도 반영되지 않는다 | 설정을 이펙트 생성 시 한 번만 복사했고, 이펙트는 Edit 상태에서 바로 만들어진다 | `ecs/ParticleSystem.cpp` `SyncEffectsWithRegistry` |
| 7 | Burst 이펙트가 에디터 Play에서 터지지 않는다(빌드한 게임에서는 터진다) | Edit 미리보기에서 이미 터진 이펙트를 Play에서 이어 썼다 | `ecs/ParticleSystem.h` `OnStart` |
| 8 | `gpuMotion` 파티클이 시야 밖에서 스폰해 시야로 날아오면 통째로 사라진다 | 컬링이 스폰 위치만 봤다. 실제 위치는 정점 셰이더가 계산한다 | `renderer/InstancedBatchManager.cpp` `UpdateVisibility` |
| 9 | 팔로 쓰는 자식 큐브(크기 0.2, 0.6, 0.2)가 Play를 누르면 (1, 1, 1)로 커진다 | 키프레임에 크기가 없는데 본의 바인드 크기를 덮어썼다 | `animation/LocomotionSystem.cpp` |
| 10 | 디스플레이 배율 150%에서 3D 화면과 UI가 위젯의 왼쪽 아래 2/3에만 그려지고 클릭 위치도 어긋난다 | 엔진에 Qt 논리 좌표를 넘겼다. 배율 100%에서는 값이 같아 드러나지 않았다 | `editor/viewport.py` `_to_pixels` |
| 11 | Material Editor에서 새 머티리얼을 기존 파일 이름으로 저장하면 덮어쓰기를 승인했는데도 "[WinError 183]"으로 실패한다 | 새 파일 전용 경로(`os.link`)로 갔다 | `panels/production_tools.py` |
| 12 | Asset References 검색이 프로젝트 폴더 이름의 일부로 찾으면 모든 줄에 걸린다 | 표에 없는 절대 경로까지 검색했다 | `panels/production_tools.py` |
| 13 | 빌드한 게임이 1초에 120줄씩 로그를 찍고, stdout을 읽지 않는 프로세스가 띄우면 검은 화면으로 멈춘다 | `RenderGraph::Reset`이 프레임마다 Info 로그를 남겼다 | `renderer/RenderGraph.cpp` |

## 동작이 바뀐 곳

- **파티클**: `ParticleEffectComponent`나 `ParticleStyleComponent` 값이 바뀌면 이펙트를 처음부터 다시 시작한다. 진행 중인 파티클의 값을 고치지 않는 이유는 `EffectInstance::settings` 주석에 있다. 게임 스크립트가 값을 바꿔도 같다.
- **파티클**: Play를 시작하면 이펙트가 처음부터 다시 시작한다. 빌드한 게임과 같다.
- **`gpuMotion` 파티클**은 프러스텀 컬링을 하지 않는다. 안 보이는 파티클도 정점 처리 비용을 낸다.

## 범위·호환성·진단

- 저장 형식 변경 없음. 바인딩은 `SnapshotPrefabFile`을 추가했다. 이미 만든 베리언트 파일은 그대로 읽히지만, 예전에 기록된 불필요한 override는 파일에 남는다. 원본 변경을 상속하게 하려면 베리언트를 다시 만들거나 해당 인스턴스로 Apply한다.
- 엔진 DLL이 바뀌었으므로 Build Game 전에 `player_template`을 다시 빌드해야 한다(이 기록 시점에 다시 빌드함).
- 원인 구분:
  - 충돌: `SweepCharacter`의 입력 `feet`/`delta`와 결과 `feet`/`grounded`.
  - 베리언트: `SnapshotPrefabFile(원본)`과 `SnapshotPrefab(인스턴스)`의 필드 차이.
  - UI: `GameOverlay::Frame`의 `interactive`, 콜백 예외는 로그 `Game UI on_ui_click:`.
  - HiDPI: 뷰포트의 `devicePixelRatioF()`와 `WindowResize` 이벤트 값.

## 검증 (CLAUDE.md 관례 2)

- **단위 테스트 통과**: C++ `SimpleEngineTests` 전체(새 충돌 4개, 파티클 3개, 로코모션 크기 검사 추가). Python `engine/editor/test_production_features.py` 8개(실제 엔진 + 네이티브 창). 충돌 판정을 수정 전 코드로 임시로 되돌려 새 충돌 테스트 3개가 실패하는 것을 확인했다(나머지 1개는 과잉 수정을 막는 테스트).
- **실제 실행(에디터, 화면 캡처)**: 실제 `EditorMainWindow`를 한글 경로의 임시 프로젝트로 띄우고 Qt 이벤트로 조작했다. 새 컴포넌트 추가, Inspector 표시, 씬 저장·다시 열기, Frame Profiler, Asset References, Play, 착지, 키보드 이동과 벽 충돌, UI 호버·클릭, 콜백 안의 input API, Pause 중 UI 표시와 클릭 무시, Stop 복원까지 27개 검사가 통과했다. 머티리얼 바닥, 시트·트레일 파티클, UI 텍스트가 그려진 것을 캡처로 확인했다. 엔진 ERROR/WARN 로그는 없었다.
- **실제 실행(빌드한 게임, 화면 캡처)**: Build Game으로 빌드한 exe를 띄우고 Win32 메시지로 입력했다. 호버 색 변화, 클릭 콜백(`created, enter, click, handled`), 라벨 변경, 벽 충돌, 머티리얼·파티클이 화면에서 확인됐다. 종료 코드 0.
- **확인하지 못한 것**:
  - HiDPI는 `QT_SCALE_FACTOR=1.5`로 흉내 냈다. 실제 고배율 모니터와, 배율이 다른 모니터로 창을 옮기는 경로(`DevicePixelRatioChange`)는 실행하지 않았다.
  - Inspector의 Apply·Variant는 버튼 클릭이 아니라 `_write_prefab`을 직접 불러 검사했다(확인 대화상자는 대체).
  - 깨끗한 PC에서의 빌드 실행, 긴 시간 플레이.
  - 트레일은 샘플 간격이 1/60초라 화면에서 짧게 보인다. 눈에 띄는 잔상을 기대하면 값 범위(0~4)를 다시 정해야 한다.

## 고치지 않은 것

- Asset References의 참조 교체는 확장자가 같은 파일만 허용한다. `.png`를 `.jpg`로 바꾸는 것도 거절된다. 안전장치로 넣은 동작이라 버그로 보지 않았다.
