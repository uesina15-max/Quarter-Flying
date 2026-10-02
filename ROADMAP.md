# Quarter Flying — 프로젝트 로드맵

**작성일**: 2026-08-10
**기준**: `C:\QuarterFlying` (정본 경로, git 이력 `452a688`~`90e1a74`). `D:\Quarter Flying`은 이 시점 이전의 구버전 사본이며 이후 갱신되지 않는다 — 상세는 [CLAUDE.md](CLAUDE.md) 참고.

이 문서는 "지금 뭐가 되고, 뭐가 안 되고, 다음에 뭘 해야 하는가"를 한 화면에 담기 위한 것이다. 개별 작업의 상세 계획은 각 절에서 링크한 문서를 따른다 — 이 문서는 그 문서들을 대체하지 않고 순서를 정한다.

장기 제품 전략/우선순위 관점의 상위 문서는 [docs/MASTER_PLAN.md](docs/MASTER_PLAN.md) — 이 로드맵은 그 문서의 P0/P1/P1.5/P2/P3 축을 세션 단위 실행 기록으로 뒷받침한다.

---

## 1. 현재 상태 (검증됨, 2026-08-10)

오늘 처음으로 저장소 전체를 빌드하고 에디터를 실제로 실행해봤다. 확인된 사실:

| 항목 | 상태 | 근거 |
|---|---|---|
| git 버전 관리 | ✅ 있음 (`C:\QuarterFlying`에서 시작, 이전엔 전혀 없었음) | 커밋 `452a688`~ |
| `ge_engine.dll` 빌드 | ✅ 성공 (이 저장소 역사상 처음으로 보임) | 커밋 `1b1682a`, `86bd2f8` |
| `ge_python.pyd` 빌드 | ✅ 성공, `import ge_python` 동작 | 커밋 `36ec29c`, `e80616a` |
| 에디터 실제 엔진 연동 실행 | ✅ HWND 임베딩 성공, TickFrame 반복(90~200 FPS), scene.json 로드 확인 | 이 세션에서 직접 실행·스크린샷 확인 (기록은 CLAUDE.md) |
| 창 리사이즈 | ✅ 크래시 없음 | 위와 동일 |
| Play Mode 뷰포트 | ✅ 2026-09-25: 초기화 성공, 크래시 없음(Play/Stop/Motion Editor 전환까지 실행 확인). ⚠️ Play 뷰포트 화면이 의도대로 그려지는지는 아직 미확인 | CLAUDE.md "알려진 버그" 해결 기록 |
| Scene Editor 3D 뷰포트 렌더링 | ⚠️ 미검증 — 크래시는 없지만 화면이 검게 나옴, 실제로 지오메트리가 그려지는지 확인 안 됨 | 위와 동일 |
| `InstanceData` GPU 레이아웃(instanced rendering) | ⚠️ 미검증 — 컴파일은 통과하나 실행 검증 없음 | CLAUDE.md |

**핵심 요약**: "빌드가 안 됐다"는 원래 문제의 진짜 원인은 툴체인 미설정이 아니라 **엔진 코어(특히 `renderer/`)가 상당 기간 컴파일된 적이 없었고, 컴파일이 통과한 지금도 렌더링 자체는 실행 검증이 안 된 상태**라는 것이다. "컴파일 통과"와 "실제로 그려짐"은 이 프로젝트에서 별개의 검증 단계로 다뤄야 한다.

---

## 2. 우선순위별 다음 작업

### P0 — 지금 당장 (블로커 성격)

1. **Play Mode 윈도우 클래스 충돌 수정** — `Win32Platform`이 `RegisterClass`를 프로세스당 한 번만 허용하는데 두 번째 `EngineViewport`가 또 등록을 시도해서 실패한다. `RegisterClassEx` 실패 시 `GetLastError() == ERROR_CLASS_ALREADY_EXISTS`면 기존 클래스를 재사용하도록 수정하거나, 뷰포트별로 고유한 클래스 이름을 쓰도록 변경. 코드 위치: `engine/platform/Win32Platform.cpp`.
2. ✅ **Scene Editor 3D 뷰포트가 실제로 그리는지 확인 — 완료(2026-08-18)**: `engine/ecs/RenderSystem.cpp`(신규) — `TransformComponent`+`RenderableComponent`를 읽어 `InstancedBatchManager`에 채우는 첫 ECS System. `RenderableComponent.meshHandle`이 미등록이면 절차적 유닛 큐브로 대체(실제 메시 에셋 파이프라인은 아직 없음). `Engine::CreateWorld()`가 새 World마다 자동 등록. `engine/renderer/SceneMeshRenderer.h/.cpp`(신규) — 그 배치를 실제로 그리는 렌더러(DebugGridRenderer와 같은 패턴, 임베드 GLSL). 상세 경위·발견한 버그 4건은 §6 참고. `SimpleEngineTests`에 `RenderSystemTests.cpp` 9개 추가, 전체 스위트 373개 — 368 통과/5 실패(기존 무관 실패, 새 회귀 없음). 실제 에디터 실행 스크린샷으로 큐브가 그리드 위 원점에 올바른 음영으로 렌더링됨을 확인.

### P1 — Python 바인딩 정리 (Phase 2~6)

Phase 1(빌드 확인)은 완료됨. 나머지는 [docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md](docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md)에 상세 계획이 있다 — 여기서는 순서만 짚는다.

- **Phase 2 — 런타임 Protocol 정의**: [docs/ge_python_api_dump_20260810.txt](docs/ge_python_api_dump_20260810.txt)를 입력으로 `typing.Protocol` 확정. `EngineConfig`는 3개 필드(`windowWidth`/`windowHeight`/`windowTitle`)만 노출된다는 점, `CreateTexture` 등 렌더러 저수준 API는 바인딩에 없다는 점을 반영.
- ✅ **Phase 3 — 공통 래퍼 계층 완료(2026-08-15, 예정보다 앞당겨 진행)**: 신규 [engine/editor/engine_binding.py](engine/editor/engine_binding.py)에 `import ge_python`을 유일하게 모아뒀다. `try: import ge_python as binding / except ImportError: binding = None`으로 `HAS_ENGINE`/`binding`을 노출하고, 다른 모든 코드는 `from engine_binding import binding as ge_python, HAS_ENGINE`으로 받아쓴다(별칭이라 호출부의 `ge_python.Xxx` 표현은 그대로 유지됨). 원래 계획한 6개보다 늘어난 **9개 파일**(`main.py`, `viewport.py`, `qt_engine_viewport.py`, `demo_scene_integration.py`, `test_viewport_compatibility.py`, `panels/scene_hierarchy.py`, `panels/inspector.py`, `panels/animation_preview.py`, `panels/motion_mixer.py` — Motion Mixer Phase 4A/5에서 새로 늘어난 2개 포함)를 전부 마이그레이션. `test_viewport_compatibility.py`의 `test_ge_python_binding()`처럼 "import 실패"를 별도로 검사하던 테스트는 `HAS_ENGINE` 명시적 체크로 바꿔서 원래 의미를 그대로 보존(래퍼가 `ImportError`를 삼켜버리므로 그대로 두면 거짓 통과가 날 뻔했음). 엔진 있음/없음 두 모드 모두 실제 에디터 실행으로 재확인, 회귀 없음. 이제 Phase 6(이름 개명) 시점에 고쳐야 할 import 지점은 이 파일 한 곳뿐이다.
- **Phase 4 — 모드 경계 + IDE 경고**: pyright/mypy 기준선 측정 후 수치로 검증.
- **Phase 5 — 테스트**: 더미/실제 인터페이스 일치를 회귀로 고정.
- ✅ **Phase 6 완료(2026-08-20) — 이름 정합성 정리 (`ge_python`/`ge_engine` → `Quarter Flying` 계열)**: `engine/bindings/PythonModule.cpp`의 `PYBIND11_MODULE(ge_python, m)` → `PYBIND11_MODULE(quarterflying, m)`, `engine/CMakeLists.txt`의 CMake 타겟명 `ge_engine`→`quarterflying_engine`/`ge_python`→`quarterflying`(및 그에 딸린 모든 `target_include_directories`/`target_link_libraries`/`target_compile_definitions`/`install(TARGETS ...)`). 실제로 고쳐야 했던 Python import 지점은 Phase 3에서 미리 만들어둔 [engine/editor/engine_binding.py](engine/editor/engine_binding.py) **한 곳뿐**이었다(`import ge_python as binding` → `import quarterflying as binding`) — 이게 바로 이 래퍼를 먼저 만든 이유였고, 실제로 그대로 증명됐다. 나머지 9개 파일의 `from engine_binding import binding as ge_python`은 `ge_python`이 그냥 로컬 별칭 이름일 뿐이라 손대지 않아도 동작에 지장이 없어 그대로 뒀다(사용자 화면에 노출되는 문자열이 아니므로). 다만 사용자에게 실제로 보이는 문자열(상태 바 메시지 "더미 모드로 실행 중 (ge_python 없음)" 등, `main.py`/`qt_engine_viewport.py`/`scene_hierarchy.py`의 주석·독스트링)은 옛 이름을 일반화된 표현("엔진 모듈")으로 고쳤다. 빌드 검증: `quarterflying_engine.dll`/`quarterflying.cp314-win_amd64.pyd` 정상 생성, `SimpleEngineTests` 전체 스위트 410개 — 405 통과/5 실패(기존 무관 실패와 정확히 동일, 새 회귀 없음), 빌드 경고는 전부 기존에 있던 것과 동일(신규 경고 없음). 옛 이름의 산출물(`ge_engine.dll`/`ge_python.pyd` 등)은 빌드 디렉터리에서 삭제. `CLAUDE.md`의 2026-08-10 버그 기록처럼 **날짜가 박힌 과거 기록은 그대로 두고**(옛 파일명을 언급한 역사적 사실이므로), 안내 문구만 덧붙였다 — 프리팹 계획 문서(`docs/PREFAB_IMPLEMENTATION_PLAN.md`)처럼 "현재 상태를 설명하는" 문서만 새 이름으로 갱신.

### P1.5 — Motion Mixer (Motion Editor 뷰포트 공유 + 커스텀 리깅 임포트/믹싱)

설계 문서: [docs/MOTION_MIXER_IMPLEMENTATION_PLAN.md](docs/MOTION_MIXER_IMPLEMENTATION_PLAN.md) — v1 초안 위에 사용자가 제시한 "착수 계약서 v3"(API 시그니처/JSON 포맷/유닛 테스트 명세까지 확정)가 부록으로 결합되어 있고, 실제 구현은 v3 기준(Phase 1~5, 4A/4B 분리)을 따른다. 임포트 포맷은 FBX/glTF가 아니라 자체 JSON. Phase 1~3(스켈레톤 자료구조/키프레임 임포터/호환성+레이어 믹싱, 전부 C++ 유닛테스트)은 렌더링과 무관하게 순차 진행 가능하고, Phase 4A(단일 뷰포트 공유 + bone hierarchy line 렌더링)부터 눈으로 검증 가능해진다. Phase 4A는 RenderSystem이 아직 없는 상태(P0-2였던 렌더링 파이프라인은 2026-08-15에 그리드까지만 연결됨, §5 참고) 위에 얹히므로 그 전제를 다시 확인하고 시작해야 함.

- ✅ **Phase 1 완료(2026-08-15)** — `engine/animation/`에 `Bone`/`Skeleton`/`SkeletonSignature`/`Transform`/`Pose` 구현, `SkeletonTests.cpp` 8/8 통과(계약서 §F의 (a)~(d) 전부 포함). 상세는 계획 문서 부록 B. 부수적으로 `SimpleEngineTests` 테스트 타겟이 그동안 한 번도 끝까지 링크된 적이 없었던 빌드 버그 2건(`tinyobjloader` 링크 누락, 제거된 `glfw` 죽은 참조)도 같이 고쳐서 처음으로 전체 스위트(315개)를 끝까지 돌려봄 — 310 통과/5 실패(실패 5개는 Motion Mixer와 무관한 기존 코드, 범위 밖).
- ✅ **Phase 2 완료(2026-08-15)** — `KeyframeClip`/`AnimationSampler`(`Sample`, `ShortestSlerp`)/`AnimationPlayer`/`SkeletonJsonIO`/`ClipJsonIO` 구현, 신규 테스트 26개 전부 통과. 상세는 계획 문서 부록 C. 전체 스위트 341개 — 336 통과/5 실패(Phase 1과 동일한 기존 무관 실패, 새 회귀 없음).
- ✅ **Phase 3 완료(2026-08-15)** — `LayerMixer`(`MaskPreset`, `LayerSpec`, `MaskWeightForBone`, `MixerError`, `MixLayers`) 구현, 신규 테스트 11개 전부 통과(pairwise 누적 증명, 위계 불일치 mask 확인, 3-레이어 중 3번째 비호환 시 인덱스 정확성 포함). 상세는 계획 문서 부록 D. 전체 스위트 352개 — 347 통과/5 실패(Phase 1/2와 동일한 기존 무관 실패, 새 회귀 없음). **여기까지가 계약서가 명시한 "핵심 기술 가설 검증 직전" 지점.**
- ✅ **Phase 4A 완료(2026-08-15)** — Motion Editor가 Scene Editor와 같은 `EngineViewport` 인스턴스를 재사용하도록 재구성(단일 인스턴스, §C11), `BoneLineRenderer`(`GL_LINES`) + `MotionPreviewState` + `RenderMode` 구현, Python 바인딩(`AnimationBindings.cpp`) 추가, 샘플 자산(`engine/assets/motion/`) 추가. 신규 유닛테스트 9개 전부 통과. **부수 발견**: `glViewport`가 창 생성 시 한 번만 설정되고 리사이즈에 전혀 반응하지 않던 버그(Motion Editor처럼 뷰포트가 다른 크기 레이아웃으로 재부모 이동하면 검은 화면) — `Engine::HandleWindowResize` + `viewport.py`의 `resizeEvent` 추가로 수정. 에디터 실행 로그로 스켈레톤/클립 로드 파이프라인 동작 확인, 사용자가 직접 화면에서 확인함. 상세·미확인 항목은 계획 문서 부록 E. 전체 스위트 361개 — 356 통과/5 실패(기존 무관 실패, 새 회귀 없음).
- ✅ **Phase 5 완료(2026-08-15)** — `MotionPreviewState`를 "base + overlay 1개"에서 "base + 레이어 N개 스택"(id 기반, 인덱스 재배치 없음)으로 확장(`AddLayer`/`RemoveLayer`/`ClearLayers`/`SetLayerFrame`/`SetLayerWeight`/`SetLayerMask`/`CheckClipCompatible`), 신규 `engine/editor/panels/motion_mixer.py`(`MotionMixerPanel`)로 실제 편집 가능한 믹서 UI 구현 — 스켈레톤/클립 자산 스캔 + `CheckClipCompatible`로 호환 클립만 dropdown에 노출, 레이어별 weight/frame/mask 슬라이더, 디스크 임의 위치에서 커스텀 스켈레톤/클립 "가져오기". `animation_preview.py`에 장착, `motion_editor.py`의 기존 `preview.canvas.update()`가 공유 뷰포트 모드(`canvas=None`)에서 항상 `AttributeError`로 죽던 잠재 버그도 같이 발견/수정. C++ 신규/변경 테스트 12개(레이어 스택 id 안정성, 호환성 사전검사, pairwise override 포함) 전부 통과, 전체 스위트 364개 — 359 통과/5 실패(기존 무관 실패, 새 회귀 없음). 실제 에디터 실행(Motion Editor 전환 → Idle+Attack+Wave 3-클립 레이어 조작 → 제거/재추가)으로 스크린샷 확인 — 뷰포트에 본 라인이 실시간으로 반영됨. 상세는 계획 문서 부록 F.
- ⬜ Phase 4B — CPU 스키닝 (다음 단계, `*.mesh.json` 포맷 신설 선행)

### P1.6 — 프리팹 시스템

설계 문서: [docs/PREFAB_IMPLEMENTATION_PLAN.md](docs/PREFAB_IMPLEMENTATION_PLAN.md)(2026-08-19 작성) — [docs/MASTER_PLAN.md](docs/MASTER_PLAN.md) §6.2에서 지적된 대로 저장소 전체에서 프리팹 관련 코드가 `AssetType::Prefab` enum 값 하나뿐(구현률 0%)임을 확인한 뒤 작성. 기존 `ComponentRegistry` 리플렉션 직렬화(PIE 스냅샷이 쓰는 `SerializeRegistry`와 동일 메커니즘)와 `EditorAPI`/`Transaction`/`CommandManager` Undo 계층을 재사용하는 방향으로 Phase 1~4(+확장 과제 Phase 5)를 분해했다. 엔티티 계층(부모-자식) 컴포넌트가 ECS에 없어서 v1은 엔티티 1개짜리 프리팹으로 범위를 좁혔고, `engine/asset/`(AssetManager/AssetRegistry)은 어디에도 연결되지 않은 고아 모듈임을 확인해 우회하기로 했다.

- ✅ **Phase 1 완료(2026-08-19)** — 신규 `engine/prefab/`(`SerializeOptions.h`, `PrefabInstanceComponent.h`, `PrefabAsset.h/.cpp`) + `engine/ecs/Reflection.cpp`에 `PrefabInstanceComponent` 리플렉션 등록 함수(`RegisterPrefabComponentsReflection()`, 기존 5종 컴포넌트와 같은 자리) 추가. **구현 착수 직전 리뷰에서 계획 문서의 모듈 경계가 한 번 더 정정됨**: 초안은 `SerializeEntityComponents`를 `SerializeRegistry()`와 같은 파일(`ecs/Reflection.h`)에 두려 했으나, 그러면 `ecs/`가 `prefab/`의 `SerializeOptions` 타입을 알아야 해서 금지된 의존 방향(`Reflection → Prefab`)이 생긴다는 게 드러남 — `SerializeOptions`/`SerializeEntityComponents`를 전부 `prefab/`로 옮기고, `SerializeRegistry()`(`Reflection.cpp`)는 아예 건드리지 않는 것으로 최종 확정(의존 방향은 `prefab → ecs`만 허용, 짧은 for-loop 중복은 감수 — SRP 기준 "변경 이유가 다르면 분리"). `ApplyToEntity`는 외부 시그니처는 그대로 두고 내부를 `CaptureSnapshot`/`SynchronizeComponents`(`RemoveMissingComponents`+`DeserializePrefabComponents`)/`RestoreSnapshotOnFailure` 5개 private 헬퍼로 분리(strong guarantee 계약이 코드 구조로 그대로 드러나도록). `PrefabAsset` 자체는 SRP를 이유로 더 쪼개지 않고 하나의 응집된 클래스로 유지(사용자 지침 — "실제 변경 이유가 갈라질 때 분리하는 게 SRP고, 파일 개수를 늘리는 건 SRP가 아니다"). 신규 `engine/tests/PrefabAssetTests.cpp` 19개 테스트(계획한 16개 항목을 좀 더 세분화) 전부 통과, 전체 스위트 392개 — 387 통과/5 실패(기존 무관 실패 5개와 정확히 동일, 새 회귀 없음). 렌더링/에디터 변경 없음(계획대로 C++ 유닛테스트만).
- ✅ **Phase 2 완료(2026-08-19)** — 신규 `engine/editor/commands/InstantiatePrefabCommand.h/.cpp`(`CreateEntityCommand`와 동일한 Redo 패턴 — 최초 Apply는 파일을 읽어 `SpawnInto`, Redo는 같은 UUID로 엔티티만 재생성하고 **최초 Apply 때 캐싱해둔 `PrefabAsset`을 재사용**해 `ApplyToEntity`로 재적용함 — 파일이 Undo/Redo 사이에 바뀌거나 삭제돼도 Redo가 항상 Undo가 지운 것과 똑같은 결과를 내도록). `EditorAPI::InstantiatePrefab(path, std::optional<Vec3> position)` 추가(`Dispatch()`를 그대로 재사용 — 트랜잭션 자동 합류), `EditorBindings.cpp`에 `instantiate_prefab` 파이썬 바인딩 추가(`<pybind11/stl/filesystem.h>`로 `std::filesystem::path` 자동 변환). 신규 `engine/tests/InstantiatePrefabCommandTests.cpp` 9개(Apply 값 검증·position override·Undo·Undo→Redo(파일 삭제 시나리오 포함)·`CommandManager` Execute/Undo/Redo 스택·`Transaction` 그룹 Undo·존재하지 않는 경로/손상된 데이터 실패 시 엔티티 미생성·null registry) 전부 통과, 전체 스위트 401개 — 396 통과/5 실패(기존 무관 실패 5개와 정확히 동일, 새 회귀 없음). **실측 검증**: `main.py`의 기존 "Test Cube"(원점) 옆에 `assets/prefabs/Barrel.prefab.json`을 `(3,0,0)`에 인스턴스화하는 호출을 추가하고 에디터를 실제로 실행 — 로그에 `InstancedBatchManager::UploadInstanceBuffer - Uploaded 2 instances`(같은 메시 배치로 함께 그려짐) 확인, 스크린샷으로 두 큐브가 그리드 위에 나란히 정상 렌더링되는 것을 직접 확인.
- ✅ **Phase 3 완료(2026-08-19)** — `EditorAPI::CapturePrefab(entity, path)` 추가(`PrefabAsset::CaptureFromEntity`+`SaveToFile` 래핑) — ECS를 바꾸지 않는 순수 읽기+파일쓰기라서 다른 EditorAPI 메서드와 달리 `Dispatch()`/`CommandManager`를 거치지 않음(되돌릴 ECS 상태가 없음), `EditorBindings.cpp`에 `capture_prefab` 바인딩 추가. `panels/scene_hierarchy.py`에 "Create Prefab..." 우클릭 메뉴(엔진 연결 시에만 노출) + `assets/prefabs/`를 기본 경로로 하는 저장 대화상자 추가. 신규 `panels/prefab_browser.py`(`motion_mixer.py`의 `os.listdir`+확장자 필터 스캔 관례를 그대로 따름, `engine/asset/`은 여전히 미사용) — `assets/prefabs/*.prefab.json` 목록 + Instantiate 버튼(Phase 2의 `instantiate_prefab`을 position 인자 없이 호출 — §2.8에 따라 캡처 당시 위치 그대로 스폰). `main.py`에 `QDockWidget`으로 붙여서 Scene Editor/Play Mode/Motion Editor 어느 모드에서도 접근 가능. **실측 검증**: 실제 에디터를 실행해 (1) `(-3,0,0)`에 별도 엔티티를 만들고 `QFileDialog.getSaveFileName`을 몽키패치해서 실제 우클릭 메뉴와 동일한 코드 경로로 "Create Prefab..." 실행 → 파일 생성 확인, (2) 원본 엔티티를 삭제해 "설계도와 인스턴스는 독립적"(§2.1) 전제를 실측으로 검증, (3) Prefab Browser 목록에서 방금 만든 프리팹을 선택해 Instantiate 버튼 클릭 → 새 엔티티가 원본 없이도 정상 스폰·렌더링됨을 스크린샷으로 확인(Test Cube/Barrel/새 인스턴스 3개 큐브가 나란히 표시, 하단 Prefabs 도킹 패널에 Barrel/Statue 두 항목 표시, 상태 바에 "프리팹 인스턴스화 완료" 메시지 확인).
- ✅ **Phase 4 완료(2026-08-19, 라이브 스크린샷 검증은 환경 문제로 보류)** — 신규 `engine/editor/commands/RevertPrefabInstanceCommand.h/.cpp`: `Apply()`/`Undo()` 둘 다 "임시 `PrefabAsset`을 만들어 그 `ApplyToEntity`를 호출"하는 같은 원리를 재사용한다(Apply는 프리팹 파일의 `componentsData`를, Undo는 Revert 직전에 캡처해둔 스냅샷을 담은 `PrefabAsset`을 대상으로) — Definition A/strong guarantee 로직을 중복 구현하지 않음. `EditorAPI::RevertPrefabInstance(entity)` 추가(ECS를 바꾸므로 `CapturePrefab`과 달리 `Dispatch()`를 거쳐 Undo 가능), `EditorBindings.cpp`에 `revert_prefab_instance` 바인딩 추가. `panels/inspector.py`에 `PrefabInstanceHeader`(신규) — 선택한 엔티티에 `PrefabInstanceComponent`가 있으면 일반 컴포넌트 위젯 대신 "Prefab: `<name>` [Revert] [Apply(비활성)]" 헤더를 맨 위에 표시(Apply는 §2.5의 명시적 스텁 정책대로 비활성+안내 툴팁). 신규 `engine/tests/RevertPrefabInstanceCommandTests.cpp` 9개(Definition A 회귀 확인·원본 파일 삭제 시 미변경+에러(§2.9)·버전 불일치에도 최신 내용 적용(§2.3)·Undo→Redo(파일 삭제 시나리오 포함)·`CommandManager` 스택·프리팹 인스턴스 아닌 엔티티 에러·null registry) 전부 통과, 전체 스위트 410개 — 405 통과/5 실패(기존 무관 실패 5개와 정확히 동일, 새 회귀 없음), 빌드 경고 0개. **라이브 검증(2026-08-20, 이름 개명 이후 재시도로 완료)**: 애초 막혔던 Windows 애플리케이션 제어 정책 문제는 `ge_python`/`ge_engine` → `Quarter Flying` 이름 개명(P1 Phase 6) 이후 재빌드한 `quarterflying.pyd`가 문제없이 로드되면서 자연히 해소됐다. 실제로 에디터를 띄워 검증하는 과정에서 프리팹과 직접 관련된 버그 2건 + 무관한 선행 버그 2건을 추가로 발견·수정했다:

- **[신규 발견/수정] `inspector.py`가 `from ge_python import Entity`를 4곳에서 직접 import** — `engine_binding.py` 래퍼를 완전히 우회하고 있어서(Phase 3 마이그레이션 때 이 파일 하나가 누락됨), 이름 개명 직후 `ModuleNotFoundError: No module named 'ge_python'`로 즉시 깨졌다. 전부 모듈 최상단에서 이미 import된 `ge_python`(래퍼 별칭)을 재사용하도록 수정 — Phase 4가 추가한 `_on_revert_requested`도 같은 실수를 반복하고 있었다.
- **[신규 발견/수정, 프리팹과 무관한 선행 버그] `scene_hierarchy.py::_refresh_inspector`에서 `GetEntityName(entity_id)`가 `Entity`로 감싸지 않은 raw id를 그대로 넘김** — pybind11이 "incompatible function arguments"로 즉시 예외를 던져 `_refresh_inspector`의 try 블록 전체(그 아래의 프리팹 헤더 로직 포함)가 실제 엔진 연결 상태에서는 한 번도 끝까지 실행된 적이 없었다. `Entity(entity_id)`로 감싸도록 수정.
- **[신규 발견/수정, 프리팹과 무관한 선행 버그] `scene_hierarchy.py::refresh_from_engine()`이 `GetAllEntities()`가 돌려주는 `Entity` 객체를 그대로 `EntityItem.entity_id`에 저장** — 이 값이 `entity_selected`(`Signal(int)`)로 emit되는 순간 PySide6가 `Entity → int` 변환을 못 해 조용히 `0`으로 깨졌다(예외 없이 실패 — 발견하기 어려운 종류). `.id`로 미리 풀어 항상 raw int를 유지하도록 수정. 또한 이 메서드가 500ms 타이머로 반복 호출되며 `tree.clear()`가 선택 상태를 매번 지우고 복원하지 않아서, 뭘 선택해도 0.5초 안에 Inspector가 "No Selection"으로 돌아가는 별개의 버그도 같이 발견해 수정(갱신 전 선택을 기억했다가 재선택).
- **[C++ 버그 수정]** `PrefabAsset::FromJson`이 `"version"` 필드의 음수 여부를 검사하지 않아, `-1` 같은 값이 `.get<uint32_t>()`에서 조용히 거대한 값(4294967295)으로 wrap-around됐다. 명시적 음수 체크 추가 + 회귀 테스트(`FromJson_NegativeVersion_ReturnsError`) 추가.
- **[하드코딩 정리]** `"PrefabInstanceComponent"` 문자열 리터럴이 `PrefabAsset.cpp`에 2곳 중복돼 있던 것을 `PrefabInstanceComponent.h`의 `kPrefabInstanceComponentName` 상수로 통합.

수정 후 재검증: `PrefabAssetTests` 20개(신규 1개 포함) + 전체 스위트 411개 — 406 통과/5 실패(기존 무관 실패와 동일) 전부 통과. 라이브 스크립트로 (1) Scene Hierarchy에서 실제 엔티티 선택 → Inspector가 정확히 그 엔티티(`entity_id=3`)를 반영, (2) Inspector 위젯 트리에서 `PrefabInstanceHeader` 인스턴스 1개 확인 — 자식 위젯 텍스트가 "📦 Prefab: Barrel" / "Revert" / "Apply"로 정확히 렌더링됨을 직접 확인, (3) Revert 클릭 → 위치가 프리팹 파일의 캡처된 값(0,0,0)으로 정확히 복원되고 상태 바에 "프리팹 되돌리기 완료" 표시됨을 확인.
- ⬜ Phase 5(확장 과제) — 계층 컴포넌트 → 다중 엔티티 프리팹 → 베리언트

### P1.7 — VFX Lite (파티클 시스템)

범위/철학 문서: [docs/VFX_LITE_PLAN.md](docs/VFX_LITE_PLAN.md)(2026-08-20 작성, 같은 날 성능 철학 절 추가) — 아직 구현 계획서 단계 전, "무엇을 만들고 무엇을 만들지 않을지"만 확정한 상태. 18개 값(Spawn/Motion/Size/Color/Rotation/Random, 초안의 "15개"는 실제로 세어보니 오류라 정정됨)짜리 최소 효과 시스템으로 범위를 제한하고 Collision/Sub-emitter/GPU Particle 등은 명시적으로 제외. 렌더링은 새 파이프라인 없이 기존 `InstancedBatchManager`(2026-08-18 검증됨) 재사용을 전제하고, 저장/재현은 새 포맷을 만들지 않고 방금 완성한 프리팹 시스템(P1.6) 위에 얹는 방향으로 잡아뒀다. §5에 성능 철학을 확정 체크리스트로 정리했다 — 핵심은 "파티클을 최적화한다"가 아니라 "파티클이 비싸질 수 있는 경로(파티클별 ECS 엔티티·순환 버퍼·프레임별 동적 할당·파티클→외부 시스템 호출)를 처음부터 안 만드는 것". 이후 이어진 리뷰에서 Burst+Rate 동시 사용, Direction(방향+범위), 이펙트 종료 조건(파티클 Lifetime이 곧 이펙트 생사 결정, 별도 파라미터 없음), "Draw Call은 항상 1개"가 아니라 "같은 `InstancedBatchKey`(Mesh/Material/Shader)를 쓰는 동안만 1개"라는 정정까지 전부 반영됐다. 남은 미해결 항목은 **§6.1 텍스처/머티리얼 파이프라인 하나**뿐 — `engine/asset/`(고아 모듈) 전체를 살리기보다 VFX Lite 전용 최소 Sprite 경로를 먼저 검토하는 쪽으로 기울어 있으나 아직 확정은 아니다. 착수는 보류 — §6.1 확정 후 `docs/VFX_LITE_IMPLEMENTATION_PLAN.md`(Phase 분해 포함)를 별도로 작성할 예정.

### P2 — 빌드 인프라 하드닝

이번에 실제로 발견한 문제들이라 재발 방지 차원에서 별도 정리:

- `cmake/Dependencies.cmake`의 `CheckFetchContentDependency` 패턴이 이번에 3개(`glew_s` 미생성, `.gitignore` 오탐, PARENT_SCOPE 누락) 문제의 근원이었다. 비슷한 실수가 다른 의존성(pybind11/fmt/glm/json)에도 잠재하지 않는지 한 번 점검할 가치가 있다.
- `tinyobjloader`는 `TINYOBJLOADER_DISABLE_FAST_FLOAT`로 우회 중 — 상위 버전에서 fast_float 호환성이 고쳐지면 이 우회를 제거하고 원래 성능을 되찾을 수 있는지 주기적으로 확인.
- Debug 빌드(`--config Debug`)는 여전히 `python3XX_d.lib` 부재로 링크 실패한다. 디버그 심볼이 필요해지면 `RelWithDebInfo`로 가는 게 맞다(이미 CLAUDE.md에 기록).

### P3 — 저장소 위생 (급하지 않음)

- git 이력이 이번 세션에서 다소 뒤섞였다(`.gitignore` 수정과 컴파일 버그 수정이 한 커밋에 같이 들어간 지점 등). 기능에는 영향 없지만, 여유 있을 때 `git rebase -i`로 정리하면 이력을 근거로 추적하기 더 쉬워진다.
- `D:\Quarter Flying`(구버전 사본, USB 추정)을 계속 들고 갈지 정리할지 결정. 현재는 완전히 별개 사본으로 방치된 상태.
- 기존 `PLAN.md`(저장소 미접근 상태로 작성된 추측 기반 정리문)와 `PROBLEM_ANALYSIS_REPORT.md`/`UNUSED_AND_LEGACY_REPORT.md`는 이번 세션에서 검증된 사실과 상당 부분 어긋나거나 이미 해소됐을 가능성이 있다 — 다음에 참고할 때는 이 로드맵과 `docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md`의 실측 결과를 우선한다.

---

## 3. 진행 순서 요약

```
P0-1 Play Mode 윈도우 클래스 버그 수정
P0-2 3D 뷰포트 실제 렌더링 확인
        │
        ▼
P1  Python 바인딩 Phase 2 → 3 → 4 → 5 → 6
        │
        ▼
P1.5  Motion Mixer Phase 1 → 2 → 3 (C++, 유닛테스트) → 4 (뷰포트 공유) → 5 (믹서 UI)
        │
        ▼
P1.6  프리팹 Phase 1 (자료구조) → 2 (인스턴스화) → 3 (저작 UI) → 4 (Revert)
        │
        ▼
P2  빌드 인프라 하드닝 (병행 가능)
        │
        ▼
P3  저장소 위생 정리 (여유될 때)
```

P0 두 개는 렌더링 파이프라인의 기본 동작 여부를 가리므로 P1(Protocol 설계)보다 먼저 처리하는 걸 권한다 — Protocol에 렌더링 관련 상태(예: Play Mode 성공 여부, 뷰포트 초기화 결과)를 반영해야 한다면, 지금 순서를 바꾸면 나중에 다시 설계해야 한다.

---

## 4. 2026-08-14 업데이트 — P0 실행 결과 + 핵심 신규 발견

### 완료된 수정 (전부 실행/로그/헤드리스로 검증됨)

| 항목 | 파일 | 내용 |
|---|---|---|
| P0-1 | `engine/platform/Win32Platform.cpp` | `RegisterClassExW` 실패 시 `ERROR_CLASS_ALREADY_EXISTS`면 기존 클래스 재사용하도록 수정. 두 번째 `EngineViewport`(Play Mode)도 정상 초기화됨을 로그로 확인 |
| — | `engine/editor/main.py` | `engine.CreateWorld()` → `CreateWorld("Untitled Scene")` (바인딩이 필수 인자였음). ECS 연결 실패의 직접 원인 |
| — | `engine/editor/viewport.py` | `EngineViewport`에 `WA_NativeWindow`/`WA_PaintOnScreen`/`WA_NoSystemBackground` + `paintEngine()→None` 추가. **"검은 화면"의 실제 원인은 렌더러가 아니라 Qt 전역 스타일시트(`QWidget{background-color}`)가 네이티브 임베딩 뷰포트 위에 매 프레임 덧칠하던 것**이었음 |
| — | `engine/editor/main.py` | `scene_path = "assets/scene.json"`가 cwd 상대경로라 `engine/editor`에서 실행 시 항상 못 찾던 버그를 `_EDITOR_DIR` 기준 절대경로로 수정 |
| — | `engine/bindings/ECSBindings.cpp` | `World` 바인딩에 `Play`/`Pause`/`Stop` 추가 (C++엔 있었지만 바인딩 누락으로 Play 버튼이 항상 `AttributeError`) — 재빌드 완료 |
| — | `engine/editor/main.py` | ECS 연결 후 기본 "Main Camera" 엔티티 생성(Transform+CameraComponent, `isMainCamera=true`) 추가 |
| 테마 | `engine/editor/style/theme.py`, `panels/status_bar.py` | 고채도 시안+핑크 네온 팔레트 → 단일 블루 강조색 기반 뉴트럴 다크 톤으로 리프레시. `status_bar.py`의 하드코딩 헥스코드를 `theme.COLORS` 참조로 정리 |

### 미해결 — 다음에 이어갈 것

- scene.json → 실제 ECS 엔티티 생성 브리지 미구현 (`demo_scene_integration.py`가 JSON을 UI 트리 위젯에만 반영하고 실제 `World`/`Registry`에는 아무것도 안 만듦) — RenderSystem 작업과 자연히 묶어서 진행하는 게 합리적.
- 씬 로드 로그가 "Loaded scene with 5 items"인데 실제 화면(사용자 스크린샷)엔 트리가 비어 보이는 현상 — 원인 미확정, 재확인 필요.

---

## 5. 2026-08-15 업데이트 — GL 렌더링 파이프라인 최초 연결 + 디버그 그리드

### 핵심 신규 발견 (이번 세션)

이전까지 "RenderSystem이 없다"까지는 파악돼 있었는데, 그보다 한 단계 더 아래가 빠져 있었다: **엔진 전체에 OpenGL 렌더링 컨텍스트를 생성하는 코드가 단 한 곳도 없었다** (`wglCreateContext`/`wglMakeCurrent`/`SwapBuffers`/`glewInit` 전무). `Engine`의 `subsystems` 목록에 `Renderer`를 등록하는 코드도 없어서, `Renderer::Tick`/`LateTick`이 애초에 호출된 적이 없었다. "3D 뷰포트가 검게/희게 나온다"는 렌더 결과가 아니라 그냥 Win32 창의 기본 GDI 배경색이었다.

### 완료된 작업 (실행/스크린샷으로 검증됨)

| 항목 | 파일 | 내용 |
|---|---|---|
| GL 컨텍스트 생성 | `engine/platform/IPlatform.h`, `Win32Platform.h/.cpp` | `CreateGraphicsContext()`/`PresentFrame()` 추가 (PIXELFORMATDESCRIPTOR → `wglCreateContext` → `wglMakeCurrent` → `glewInit`, 매 프레임 `SwapBuffers`) |
| Renderer 서브시스템 등록 | `engine/core/Engine.cpp` (`InitializeCoreSystems`) | GL 컨텍스트 생성 직후 `RegisterSubsystem<Renderer>()` 호출 추가. 이전엔 이 등록 자체가 없어서 `Renderer::Initialize/Tick/LateTick`이 한 번도 안 불렸음 |
| 임시 기본 카메라 | `engine/core/Engine.cpp`, `Engine.h` | ECS `CameraComponent` → 렌더러 `Camera`를 잇는 RenderSystem이 아직 없어서, 고정값(`position (0,5,15)`, `lookAt` 원점, `fov 60°`) `Camera`를 Engine이 직접 만들어 `Renderer::SetMainCamera()`에 전달. **ECS Main Camera 엔티티 값과는 아직 연결 안 됨** — Inspector에서 카메라를 옮겨도 반영 안 됨 |
| 디버그 그리드 | `engine/renderer/DebugGridRenderer.h/.cpp` (신규), `Renderer.h/.cpp` | `CommandList` 추상화가 `GL_TRIANGLES`만 지원해서(`DrawArrays` 등 하드코딩) 라인 그리기가 안 됨 → 이 클래스가 직접 OpenGL 호출로 우회. `Renderer::Render()`가 RenderGraph 패스 개수와 무관하게 항상 clear + 그리드를 그리도록 최소 기준선 추가 |
| 빌드 | `engine/CMakeLists.txt` | `RENDERER_SOURCES`에 `DebugGridRenderer.cpp` 추가 |

### 남은 것

- **[여전히 P0급] ECS ↔ 렌더러 RenderSystem 부재는 그대로다.** 이번 작업은 "렌더링 파이프라인 자체가 살아서 화면에 뭔가 그린다"까지만 만든 것이고, `RenderableComponent.meshHandle`을 읽어 실제 지오메트리 draw call을 내는 시스템은 여전히 없다. 별도 설계 작업 필요(메시 리소스 등록 매니저 + RenderSystem 신설 + InstancedBatchManager 연동) — 이번 GL 컨텍스트 작업으로 그 위에 얹을 기반은 마련됨.
- 카메라가 ECS와 분리된 고정값이라, 그리드의 위치/각도가 실제 화면에서 다소 비스듬하게 보임(사용자 확인) — 카메라 위치/FOV/종횡비 튜닝 또는 ECS 카메라 연동이 다음 순서.
- `Win32Platform::Shutdown()`이 외부 핸들(Qt 임베딩) 창도 `DestroyWindow`를 시도하는 기존 동작은 이번에 건드리지 않음 — GL 컨텍스트 정리(`wglDeleteContext`/`ReleaseDC`)만 대칭적으로 추가.

---

## 6. 2026-08-18 업데이트 — RenderSystem 신설 + P0-2 완결 (ECS 지오메트리 실제 렌더링 확인)

### 배경

§5에서 "여전히 P0급"으로 남겨뒀던 항목 — `RenderableComponent.meshHandle`을 읽어 실제 draw call을 내는 System이 없다는 것 — 을 이번에 신설했다. 목표는 단순히 "System을 하나 추가하는 것"이 아니라 "ECS에 엔티티를 만들면 실제로 화면에 뭔가 그려지는가"를 처음부터 끝까지 검증하는 것이었는데, 그 과정에서 이 파이프라인의 여러 지점에 각각 독립적인 pre-existing 버그 4개가 겹쳐 있어서(전부 "실행된 적이 없어서 발견 안 됐던" 종류) 실제로 그려지기까지 예상보다 훨씬 오래 걸렸다. 아래 순서대로 하나씩 걷어내야 화면에 뭔가 보였다.

### 구현

| 항목 | 파일 | 내용 |
|---|---|---|
| RenderSystem (신규) | `engine/ecs/RenderSystem.h/.cpp` | `Transform`+`Renderable` 컴포넌트를 읽어 `InstancedBatchManager`에 배치를 채우는 첫 ECS System. `meshHandle`이 명시적으로 `RegisterMesh()` 안 됐으면 절차적 유닛 큐브로 대체(24 vertex/36 index) — 실제 `*.obj`→핸들 매핑 파이프라인은 아직 없음(다음 과제). `CanRunInParallel()=false` 명시(GL 자원을 건드리므로). 매 프레임 `isMainCamera` ECS 엔티티의 Transform → `Camera` position/lookAt도 동기화(단, projection은 안 건드림 — resize 이벤트 경로와 충돌 방지). 순수 로직(`ComposeWorldMatrix`, `MakeMeshBatchKey`)은 GL 없이 유닛테스트 가능하도록 분리 |
| SceneMeshRenderer (신규) | `engine/renderer/SceneMeshRenderer.h/.cpp` | RenderSystem이 채운 배치를 실제로 그림. `pbr_instanced.frag`(기존 셰이더)를 안 쓰고 새로 작성 — 그쪽은 인스턴스별 color/roughness가 uniform 폴백이라 실제 인스턴스 attribute와 어긋나 있었고, point-light 배열(1/distance² 감쇠)이라 오브젝트가 광원과 겹치면 0-나눗셈 위험이 있었음. 대신 감쇠 없는 방향광 하나 + 실제 인스턴스 attribute(location 7 color)를 쓰는 단순 셰이더 |
| `InstancedBatchManager::ClearInstances()` (신규 메서드) | `engine/renderer/InstancedBatchManager.h/.cpp` | RenderSystem처럼 "매 프레임 처음부터 다시 나열"하는 소비자를 위해 GPU 자원은 유지한 채 CPU 인스턴스 목록만 비움 |
| `Engine::CreateWorld()` | `engine/core/Engine.cpp` | 새 World 생성 시 `RenderSystem`을 자동 등록(`defaultCamera`/`Renderer::GetInstancedBatchManager()` 주입) — Python `engine.CreateWorld()` 호출 경로가 이 한 곳뿐이라 빠짐없이 커버됨 |
| `Renderer::SetViewportSize()` (신규) | `engine/renderer/Renderer.h/.cpp` | 아래 "버그 4" 참고 — 매 프레임 `glViewport` 재적용 |
| 테스트 | `engine/tests/RenderSystemTests.cpp` (신규) | `ComposeWorldMatrix`(TRS 합성 순서 포함 5개) + `MakeMeshBatchKey`(2개) + null 세이프가드(2개) = 9개, GL 없이 순수 로직만 검증 |
| 에디터 검증용 엔티티 | `engine/editor/main.py` | ECS 연결 시 "Main Camera"와 함께 "Test Cube"(Transform + Renderable, 기본값)도 자동 생성 — RenderSystem이 실제로 뭔가 그리는지 매 실행마다 눈으로 바로 확인 가능 |

### 발견하고 고친 pre-existing 버그 4건 (전부 "실행된 적이 없어서" 발견 안 됐던 것들)

1. **`Components.h`가 `<string>`을 직접 include 안 함** — `AIComponent`가 `std::string`을 쓰는데도, 이 헤더 자체는 `<string>`을 포함하지 않고 있었다. 지금까지 이 헤더를 include하는 다른 파일이 항상 먼저 `<string>`을 끌어와 준 우연 덕에 컴파일됐던 것 — `RenderSystem.h`가 그 우연이 처음 깨진 include 순서라 C2039로 드러남. `Components.h`에 직접 `#include <string>` 추가로 해결.
2. **`Mesh::drawInstanced()`가 자신의 `m_vao`(mesh 고유 3-attribute VAO, 인스턴스 attribute 없음)를 다시 bind해버림** — 유일한 호출부인 `InstancedBatchManager::RenderBatch()`가 이미 인스턴스 attribute(model 행렬/color 등)까지 포함하는 `batch->instanceVAO`를 bind해둔 상태에서 이 함수를 부르는데, 함수 내부에서 `m_vao`로 다시 bind해버려서 그 인스턴스 데이터가 전부 무시됐다(model 행렬이 정의되지 않은 값이 됨). draw call 자체는 GL 에러 없이 "성공"해서 원인 추적이 까다로웠다 — `CLAUDE.md`의 "`InstanceData` GPU 레이아웃 미검증" 항목과 뿌리가 같다(이 인스턴싱 경로가 이번에 처음 실제로 실행됨). `drawInstanced()`에서 `glBindVertexArray` 호출 두 줄 제거로 해결.
3. **`WorldManager::CreateWorld()`가 만든 World가 자동으로 active가 되지 않음** — `WorldManager::Update()`는 `activeWorld`만 틱하는데, `main.py`의 `engine.CreateWorld("Untitled Scene")` 호출 뒤에 `SetActiveWorld()`를 부르는 코드가 어디에도 없었다(애초에 **`SetActiveWorld`가 Python 바인딩에 노출조차 안 되어 있었음** — `EngineBindings.cpp`에 `GetActiveWorld`만 있고 `SetActiveWorld`는 없었다). 그 결과 `World::Update()`가 한 번도 안 불려서 등록된 ECS System(RenderSystem 포함)이 전부 죽은 코드였다. 엔티티/컴포넌트 생성 자체는 registry 직접 조작이라 active 여부와 무관하게 됐기 때문에, Scene Hierarchy에 엔티티가 정상적으로 보이는 것만으로는 이 버그가 전혀 티가 안 났다. `EngineBindings.cpp`에 `SetActiveWorld` 바인딩 추가 + `main.py`에서 `CreateWorld` 직후 호출.
4. **[가장 오래 걸린 원인] 매 프레임 `glViewport`가 재적용되지 않음** — 프로세스 안에 `EngineViewport`(=`Engine` 인스턴스, GL 컨텍스트) 여러 개가 존재(Scene Editor용 + Play Mode용)하고, 각자 자기 HWND에서 독립적인 Qt `QTimer`로 `TickFrame()`을 도는데, `wglMakeCurrent`는 스레드 단위 상태라서(같은 Qt 메인 스레드) 어느 한쪽이 `InitializeFromWindowHandle()`을 부르면 그 순간 그 스레드의 "현재 컨텍스트"가 바뀐다. `glViewport`는 컨텍스트별 상태라, 실제로 그리는 시점에는 전혀 다른(엉뚱하게 작은) viewport가 걸려있을 수 있었다 — 실측으로 `glGetIntegerv(GL_VIEWPORT,...)`가 `(0,0,96,480)`을 반환하는데 카메라 projection은 종횡비 1.3을 가정하고 있어서, 그려지는 지오메트리가 가로로 심하게 눌려 보였다(디버그 그리드는 우연히 덜 두드러져서 눈치채기 어려웠다). 근본 원인(같은 프로세스 안 여러 GL 컨텍스트가 뒤섞이는 것 자체)은 별도의 더 큰 아키텍처 작업이라 후속 과제로 남기고, 여기서는 `Renderer::SetViewportSize()`를 추가해 매 프레임 `Render()` 시작 시 우리가 알고 있는 올바른 크기로 `glViewport`를 다시 걸어서 증상을 막았다(`Engine::InitializeCoreSystems`와 `HandleWindowResize` 양쪽에서 갱신).

이 4개를 순서대로 다 고치고 나서야(1→2→3→4 순으로 하나씩 드러남 - 각각을 고쳐야 다음 증상이 보였다) 화면에 정상적으로 음영 처리된 큐브가 그려졌다.

### 검증 결과

- `SimpleEngineTests.exe` 전체 스위트 373개 — 368 통과 / 5 실패(§4A부터 동일한 기존 무관 실패, 새 회귀 없음). `RenderSystemTest.*` 9개 전부 통과.
- 실제 에디터 실행(스크린샷, PrintWindow 캡처): Scene Editor 뷰포트에 그리드 + 원점의 "Test Cube" 엔티티가 올바른 크기·비례·음영(방향광 기반 ambient+diffuse+specular)으로 렌더링됨을 확인. `Entities: 2`(Main Camera + Test Cube)로 상태바에도 반영.

### 남은 것

- **여러 GL 컨텍스트가 같은 스레드에서 뒤섞이는 근본 문제**는 여전히 해결 안 됨 — `SetViewportSize()`는 증상(viewport 크기)만 막았을 뿐, `wglMakeCurrent`가 계속 널뛰는 것 자체는 그대로다. 다른 컨텍스트별 상태(현재 바인딩된 셰이더/텍스처/VAO 등)도 같은 방식으로 오염될 수 있다는 뜻이라, Play Mode·Motion Editor 동시 사용 시나리오는 더 면밀히 재검증이 필요하다.
- ECS Main Camera → `Camera` 동기화(RenderSystem의 카메라 sync 블록)가 **실제로 한 번도 실행되지 않는 것으로 확인됨**(로그에 해당 디버그 라인이 전혀 안 찍힘) — `SetComponentJson`으로 채운 `CameraComponent.isMainCamera`가 `GetComponentArray<CameraComponent>()`로 안 읽히는 것으로 추정되나 원인 미확정. 지금은 `defaultCamera`의 초기 고정값이 우연히 맞아서(에디터가 만드는 카메라 엔티티 위치와 Engine의 하드코딩된 시작값이 같음) 문제가 안 보이지만, Inspector에서 카메라를 옮겨도 반영 안 되는 건 여전하다 — 별도 조사 필요.
  - **2026-09-25 원인 확정 / 유닛 테스트 통과, 화면 검증은 아직**: 원인은 `ECSRegistry.SetComponentJson` 바인딩이 JSON을 `{"CameraComponent": {...}}`로 한 겹 더 감싸서 `deserialize`에 넘긴 것이었다. 필드 이름이 하나도 매칭되지 않아 `isMainCamera`가 영원히 `false`로 남았고, sync 루프가 항상 `continue`했다. 이 바인딩 버그는 그 사이 이미 고쳐져 있었다(`engine/bindings/ECSBindings.cpp` 주석). 다만 sync 경로 자체를 검증하는 테스트가 없었다(기존 테스트는 `camera=nullptr`로 크래시 여부만 확인). 그래서 `engine/tests/RenderSystemTests.cpp`에 `RenderSystemCameraSyncTest` 5개를 추가했다: Main Camera 위치 복사, 비-Main 무시, Transform 변경 후 다음 프레임 추종, 리플렉션 JSON 경로로 `isMainCamera` 설정 후 동기화, 감싼 JSON은 무시됨(과거 원인 문서화). 5개 모두 통과했다. 전체 스위트는 468개 중 463 통과 / 5 실패이고, 실패 5개는 기존과 같은 테스트다. **남은 것:** 에디터 Inspector에서 Main Camera Position을 바꿨을 때 뷰포트가 실제로 따라 움직이는지는 화면에서 확인하지 않았다.
- ✅ **2026-09-30 해결(화면 캡처로 확인)**: `RenderableComponent.meshPath` + `RenderSystem::ResolveMeshHandle` + `editor/scene_instantiation.py`로 scene.json 모델이 실제 ECS 엔티티로 만들어지고 그려진다(docs/REVIEW_BASED_IMPROVEMENT_PLAN.md P0-2). 아래는 해결 전 기록.
- `RenderableComponent.meshHandle`은 여전히 절차적 큐브로만 대체된다 — scene.json의 `objects[].model` 경로를 실제 meshHandle로 매핑하는 브리지, 그리고 `demo_scene_integration.py`가 실제 ECS 엔티티를 안 만드는 문제(§4에서부터 알려진 것)는 별개 후속 작업.
