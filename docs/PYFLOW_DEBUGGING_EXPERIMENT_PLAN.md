# PyFlow 디버깅 효과 실험 계획서

작성 2026-10-16 · 상태: **갈래 A·B 완료** (결과: [A](PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_2026-10-16.md), [B](PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_B_2026-10-22.md)) · 관련: [PYFLOW_COMPARISON_BUG_REPORT_2026-10-16.md](PYFLOW_COMPARISON_BUG_REPORT_2026-10-16.md)

## 1. 질문과 가설

**질문**: 증상만 알고 원인을 찾는 디버깅에서, PyFlow 브리핑을 쓰면 안 쓸 때보다 더 쉬운가?

2026-10-16 점검은 버그를 *찾는* 작업이었고, 결론은 "비슷하거나 영역에 따라 더 어렵다"였다. 다만 근거가 한 번의 점검이고 점검자가 답을 알게 된 뒤의 판단이었다. 이 실험은 **답을 모르는 새 검토자**에게 **같은 증상**을 주고 두 조건을 비교한다.

사전 등록 가설(실행 전에 고정, 결과를 보고 바꾸지 않는다):

- **H1**: C++ 또는 여러 모듈에 걸친 과제(T2~T7)에서 PyFlow 조건이 정답 파일에 더 빨리 도달한다.
- **H0**: 에러 메시지로 grep하면 바로 닿는 과제(T1, T8, T9)에서는 차이가 없다.
- **H2**: PyFlow가 정답 파일 분석에 실패한 과제에서는 PyFlow 조건이 더 느리거나 같다.

## 2. 설계

| 항목 | 내용 |
| --- | --- |
| 조건 | **N**(PyFlow 없음): 스냅샷 코드만. **P**(PyFlow): 같은 스냅샷 + 미리 만든 PyFlow 브리핑 폴더 `_pyflow/` |
| 피험자 | 매 실행마다 새로 띄운 서브에이전트(맥락 없음). 모든 실행에 같은 모델·같은 도구 |
| 과제 | 9개(§3). 각 과제는 원인이 알려진 실제 버그 |
| 배정 | 과제 × 조건 전부 실행(9 × 2). 같은 에이전트가 두 조건을 하지 않는다 |
| 반복 | 1차 1회(18회 실행). 결과가 경계선이면 2회 더(총 54회) |
| 허용 도구 | 파일 읽기·검색, Python 실행(엔진 모듈 없음). **빌드 금지**(옛 스냅샷의 `.pyd`가 없고, 빌드 시간이 결과를 지배한다) |
| 상한 | 실행당 도구 호출 60회 또는 30분. 넘으면 그때까지의 최선 추정을 답한다 |
| 답 형식 | 원인 파일·함수, 원인 설명(무엇을 잘못 가정했나), 근거 행. 쓴 도구 이름은 답에 적지 않는다(블라인드 채점용) |

검증 단계 관례(CLAUDE.md 관례 2)에 비추면 이 실험은 **"원인 위치 추정"까지만** 측정한다. 에이전트가 고친 코드가 실제로 동작하는지는 측정하지 않는다.

## 3. 과제

증상 문구는 원래 수정 커밋의 "증상" 절에서 원인 힌트를 뺀 것이다. 에이전트에게는 **증상 문구만** 준다. 정답은 부록 A에 있고 에이전트에게 주지 않는다.

| ID | 영역 | 스냅샷(수정 직전) | 에이전트에게 주는 증상 |
| --- | --- | --- | --- |
| T1 | Python | `4d637b8` (`2c9c929`의 부모) | Scene Play에서 Sound Test 엔티티의 소리가 나지 않는다. 콘솔에 `[SoundPlayer] 프리로드되지 않은 클립(재생 건너뜀): ../sfx/Blip.wav`가 반복된다. Motion Editor에서 파일로 저장했거나 불러온 액션도 미리듣기가 무음이다. |
| T2 | Python↔바인딩 | `6f782e4` (`39af990`의 부모) | Inspector에서 EntityRef 필드(예: CameraOrbitControlComponent.target)의 콤보 상자에 "None"만 나온다. 현재 값이 표시되지 않고, 다른 항목을 골라도 반영되지 않는다. 에러 창은 뜨지 않는다. |
| T3 | C++ ECS | `68d5105` (`f6e1a78`의 부모) | 에디터에서 Play 후 Stop하면 Scene Hierarchy의 이름이 전부 "Main Camera" → "Entity_108" 식으로 바뀐다. |
| T4 | C++ ECS·리플렉션 | `5cebee6` (`68d5105`의 부모) | Y축으로 45도 회전시킨 엔티티가 Play → Stop 뒤에(또는 그 엔티티로 만든 프리팹을 인스턴스화하면) 다른 회전이 된다. 크래시나 에러는 없다. |
| T5 | C++ 렌더러 | `e6c2596` (`f436743`의 부모) | Main Camera의 fov를 60에서 25로 줄이면 큐브 그리드의 가운데와 오른쪽이 사라지고 왼쪽 몇 개만 그려진다. GL 에러는 없다. |
| T6 | C++ 플랫폼·Qt | `d7ddc34` (`449f868`의 부모) | Play Mode로 전환하거나 Play 버튼을 누르면 에디터 프로세스가 `Windows fatal exception: access violation`으로 죽는다. |
| T7 | Python↔엔진 | 현재 트리 고정본(§5) | Inspector에서 엔티티의 Rotation을 바꾸고 에디터를 닫으면 저장할지 묻지 않고 닫힌다. 다시 열면 바뀐 회전이 없다. 창 제목에도 `*`가 붙지 않는다. Position을 바꿨을 때는 묻는다. |
| T8 | Python | 현재 트리 고정본 | Sound 이벤트가 있는 저장된 액션을 Motion Editor에서 하위 폴더로 "Save Action As…" 하면, 새 파일은 미리듣기가 무음이고 Build Game이 "액션이 참조하는 클립이 없습니다"로 실패한다. 원본 파일은 정상이다. |
| T9 | Python (대조군) | 현재 트리 고정본 | Build Game이 `스크립트 문법 오류: …\scripts\player_move.py: invalid non-printable character U+FEFF`로 실패한다. 같은 스크립트는 에디터 Play에서는 정상 동작한다. |

T9는 에러 메시지만으로 바로 찾을 수 있는 **대조군**이다. 두 조건 모두 빨리 끝나야 정상이다. 그렇지 않으면 실행 환경이나 지시문에 문제가 있다는 뜻이다.

과제에서 뺀 것: `65b4409`(잡 시스템 교착 — 원인이 여럿이고 분리 확인되지 않아 채점 기준을 세울 수 없음), `1a8317e`(증상에서 출발한 수정이 아님), 프리팹 Phase 4의 entity id 버그(큰 기능 커밋 `0b06406` 안에 섞여 있어 수정 직전 상태를 분리할 수 없음).

## 4. 오염 차단

에이전트가 답을 미리 볼 수 있는 경로를 모두 막는다.

1. **git 이력 없음**: 스냅샷은 `git archive <커밋> engine`으로 풀어 `.git`을 넣지 않는다. 작업 폴더에 `git log --all`이 없으므로 수정 커밋 메시지를 볼 수 없다.
2. **문서 없음**: `engine/`만 넣는다. `CLAUDE.md`, `ROADMAP.md`, `docs/`는 넣지 않는다. 이 안에 버그 기록과 이 계획서·보고서가 있다.
3. **저장소 밖 경로**: 실행 폴더는 `C:\Users\vhale\qf_debug_exp\runs\<run_id>\`. Claude Code의 프로젝트 메모리는 작업 경로 기준이라 `C:\QuarterFlying`의 메모리가 실리지 않는다.
4. **경로 감사**: 실행 기록에서 `C:\QuarterFlying`, `D:\Quarter Flying`, 다른 run 폴더를 읽은 흔적이 있으면 그 실행은 **무효**로 하고 다시 한다.
5. **코드 안 힌트 확인**: 각 스냅샷에서 수정 커밋이 추가한 설명 문구(예: T4의 `EulerRotation`)가 없음을 grep으로 확인한다. 2026-10-16에 T1(로그 문구가 수정 전 코드에 있음 = 증상과 일치), T3(`"Entity_"` 기본 이름 반환 존재), T4(`EulerRotation` 없음), T5(`visibleInstanceData` 없음), T6(`5c951c4`가 `d7ddc34`의 조상 = 두 번째 뷰포트 초기화가 성공하는 상태) 확인했다.
6. **블라인드 채점**: run_id를 무작위로 붙이고, 채점자는 답만 받는다. 답에 도구 이름(PyFlow·브리핑)이 있으면 채점 전에 가린다.

## 5. 실행 환경과 폴더

```text
C:\Users\vhale\qf_debug_exp\
  snapshots\        스냅샷 tar(+ sha256), 현재 트리 고정본
  pyflow\<task>\    스냅샷별 PyFlow 브리핑(조건 P에 복사해 넣음)
  runs\<run_id>\    실행마다 새로 푼 engine\ (+ 조건 P면 _pyflow\)
  prompts\          실행별 지시문(조건만 다름)
  transcripts\      실행 기록
  answers\          최종 답(블라인드)
  grading\          채점표, 집계
```

**현재 트리 고정본(T7~T9)**: 2026-10-16 18:12에 만들었다. 이 버그들이 고쳐지면 작업 트리가 바뀌므로 지금 상태를 보존했다.

| 파일 | 내용 | sha256 |
| --- | --- | --- |
| `T7-T9_current_20261016_engine.tar` | `engine/` 작업 트리(build·캐시 제외, 432개 항목) | `afb39f26381bbb78049442a3616a40632b72611362c500d19645df973bff1c00` |
| `T7-T9_current_20261016.patch` | HEAD `35429a4` 대비 diff | `380a3b0a994562e74cbeff9280caa056aa904956decb8d8ffd31742677d55104` |
| `T7-T9_current_20261016_untracked.txt` | 그때의 untracked 파일 목록(38개) | — |

**PyFlow 브리핑**: 스냅샷마다 GUI 없이 `ProjectHierarchicalParser(<snapshot>/engine).parse()` → `export_briefing` → `export_review`로 만든다(보고서와 같은 방법). 분석 시간은 조건 P의 비용으로 따로 기록한다.

## 6. 지시문

두 조건의 지시문은 아래 한 문단만 다르다. 나머지(증상, 답 형식, 상한, 금지 사항)는 글자까지 같다.

- 공통: "`engine/` 폴더는 게임 엔진과 Python(PySide6) 에디터의 소스다. 아래 증상의 원인을 찾아라. 코드를 고치지 말고, 원인 파일·함수, 원인 설명, 근거 행을 답하라. 빌드하지 말 것. 이 폴더 밖을 읽지 말 것. 답에 쓴 도구 이름을 적지 말 것."
- 조건 P 추가 문단: "`_pyflow/` 폴더에 이 코드의 정적 분석 브리핑이 있다. `_pyflow/README.md`의 읽기 순서대로 시작하라. `analysis.json`(심볼·호출 관계)과 `review.json`(검토 후보)을 조회해도 된다."
- 조건 N에는 PyFlow를 언급하지 않는다.

조건 P에서 "브리핑부터 시작하라"고 정하는 이유: 쓰라고만 하면 에이전트가 무시하고 grep으로 갈 수 있다. 그러면 조건 간 차이가 없어져서 "도구를 썼을 때"를 측정하지 못한다.

## 7. 측정과 채점

| 지표 | 정의 | 얻는 방법 |
| --- | --- | --- |
| 정답 점수 | 2 = 정답 파일·함수 + 원인 메커니즘이 맞음. 1 = 파일·영역은 맞지만 메커니즘이 틀리거나 빠짐. 0 = 틀림 | 블라인드 채점(부록 A 기준) |
| 정답 파일 도달 | 정답 파일 중 하나를 처음 읽은 도구 호출의 순번 | 실행 기록에서 자동 추출 |
| 총 도구 호출 | 답할 때까지의 도구 호출 수 | 실행 기록 |
| 토큰 | 실행 전체 입력+출력 토큰 | 실행 메타데이터 |
| 경과 시간 | 시작~답(분) | 실행 메타데이터 |
| 헛걸음 | 정답과 무관한 파일을 깊게(전체 또는 200행 이상) 읽은 수 | 실행 기록 |
| 준비 비용 | PyFlow 분석 시간(조건 P만) | §5 기록 |

"정답 파일 도달"은 사람의 판단 없이 기록에서 기계적으로 뽑을 수 있어서 주 지표로 쓴다. 채점자가 조건을 알고 "처음 맞는 가설을 세운 시점"을 고르면 편향이 들어간다.

다중 원인 과제(T2, T4, T6)는 부록 A의 **주 원인**을 맞히면 2점이다. 부 원인은 따로 기록만 한다.

## 8. 판정 기준 (사전 등록)

과제별로 두 조건을 비교한 뒤, 아래 규칙을 기계적으로 적용한다.

- **"PyFlow가 더 쉽다"**: 아래 셋을 모두 만족.
    1. 정답 점수 합계가 P ≥ N.
    2. "정답 파일 도달"이 9개 과제 중 6개 이상에서 P가 더 작다(같으면 무승부, 어느 쪽에도 세지 않음).
    3. 총 토큰 중앙값이 P가 N보다 20% 이상 적다. 조건 P의 토큰에는 브리핑을 읽는 비용이 포함된다.
- **"PyFlow가 더 어렵다"**: 위 셋을 N 기준으로 뒤집어 모두 만족.
- 그 밖: **"비슷하다"**.

결과는 영역별(Python: T1·T8·T9 / 경계: T2·T7 / C++: T3~T6)로도 나눠 보고한다. 단, 판정은 전체 기준 하나로만 한다. 영역별 수치는 설명용이고, 결과를 본 뒤 유리한 영역을 골라 결론을 내지 않는다.

1차(1회 반복)에서 조건 2가 5 또는 6 대 3 근처로 경계선이면 2회를 더 해서 과제별 중앙값으로 다시 판정한다.

## 9. 사전 점검 결과와 관문

2026-10-16 파일럿: T5 스냅샷(`e6c2596`)의 `engine/` 전체에 PyFlow를 돌렸다.

- 35.3초, 파일 269개, 검토 후보 6,646개.
- **분석 실패 42개 + 부분 분석 11개.** 실패 41건이 `C/C++ 해석기가 종료되었습니다 (3221225477)`(access violation)이다. `tree_sitter_c has no attribute 'language'` 오류도 1건 나왔다.
- 실패한 파일에 **C++ 과제의 정답 파일이 거의 다 들어 있다**:

| 과제 | 정답 파일 | PyFlow 분석 |
| --- | --- | --- |
| T2 | `editor/panels/inspector.py`, `bindings/ECSBindings.cpp` | inspector.py 성공 · ECSBindings.cpp **실패** |
| T3 | `ecs/Reflection.cpp`, `ecs/ECSRegistry.cpp` | Reflection.cpp **실패** · ECSRegistry.cpp 성공(ECSRegistry.h는 **실패**) |
| T4 | `ecs/Reflection.cpp` | **실패** |
| T5 | `renderer/InstancedBatchManager.cpp` | **실패**(.h도 실패) |
| T6 | `platform/Win32Platform.cpp`, `core/Engine.cpp` | 둘 다 **실패** |
| T1·T7·T8·T9 | `editor/` 아래 Python 파일 | 모두 성공 |

이 표는 T5 스냅샷 한 개에서 잰 것이다. 다른 스냅샷의 상태는 브리핑을 만들 때 다시 확인한다(§13).

즉 지금의 PyFlow로 실험하면, C++ 과제에서 조건 P는 **정답이 있는 바로 그 파일에 대한 정보가 빠진 브리핑**을 받는다. 그래서 두 갈래로 나눈다.

- **갈래 A — 현재 PyFlow(권장 순서 1)**: 지금 상태 그대로 실행한다. "지금 이 도구를 쓰면 디버깅이 쉬운가"에 바로 답한다. C++ 과제의 결과는 H2의 검증이 된다.
- **갈래 B — 크래시 수정 후(선택)**: PyFlow의 C/C++ 해석기 크래시를 고치고, 정답 파일이 모두 분석되는지 확인한 뒤 같은 실험을 다시 한다. "도구가 제대로 동작하면 쉬운가"를 잰다.

갈래 B의 진입 조건: 9개 스냅샷 모두에서 부록 A의 정답 파일이 PyFlow `분석 완료`(또는 `분석 완료 (구문 기반)`)일 것.

갈래 A를 먼저 하는 이유: 사용자가 물은 것은 "이것을 쓰는 것이 더 쉬운가"이고, 지금 쓸 수 있는 것은 현재 상태의 도구다. 갈래 B는 PyFlow를 고칠 가치가 있는지 판단하는 근거가 된다.

## 10. 위협과 한계

- **피험자가 사람이 아니다**: 에이전트는 grep과 파일 읽기가 빠르다. 처음 보는 코드 앞의 사람에게는 구조 그림이 더 도움이 될 수 있다. 사람 조건은 넣지 않는다. 이 저장소의 사용자는 이미 이 버그들의 답을 알기 때문이다(오염).
- **실행 없이 추정**: 빌드를 막았으므로 "재현해서 확인"하는 디버깅 단계는 측정하지 않는다. 그 단계는 PyFlow 사용 여부와 무관하다고 보고 뺐다.
- **과제 수 9개**: 큰 차이만 드러난다. 작은 차이는 "비슷하다"로 나온다.
- **모델 지식**: 에이전트가 일반 지식으로 흔한 버그 유형(쿼터니언 w 유실 등)을 바로 의심할 수 있다. 두 조건에 똑같이 작용하므로 비교는 유효하다.
- **같은 영역 과제**: T1과 T8은 같은 소리 경로를 다룬다. 실행마다 새 에이전트라서 학습 효과는 없다.
- **지시 효과**: 조건 P에 "브리핑부터 시작하라"고 지시하므로, 브리핑이 쓸모없는 과제에서는 그만큼 비용이 든다. 이것은 "도구를 쓰는 비용"으로 보고 측정에 포함한다.

## 11. 비용 추정

- 갈래 A 1차: 18회 실행 × 최대 60 도구 호출. 실행 대부분은 60회보다 훨씬 일찍 끝날 것으로 본다(근거: 2026-10-16 점검에서 버그 하나의 원인 확인에 10~20회).
- PyFlow 준비: 스냅샷 7개(T7~T9는 하나를 공유) × 약 35초.
- 채점: 답 18개 × 부록 A 대조.

## 12. 저장소 영향 (CLAUDE.md 관례 4)

1. **바뀌는 곳**: 없음. 실험은 저장소 코드를 바꾸지 않는다. 산출물은 모두 `C:\Users\vhale\qf_debug_exp\`에 둔다. 저장소에는 이 계획서와 결과 보고서만 추가한다.
2. **영향받는 곳**: 없음. 스냅샷은 `git archive`로 읽기만 한다.
3. **호환성**: 해당 없음.
4. **고장 시 갈리는 지점**: T9(대조군)가 두 조건 모두 빨리 끝나지 않으면 실행 환경 문제다. PyFlow 브리핑이 비어 있거나 분석 실패가 파일럿(42개)보다 크게 늘면 PyFlow 실행 문제다.
5. **회귀 확인**: 해당 없음.

## 13. 준비 체크리스트

- [x] 과제 후보 선정, 수정 커밋별 증상·원인 확인 (2026-10-16)
- [x] 증상 문구를 수정 직전 코드와 대조, 코드 안 힌트 확인 (§4-5)
- [x] 현재 트리 고정본 생성 + sha256 (§5)
- [x] PyFlow 파일럿: 엔진 전체 분석 시간·실패 파일 측정 (§9)
- [x] 과거 스냅샷 6개 추출 + sha256 기록(`snapshots/SHA256SUMS.txt`). 스냅샷 안의 `.md` 문서에 정답 단서가 없음을 grep으로 확인
- [x] 스냅샷 7개의 PyFlow 브리핑 생성(각 37.6~48.1초). 정답 파일 분석 상태는 파일럿(§9 표)과 같다: T2~T6의 주 정답 파일 분석 실패, Python 과제는 모두 성공
- [x] 지시문 생성기(`tools/run_trial.py`), run_id 무작위 배정표(`grading/assignment_key.json`, 채점자에게는 비공개)
- [x] T9 대조군으로 파일럿 2회: 둘 다 2점, N 6회·P 7회 호출로 끝남
- [x] 갈래 A 1차 실행 18회 (2026-10-16, 모두 정상 종료)
- [x] 경로 감사 → 블라인드 채점 → §8 판정 → 결과 보고서: [PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_2026-10-16.md](PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_2026-10-16.md) — 판정 "PyFlow가 더 어렵다"
- [x] 결과 검토 반영(2026-10-21): 판정 범위 한정, 부호 검정, 천장 효과, 토큰 턴별 분해 — 결과 보고서 "검토 반영" 절
- [x] 갈래 B(2026-10-22, 반복 1회로 축소): [결과](PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_B_2026-10-22.md) — 최종 판정(B2 vs N) "비슷하다", 제공 방식 효과 유의(B2 vs B1 13:2), 크래시 수정 효과 유의하지 않음

## 14. 실행 기록과 계획 대비 변경

실행 도구는 모두 `C:\Users\vhale\qf_debug_exp\tools\`에 있다: `run_trial.py`(1회 실행), `run_batch.py`(배치), `analyze.py`(지표), `grade.py`(블라인드 채점), `summarize.py`(§8 판정).

**격리 방식(§4를 구체화)**: 서브에이전트 대신 Claude Code CLI(`claude.exe -p`, VS Code 확장 번들 2.1.301)를 실행 폴더에서 따로 띄운다. 이 세션에서 띄우는 서브에이전트는 저장소 CLAUDE.md를 받을 수 있다. 그런데 CLAUDE.md "알려진 버그" 절에 T6의 원인이 적혀 있다. 옵션:

- `--setting-sources project`: 사용자 `settings.json`을 싣지 않는다. 그 파일은 `Read(//c/QuarterFlying/**)`를 허용하고 `C:\QuarterFlying`을 추가 디렉터리로 둔다.
- `--strict-mcp-config`(MCP 없음), `--no-session-persistence`, `--permission-mode dontAsk`.
- 허용: `Bash(python:*)`. Read·Grep·Glob은 작업 폴더 안에서만 기본 허용된다.
- 거절: 웹, 에이전트, 편집 도구 등.
- 모델 `claude-opus-5-5`, effort `high`.

격리 시험(`sandbox01`) 결과: 폴더 안 읽기와 Python은 성공했다. `C:\QuarterFlying\CLAUDE.md` 읽기, 바깥 grep, `ls ..`는 거절됐다. CLAUDE.md·메모리는 로드되지 않았다.

**파일럿 뒤 바꾼 것**(두 조건에 똑같이 적용):

- 지시문에 "Python은 `cd` 없이 이 폴더에서 바로 실행하라"를 추가했다. 파일럿에서 두 조건 모두 `cd /tmp && python …`이 거절돼 호출 1회를 버렸기 때문이다.
- 경로 감사 정규식이 자기 실행 폴더를 바깥으로 잘못 세던 것을 고쳤다.

**경과 시간**: 3개씩 동시에 실행하므로 서로 영향을 받는다. 판정에 쓰는 지표(정답 파일 도달·점수·토큰)는 영향받지 않는다.

## 15. 갈래 B 설계 초안 (2026-10-21, 실행 전 확정 필요)

갈래 A 1차 결과와 그 검토([결과 보고서](PYFLOW_DEBUGGING_EXPERIMENT_RESULTS_2026-10-16.md) "검토 반영")를 바탕으로 한다. A는 두 원인을 섞어서 쟀다. (1) 정답 파일 분석 실패, (2) 브리핑을 통째로 먼저 읽히는 제공 방식. B는 이 둘을 따로 잰다. 아래 숫자는 **첫 실행 전에 고정**하고, 결과를 본 뒤 바꾸지 않는다.

### 조건

| 조건 | 분석기 | 제공 방식 | 지시문 |
| --- | --- | --- | --- |
| N | 없음 | — | A와 같음 |
| A | 현재 PyFlow(크래시 포함) | 브리핑 선읽기 | A의 P와 같음. **같은 배치에서 다시 실행**한다(날짜·모델 버전 차이를 없애기 위해 A1 결과를 재사용하지 않는다) |
| B1 | C/C++ 크래시 수정판 | 브리핑 선읽기 | A와 같음 |
| B2 | B1과 같은 분석 결과 | 증상 기반 질의 도구 `_pyflow/q.py` | "필요하면 `python _pyflow/q.py …`를 써도 된다" + 사용법 4줄. 브리핑을 읽으라고 하지 않는다 |

**B2 질의 도구** — B1과 **같은 `analysis.json`만** 쓴다. 새 분석을 넣으면 "제공 방식"과 "정보량"이 섞인다.

- `find "<문자열>"`: 그 문자열을 포함한 함수 정의를 반환한다(파일:행, 함수 이름). 최대 10개.
- `callers <함수>` / `callees <함수>`: 파일:행과 연결 종류(확정 또는 추정)를 반환한다. 최대 15개.
- `show <파일>:<행> [n]`: n줄 발췌를 보여 준다(기본 30, 최대 80).
- INDEX 같은 전체 요약은 내보내지 않는다. 결과가 상한을 넘으면 "N개 더 있음"만 표시한다.

### 비교와 해석

| 비교 | 재는 것 | 해석 규칙 |
| --- | --- | --- |
| B1 vs A | 분석 실패 제거 효과(제공 방식 같음) | 유의하면 "크래시 수정이 효과 있음" |
| B2 vs B1 | 제공 방식 효과(정보 같음) | 유의하면 "질의 방식이 효과 있음" |
| B2 vs N, B1 vs N | 실용 효과 | 아래 최종 판정에 쓴다 |

검토 제안의 "B1이 N보다 나으면 원인은 분석 실패 제거"는 쓰지 않는다. B1과 N은 분석기와 브리핑 유무가 함께 다르기 때문이다. 크래시 수정 효과는 제공 방식이 같은 **B1 vs A**로만 가른다.

### 과제

- **T1~T9** 그대로(스냅샷·증상 문구 동일).
- **천장 효과 완화**: A1에서 9개 중 8개를 두 조건 모두 맞혀 정확도가 변별력을 잃었다. 에러 문자열 없이 동작만 이상한 과제를 6개 추가한다(T10~T15).
    - 원인이 Python↔C++ 경계나 C++ 내부에 있고, 증상 문구로 grep해도 원인 파일에 바로 닿지 않는 것.
    - 실제 버그가 모자라면 현재 트리 고정본에 결함을 심는다. 심은 위치·정답은 부록 B에 적고, 실행 폴더에는 넣지 않는다. 심은 결함은 기존 테스트가 잡지 못하는 것이어야 한다.
- 총 15개 과제. 부호 검정 양측 p < 0.05가 되려면, 같음을 뺀 15개 기준 12:3 이상이 필요하다.

### 반복과 비용

- 과제 × 조건마다 **k = 3회** 실행한다. 과제별 대표값은 3회의 중앙값이다.
- 총 실행 수: 15 과제 × 4 조건 × 3회 = 180회.
- 비용 추정: A1 평균 $0.51/회 × 180 ≈ $90. 어려운 과제(T6 수준, $1.5~2.1/회)가 늘면 $120~150.
- 실행 순서는 조건·과제를 섞어 무작위로 한다. 동시 실행은 3개.

### 측정

A1 지표(§7)에 다음을 더한다.

| 지표 | 정의 |
| --- | --- |
| 정답 점수 0~4 | 위치 1 + 메커니즘 1 + 부 원인 1 + 근거 행이 실제로 그 원인을 보여 줌 1. A1의 0~2 기준은 함께 기록한다(비교용) |
| 토큰 분해 | API 호출마다 새 입력 / 캐시 생성 / 캐시 읽기 / 출력(최종 `result`의 값). 턴 수 |
| 과금 | 실행별 `total_cost_usd`. 토큰 총량과 별도로 판정에 쓴다 |
| 질의 사용률(B2) | `q.py`를 1회 이상 쓴 실행의 비율 |
| 질의 → 열람 전환율(B2) | `q.py` 결과에 나온 파일:행을 다음 3회 호출 안에 열람한 비율 |
| 상세 분석 열람(A·B1) | `analysis.json`·`review.json`을 연 실행 수 |

### 판정 규칙

비교마다 과제별 대표값으로 짝 비교한다.

- **주 지표**: 정답 파일 도달. 15개 과제의 부호 검정(같음 제외), 양측 α = 0.05.
- **최종 판정 "PyFlow(B2 방식)가 디버깅을 더 쉽게 한다"**: B2 vs N에서 다음 셋을 모두 만족.
    1. 주 지표 부호 검정 p < 0.05, B2 쪽.
    2. 정답 점수(0~4) 합계 B2 ≥ N.
    3. 과제별 과금 중앙값 B2 ≤ 1.2 × N.
- **"더 어렵다"**: N 쪽으로 같은 조건을 만족.
- 그 밖: **"비슷하다"**. B1 vs N도 같은 규칙으로 판정해서 함께 보고한다.
- **무효 조건**: B2 실행 중 질의 사용률이 50% 미만이면, B2는 "도구가 선택되지 않음"으로 보고하고 제공 방식 효과는 판정하지 않는다.

### 진입 조건과 준비

1. [x] PyFlow C/C++ 해석기 크래시(0xC0000005, 스냅샷당 39~52개 파일)를 고친다(2026-10-21).
    - 원인: tree-sitter 0.26.0. 0.25.2로 고정하고, 실행 환경을 새 폴더 `C:\Users\vhale\pyflow_runtime\py314-ts0252`로 옮겼다. 이전 실행 환경 폴더는 하위 패키지를 읽을 수 없어서, C 문법은 로드조차 안 되고 있었다.
    - 상세 기록은 PyFlow README의 "C/C++ 해석기 크래시 수정과 질의 도구 (2026-10-21)" 절.
2. [x] 정답 파일 분석 상태(2026-10-21, `qf_debug_exp\pyflow_b1\`): 7개 스냅샷 모두 분석 실패 0개(이전 39~52개).
    - 정답 파일 중 `Reflection.cpp`(T3·T4), `Win32Platform.cpp`(T6), `ECSBindings.cpp`(T2)는 `부분 분석`이다. 원인은 파일 안 다른 곳의 매크로 구문 오류다.
    - 정답 함수는 모두 심볼로 추출됐다: `SerializeRegistry`, `DeserializeRegistry`, `RegisterPODComponentsReflection`(T4의 `GE_FIELD` 등록), `PollEvents`, `UpdateVisibility`, `RenderBatch`, `RegisterRegistryBindings`.
    - **기준 변경**: 진입 조건을 "정답 파일이 `분석 완료`"에서 **"정답 함수가 심볼로 추출됨"**으로 바꾼다. 이 절이 아직 확정 전이라 바꿀 수 있다. 바꾼 이유는 위와 같다.
3. [x] 질의 도구를 PyFlow 기능으로 만들었다: `src/briefing_query.py` → 브리핑 폴더의 `q.py`.
    - 실제 증상으로 확인했다: T9·T8 문구는 원인 함수 1개로, T1 문구는 `SoundPlayer.play`로 바로 이어진다. `callers`는 T1의 정답 경로 두 곳을 테스트보다 앞에 보여 준다.
    - B1용 브리핑은 `export_briefing(..., include_query_tool=False)`로 만든다. 그래야 B1 README에 질의 안내가 실리지 않아 B2와 섞이지 않는다.
4. [x] T10~T15 과제 확정, 부록 B 작성(2026-10-21). 결함마다 증상 확인 + 기존 테스트(C++ 643, 에디터 14개 파일) 미검출 확인.
5. [x] **확정(2026-10-21, 첫 실행 전).** 위 숫자는 그대로 둔다: 과제 15개 × 조건 4개 × 3회 = 180회, 부호 검정 양측 α = 0.05, 과금 비율 상한 1.2, B2 무효 기준 질의 사용률 50%. 판정 코드는 `tools/summarize_B.py`에 고정했다. 구현하면서 정한 세부:
    - **조건별 `_pyflow/` 내용**
        - A: 수정 전 PyFlow(원본 zip에서 푼 `qf_debug_exp\pyflow_orig`)를 갈래 A와 같은 실행 환경으로 돌린 브리핑. T1~T9는 갈래 A 브리핑을 그대로 쓰고, T11~T15는 같은 방법으로 새로 만들었다.
        - B1: 수정본 PyFlow 브리핑, 질의 도구 없음. A와 같은 7개 파일이다.
        - B2: `q.py`와 `analysis.json` 2개만 넣는다. INDEX 등을 넣지 않아야 "제공 방식"만 다르다.
        - 모든 조건에서 `analysis.json`의 `project`를 실행 폴더의 `engine/`으로 바꾼다. 그래야 `q.py`가 폴더 밖 원문을 읽지 않는다.
    - **지시문**: A·B1은 갈래 A의 P와 같은 문단이고, B2는 `q.py` 사용법 4줄이다(`tools/run_trial.py` `CONDITION_B2`). N은 PyFlow를 언급하지 않는다.
    - **CLI 버전 고정**: 실행 중에 VS Code 확장이 2.1.301 → 2.1.304로 업데이트돼 번들 경로가 사라졌다. 갈래 B는 복사본 `tools\claude-2.1.304.exe`(sha256 앞 16자 `019e5174d5d95458`)만 쓴다. A를 같은 배치에서 다시 돌리므로 버전 차이는 비교에 들어가지 않는다.
    - **채점**: 0~2점(갈래 A와 같은 기준)과 0~4점(loc·mech·secondary·evidence)을 함께 낸다. 근거 행을 확인하도록 채점 폴더에 과제별 정답 파일 원문(`code/<과제>/engine/…`)을 넣는다. 조건 정보는 넣지 않는다.
    - **파일럿**: T9 × 조건 4개, 본실험과 별개(`pilotB_T9_*`). 실행 순서는 `grading/run_order_B.txt`(무작위)이고, 동시 실행은 3개다.
    - **파일럿 결과(2026-10-21)**: 4개 모두 정상 종료, 경로 감사 0건.

        | 조건 | 도구 호출 | 정답 파일 도달 | 토큰 | 과금 |
        | --- | --- | --- | --- | --- |
        | N | 5 | 2 | 69k | $0.16 |
        | A | 8 | 4 | 191k | $0.33 |
        | B1 | 7 | 4 | 172k | $0.34 |
        | B2 | 7 | 2 | 86k | $0.17 |

        - B2는 첫 호출로 `q.py find "스크립트 문법 오류"`를 썼다. 결과가 원인 함수 하나였고, 다음 호출에서 그 파일을 열었다(질의 → 열람 전환 1/1).
        - 파일럿 중 지표 코드의 전환율 계산 버그(역슬래시 경로 비교)를 고쳤다. 판정 규칙은 바꾸지 않았다.

### 변경 기록 (2026-10-21, 결과를 보기 전)

- **무슨 일이 있었나**: 갈래 B 본실험 180회 중 164회가 API 사용량 한도(429 `usage_limit_reached`, "You've hit your session limit")로 시작하자마자(약 2초) 끝났다. 정상 종료는 16회다. 한도는 이 계정의 일반 작업과 공유된다.
    - 실패 164회는 실행이 아니라 빈 기록이라 `qf_debug_exp\failed_usage_limit\`로 옮겼고, 데이터로 쓰지 않는다. 이 중 2회는 실행 도중에 한도에 걸렸다.
    - 배치 실행기(`tools/run_batch.py`)는 이제 한도에 한 번 걸리면 남은 실행을 시작하지 않는다.
- **변경**: 반복을 **k = 3 → k = 1**로 줄였다. 총 60회다(15 과제 × 4 조건). 사용자가 결정했다.
- **어느 실행을 쓰나**: 미리 무작위로 배정한 `rep = 1` 실행만 쓴다(`assignment_key.json`의 `rep`). 결과에 따라 고르지 않는다.
    - 이미 끝난 16회 중 rep 1은 4회이고, 판정에 쓴다.
    - rep 2·3인 12회는 결과와 무관하게 판정에서 뺀다. 열어 보지 않았고, 채점도 하지 않는다.
    - 남은 rep 1 실행 56회는 `grading/run_order_B_rep1.txt`(원래 무작위 순서 유지)로 돌린다.
- **바뀌지 않는 것**:
    - 과제 15개, 조건 4개, 지표, 판정 규칙. 최종 판정은 부호 검정 양측 p < 0.05이고, 같음을 뺀 15개 기준 12:3 이상이 필요하다.
    - 과금 비율 상한 1.2, B2 무효 기준 50%.
    - 판정 코드는 `summarize_B.py`에 `REPS = (1,)` 한 줄만 더했다.
- **영향**:
    - 과제별 대표값이 3회 중앙값이 아니라 1회 값이라 잡음이 커진다. 실행 하나가 우연히 헤매면 그 과제의 승패가 바뀐다(갈래 A의 T6이 그런 예였다).
    - 그 결과, 12:3에 못 미쳐 "비슷하다"로 나올 가능성이 높아진다. 결과는 이 점을 감안해 읽는다.
    - 비용 추정은 60회 × $0.5~0.8 ≈ $30~50.

## 부록 A. 정답 (채점용 — 에이전트에게 주지 않는다)

| ID | 주 원인 | 정답 파일·함수 | 부 원인 |
| --- | --- | --- | --- |
| T1 | 프리로드는 액션 파일 기준 절대 경로로 등록하는데, 재생은 `params.clip`의 상대 경로 원문으로 찾는다 | `editor/core/scene_action_playback.py`(재생 경로), `editor/core/sound_player.py`(preload·play 키) | Motion Editor 미리듣기(`editor/panels/animation_preview.py`)도 같은 불일치. `save_action()`이 메모리의 clip을 상대화해서 저장·불러온 액션만 증상이 난다 |
| T2 | `GetEntityName(e.id)`에 raw int를 넘겨 pybind11 "incompatible function arguments" 예외가 나고 bare except가 삼킨다 → 목록이 None만 남음 | `editor/panels/inspector.py` `DynamicComponentWidget.__init__`의 EntityRef(ftype 5) 분기 | 저장값(UUID)과 콤보 데이터(runtime id)가 달라 표시 실패. 변경 시 runtime id를 보내지만 엔진 `patchField`는 UUID로 해석 |
| T3 | PIE 스냅샷(`SerializeRegistry`)이 UUID와 컴포넌트만 저장하고 이름을 저장하지 않는다. 복원은 새 runtime id로 다시 만들어 이름 매핑이 사라지고 기본 이름 `"Entity_<id>"`가 나온다 | `ecs/Reflection.cpp` `SerializeRegistry`/`DeserializeRegistry` | `ecs/ECSRegistry.cpp` `GetEntityName`의 기본 이름 |
| T4 | `TransformComponent::rotation`(Quaternion 16B)이 리플렉션에 Vec3(12B)로 등록돼 직렬화 때 w가 빠지고 역직렬화 때 w가 기본값 1로 남는다 | `ecs/Reflection.cpp`의 TransformComponent 등록(필드 타입) | `ECSRegistry::SetTransformRotation`이 오일러 값을 쿼터니언 x,y,z에 넣고 w=1. `RotateEntityCommand`가 w를 버린다 |
| T5 | `UpdateVisibility`는 보이는 개수만 세고 GPU 버퍼에는 전체 인스턴스를 원래 순서로 올린다. `drawInstanced(visibleCount)`가 버퍼 앞쪽 N개를 그린다 | `renderer/InstancedBatchManager.cpp` `UpdateVisibility`·`RenderBatch` | — |
| T6 | 임베드 모드에서도 `Win32Platform::PollEvents()`가 `PeekMessage(nullptr)`로 Qt 스레드의 메시지 큐 전체를 펌프한다 → 엔진 프레임 도중 Qt 핸들러가 실행되어 `TickFrame`이 재진입하거나 다른 Engine이 GL 컨텍스트를 바꾼다 | `platform/Win32Platform.cpp` `PollEvents` | 각 Engine이 매 프레임 자기 GL 컨텍스트를 current로 만들지 않는다(`core/Engine.cpp` `TickFrame`) |
| T7 | Inspector가 position·parent 외 필드를 `registry.SetComponentFieldJson`으로 직접 써서 명령을 거치지 않는다. "저장 안 된 변경"은 `CommandManager` 변경 카운터로만 판단한다 | `editor/panels/inspector.py` `DynamicComponentWidget._on_field_change` + `editor/project_actions.py` `_is_scene_dirty`/`_scene_change_count` | 같은 원인으로 Undo도 안 된다 |
| T8 | `relativize_clip_paths()`가 절대 경로만 상대화하고 이미 상대인 clip은 건너뛴다. 옛 폴더 기준 상대 경로가 새 파일에 그대로 쓰이고 `source_path`만 새 폴더로 바뀐다 | `editor/core/action_data.py` `relativize_clip_paths`·`save_action` | — |
| T9 | 의존성 수집이 스크립트를 `read_text(encoding="utf-8")`로 읽어 BOM(U+FEFF)이 남은 채 `ast.parse`에 넘긴다 | `editor/gamebuild/game_builder.py` `_collect_script_imports` | `_collect_material_files`도 같은 방식(미재현) |

T1~T6의 정답은 각 수정 커밋(`2c9c929`, `39af990`, `f6e1a78`, `68d5105`, `f436743`, `449f868`)의 메시지와 diff에서 가져왔다. T7~T9는 [2026-10-16 점검 보고서](PYFLOW_COMPARISON_BUG_REPORT_2026-10-16.md)의 재현 결과다.

## 부록 B. 갈래 B 추가 과제 T10~T15 (2026-10-21) — 에이전트에게 주지 않는다

천장 효과를 줄이기 위한 과제다(§15 "과제"). 모두 에러 문자열이 없고 동작만 이상하다. T10은 고정본에 실제로 있는 버그다(2026-10-16 점검 보고서 #2). T11~T15는 고정본(`T7-T9_current_20261016_engine.tar`)에 결함 **하나**를 심은 스냅샷이다. 원인 위치는 §15 기준을 따랐다(Python↔C++ 경계 또는 C++ 내부). T10만 원인이 Python이다. 실제 버그라서 예외로 넣었다.

### 정답

| ID | 종류 | 주 원인 | 정답 파일·함수 | 부 원인·경로 |
| --- | --- | --- | --- | --- |
| T10 | 실제 | Play 중 Inspector를 읽기 전용으로 만드는 `InspectorPanel.set_play_mode(True)`를 부르는 곳이 없다. Stop의 `set_play_mode(False)`만 있다 | `editor/main.py` `_on_play` (+ `editor/panels/inspector.py` `set_play_mode`) | 계층 패널의 버튼·메뉴도 Play 중 꺼지지 않는다 |
| T11 | 심음 | `ComputeWorldTransform`이 조상 회전을 `rotation * parent` 순서로 누적한다(맞는 순서는 `parent * rotation`). 축이 다른 회전이 겹칠 때만 틀린다. 카메라 시선은 이 함수를, 메시는 `ComputeWorldMatrix`(올바름)를 쓴다 | `ecs/Hierarchy.cpp` `ComputeWorldTransform` | 사용처 `ecs/CameraSystem.cpp`, `CameraGizmoSystem` |
| T12 | 심음 | 리플렉션 필드 패치(`patchField`)의 EulerRotation 분기가 `{val[0], val[2], val[1]}`로 Y·Z를 바꾼다. 역직렬화는 올바르다 | `ecs/Reflection.h` `GE_BEGIN_COMPONENT`의 `patchField` | 경로: `inspector.py _on_field_change` → `ECSBindings.cpp SetComponentFieldJson` → `patchField` |
| T13 | 심음 | Qt 마우스 버튼 → 엔진 `MouseButton` 변환에서 오른쪽·가운데가 바뀐다. 게임 입력만 이 변환을 거친다. 에디터 카메라는 Qt 이벤트를 직접 보고, 빌드한 게임은 `Win32Platform`이 변환한다 | `editor/viewport.py` `EngineViewport._map_mouse_button` | — |
| T14 | 심음 | `World::Play`가 계속(Pause → Play)할 때도 스냅샷을 다시 찍는다. Stop은 계속한 순간의 상태로 되돌린다 | `ecs/World.cpp` `World::Play` | 에디터 경로 `main.py _resume` |
| T15 | 심음 | `SetComponentJson` 바인딩이 `deserialize` 전에 컴포넌트를 지운다(`info->remove`). 일부 필드만 보내는 Camera 이벤트(`{isMainCamera, priority}`)의 나머지 필드가 기본값(fov 45, blendInSeconds 0)으로 돌아간다 | `bindings/ECSBindings.cpp` `SetComponentJson` | 부분 JSON 호출부 `editor/core/scene_action_playback.py _activate_camera` |

심은 결함의 정확한 원문·대체문은 `C:\Users\vhale\qf_debug_exp\seed\mutations.py`에 있다. 스냅샷과 sha256은 `snapshots\T1x_seeded_engine.tar`와 `SHA256SUMS.txt`에 있다. 각 스냅샷은 고정본과 **파일 1개만** 다르다(`diff -rq`로 확인).

### 검증 (2026-10-21)

결함마다 두 가지를 확인했다. (1) 기존 테스트가 잡지 못한다, (2) 증상이 실제로 나타난다.

- **증상 확인 방법**
    - T11·T12·T14: 검증용 gtest(`seed\SeedVerifyTests.cpp`)를 썼다. 원본에서는 통과하고 결함을 심으면 실패한다. 이 테스트는 별도 빌드 복사본에만 넣었고, 실험 대상 스냅샷에는 없다(`SeedVerify` 항목 0개).
    - T15: 별도 빌드한 실제 엔진 모듈로 `ScenePlaybackController._activate_camera`를 실행했다.
    - T13: `_map_mouse_button`을 함수 단위로 호출했다.
- **기존 테스트**
    - C++ `SimpleEngineTests` 643개: 결함마다 모두 통과(검증 테스트 3개 제외).
    - 에디터 Python 테스트 14개 파일: T13은 원본과 결과가 같다. C++ 결함(T11·T12·T14·T15)은 결함을 넣어 다시 빌드한 엔진 모듈로 돌려 원본과 비교했다(아래 표).
    - 비교에서 `test_sound_player.py`는 뺐다. 원본에서도 3회 중 2회 실패한다("같은 클립 재트리거" 항목). 결함과 무관하게 불안정한 테스트다.

| ID | 측정한 증상 | 원본 | 결함 | 기존 테스트 |
| --- | --- | --- | --- | --- |
| T10 | Play 중 position 입력칸 / ＋Component / `_play_mode` | — | `enabled=True` / `True` / `False`(실제 에디터, 2026-10-16) | 해당 없음(실제 버그) |
| T11 | 부모 Y90 + 자식 X−30 카메라의 시선 | (−0.866, −0.500, 0) = 렌더 행렬과 같음 | (−1, 0, 0), 위 벡터 (0, 0.866, −0.5) — 수평을 보며 기욺 | C++ 643 통과, 에디터 테스트 원본과 동일 |
| T12 | `patchField` Rotation [0, 30, 0] 뒤의 오일러 각 | (0, 30, 0) | (0, 0, 30) | C++ 643 통과, 에디터 테스트 원본과 동일 |
| T13 | Qt Right / Middle → 엔진 버튼 | Right / Middle | Middle / Right | 에디터 테스트 원본과 동일 |
| T14 | Play → x=5 → Pause → Play → x=9 → Stop 뒤의 x | 0 | 5 | C++ 643 통과, 에디터 테스트 원본과 동일 |
| T15 | Camera 이벤트 뒤 Cam B의 fov / blendInSeconds | 30 / 1.5 | 45 / 0 | C++ 643 통과(바인딩은 C++ 테스트 범위 밖), 에디터 테스트 원본과 동일 |

검증 단계(CLAUDE.md 관례 2): T10은 실제 실행이다. T11·T12·T14는 단위 테스트, T15는 실제 엔진 모듈 단위 실행, T13은 함수 단위다. 화면으로 직접 본 것은 없다. 증상 문구의 화면 묘사는 측정값에서 유도했다. 예를 들어 T11의 "수평을 보며 옆으로 기울어진다"는 시선 y 성분이 0이고 위 벡터가 기운 값에서 나왔다.

**빌드 환경**: `C:\Users\vhale\qf_debug_exp\seed\build`. 원본 저장소 빌드 폴더와 작업 트리는 건드리지 않았다. 의존성은 `engine\build\deps\*-src`를 오프라인으로 썼다(`FETCHCONTENT_FULLY_DISCONNECTED`).
