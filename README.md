# Quarter Flying Game Engine

C++23 기반 게임 엔진 + PySide6 에디터. **현재 Windows 전용**이다(Win32 + OpenGL). 다른 OS는 플랫폼
구현이 없어서 `PlatformFactory`가 `nullptr`를 돌려준다. 다른 OS로 포팅할 때 이 문서도 함께 고친다.

## 시작 방법 (Windows)

### 요구사항

- CMake 3.15+ (4.x에서 확인)
- Visual Studio 2022 Build Tools (MSVC, C++23)
- Python 3.14 + PySide6 (`pip install PySide6`)

### 빌드

```bash
cd engine
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Release로 빌드한다. Debug는 `python3XX_d.lib`가 보통 없어서 링크 단계에서 실패한다.
산출물: `engine/build/Release/quarterflying.cp314-win_amd64.pyd`(에디터가 쓰는 엔진 모듈),
`engine/build/tests/Release/SimpleEngineTests.exe`(C++ 테스트).

### 에디터 실행

```bash
cd engine/editor
set PYTHONPATH=..\build\Release
python main.py
```

`PYTHONPATH`에 엔진 모듈이 없으면 에디터는 **엔진 없이 더미 모드**로 뜬다(뷰포트에 안내 문구만 보임).

## 주요 기능

- **ECS**: dense 배열 기반 컴포넌트 저장, 리플렉션으로 Inspector 폼 자동 생성, PIE(Play-In-Editor) 스냅샷
- **Job System**: work-stealing 스케줄러, 의존성 해석
- **렌더링**: OpenGL, 인스턴싱 배치 + frustum 컬링, `scene.json`의 OBJ 메시 로드(`RenderableComponent.meshPath`), VFX 파티클
- **카메라**: 에디터 카메라(궤도/이동/줌), 활성 카메라 priority 규칙, 전환 블렌드, 추적/주시/플레이어 궤도 카메라, 카메라 기즈모
- **에디터 (PySide6)**: Scene Editor / Play Mode / Motion Editor, Undo/Redo, 프리팹(인스턴스화/Revert), 액션 이벤트(사운드, 카메라 전환)
- **테스트**: C++ `SimpleEngineTests`(gtest), 에디터 `engine/editor/test_*.py`(직접 실행)

## 에디터 사용법

### Scene Editor

- 시작하면 `engine/assets/scene.json`과 검증용 기본 엔티티가 자동으로 만들어진다.
  File 메뉴의 New/Open/Save는 **아직 동작에 연결되지 않았다**.
- 씬 계층에서 엔티티를 선택하고 Inspector에서 컴포넌트 값을 편집한다(Undo/Redo 지원).
- 뷰포트 카메라: **우클릭 드래그**(또는 Alt+좌클릭) 회전, **휠클릭 드래그**(또는 Shift+우클릭) 이동,
  **휠** 줌, **F** 리셋.
- 툴바의 카메라 도구:
  - 게임 카메라로 보기: 활성 카메라(Main Camera) 시점으로 본다.
  - 선택 카메라를 이 시점으로: 선택한 카메라를 지금 에디터 시점으로 옮긴다.
  - 선택 카메라 시점으로 보기: 에디터 시점을 선택한 카메라로 옮긴다.

### Play Mode

- 툴바의 **Play**로 시작하고 **Stop**으로 끝낸다. Stop하면 Play 전 상태로 복원된다.
  단축키는 아직 없다.
- Play 화면은 활성 카메라 시점이다. 궤도 카메라(`CameraOrbitControlComponent`)가 있으면
  우클릭 드래그와 휠로 조작한다.

### Motion Editor

- 스켈레톤/클립 로드, 레이어 믹서, 액션 섹션/이벤트(Sound, Camera) 편집, 미리듣기.

## 프로젝트 구조

```
QuarterFlying/
├── engine/
│   ├── core/          # 엔진 수명주기(Engine), 로그, 메모리, 공통 타입
│   ├── platform/      # 플랫폼 추상화 (현재 Win32Platform만)
│   ├── input/         # 프레임 입력 상태(InputState)
│   ├── renderer/      # OpenGL 렌더러, 인스턴싱, RenderGraph
│   ├── ecs/           # ECS, 리플렉션, 시스템(Render/Camera/CameraRig/Particle...)
│   ├── animation/     # 스켈레톤, 클립, 레이어 믹서
│   ├── prefab/        # 프리팹 자산
│   ├── job/           # Job System
│   ├── asset/         # 에셋 관리 (텍스처 파일 -> 픽셀. RenderableComponent.texturePath가 이 경로를 쓴다)
│   ├── bindings/      # pybind11 바인딩 (quarterflying 모듈)
│   ├── editor/        # PySide6 에디터
│   ├── assets/        # 씬, 모델, 프리팹, 액션, 사운드
│   └── tests/         # C++ 테스트
├── docs/              # 계획서, 설계 문서
└── README.md
```

## 개발 문서

- [CLAUDE.md](CLAUDE.md): 작업 경로, 빌드 메모, 코드 품질 관례, 알려진 버그
- [ROADMAP.md](ROADMAP.md): 진행 상황
- [docs/REVIEW_BASED_IMPROVEMENT_PLAN.md](docs/REVIEW_BASED_IMPROVEMENT_PLAN.md): 개선안
- [docs/INGAME_CAMERA_PLAN.md](docs/INGAME_CAMERA_PLAN.md): 카메라 기능 계획
- [docs/ARCHITECTURE_COUPLING_AUDIT.md](docs/ARCHITECTURE_COUPLING_AUDIT.md): 모듈 결합도 점검
- [docs/ARCHITECTURE_KO.md](docs/ARCHITECTURE_KO.md)
- [docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md](docs/PYTHON_BINDING_IMPLEMENTATION_PLAN.md)
- 보관 문서: [docs/archive/legacy/](docs/archive/legacy/)

## 라이선스

이 프로젝트는 Apache License 2.0 하에 라이센스됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하십시오.
