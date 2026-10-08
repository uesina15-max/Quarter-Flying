# GE (Game Engine) — 아키텍처 문서

> **작성일**: 2026-07-01  
> **분석 기준**: `d:\ge\engine` 소스 트리 직접 분석  
> **언어**: C++23 (엔진 코어) / Python (에디터)  
> **빌드 시스템**: CMake 3.15+

---

## 목차

1. [프로젝트 개요](#1-프로젝트-개요)
2. [전체 디렉토리 구조](#2-전체-디렉토리-구조)
3. [레이어드 아키텍처](#3-레이어드-아키텍처)
4. [모듈별 상세 분석](#4-모듈별-상세-분석)
5. [핵심 설계 패턴](#5-핵심-설계-패턴)
6. [빌드 결과물 (Targets)](#6-빌드-결과물-targets)
7. [주요 의존성 (Third-Party)](#7-주요-의존성-third-party)
8. [아키텍처 불변 원칙 (5대 원칙)](#8-아키텍처-불변-원칙-5대-원칙)
9. [데이터 흐름 — 한 프레임의 생애](#9-데이터-흐름--한-프레임의-생애)
10. [테스트 구조](#10-테스트-구조)
11. [현재 구현 상태 요약](#11-현재-구현-상태-요약)

---

## 1. 프로젝트 개요

GE는 **C++23 기반의 모듈형 게임 엔진**으로, 다음 목표를 추구합니다.

| 목표 | 세부 내용 |
|---|---|
| **성능** | 멀티스레드 Job System + Work-Stealing, Lock-free ECS |
| **확장성** | Subsystem 플러그인 아키텍처, ECS 기반 게임 로직 |
| **스크립팅** | pybind11을 통한 Python ↔ C++ 양방향 바인딩 |
| **렌더링** | RenderGraph 기반 프레임 그래프, GPU 리소스 자동 라이프타임 관리 |
| **에디터** | Python(PySide6) 기반 독립 에디터, 엔진과 바인딩으로 연동 |

---

## 2. 전체 디렉토리 구조

```
d:\ge\
├── docs/                          # 아키텍처 원칙 문서
│   └── ArchitecturePrinciples.md
├── engine/                        # 엔진 소스 루트 (CMake 프로젝트)
│   ├── CMakeLists.txt             # 메인 빌드 정의
│   ├── core/                      # 엔진 핵심 (Engine, Subsystem, Memory, Logger)
│   │   ├── Engine.h / Engine.cpp
│   │   ├── Subsystem.h            # 서브시스템 기본 인터페이스
│   │   ├── Types.h                # 공통 타입 (Vec3, Quaternion, Handle<T>)
│   │   ├── EngineError.h          # Result<T> 에러 계약
│   │   ├── PlatformFactory.h/cpp  # 플랫폼 선택 팩토리
│   │   ├── UUID.h/cpp
│   │   ├── logging/               # Logger (zero-cost disabled log)
│   │   └── memory/                # FrameAllocator, PoolAllocator, StackAllocator
│   ├── platform/                  # 플랫폼 구현체
│   │   ├── IPlatform.h            # 플랫폼 추상 인터페이스
│   │   ├── Win32Platform.h/cpp    # Windows 네이티브 구현
│   │   └── GLFWPlatform.h/cpp     # GLFW 크로스플랫폼 구현
│   ├── job/                       # 멀티스레드 Job System
│   │   ├── JobSystem.h/cpp        # 최상위 퍼사드
│   │   ├── JobScheduler.h/cpp     # 스케줄링 로직
│   │   ├── WorkStealingDeque.h/cpp
│   │   ├── DependencyResolver.h/cpp
│   │   ├── JobLifecycleManager.h/cpp
│   │   └── ValidationLayer.h/cpp
│   ├── ecs/                       # Entity-Component-System
│   │   ├── World.h/cpp            # ECS 최상위 컨테이너
│   │   ├── WorldManager.h/cpp     # 다중 World 관리
│   │   ├── ECSRegistry.h/cpp      # Entity/Component 저장소
│   │   ├── EntityManager.h/cpp
│   │   ├── ComponentManager.h/cpp
│   │   ├── ComponentArray.h       # 타입 안전 배열
│   │   ├── ComponentTypeRegistry.h/cpp
│   │   ├── System.h               # System 기본 인터페이스
│   │   ├── SystemDependencyAnalyzer.h/cpp
│   │   ├── SystemDispatcher.h/cpp
│   │   ├── ParallelGroupBuilder.h/cpp
│   │   ├── Reflection.h/cpp       # 런타임 리플렉션 + 직렬화
│   │   ├── ScriptSystem.h/cpp     # Python 스크립트 System
│   │   └── Components.h           # 내장 컴포넌트 (Transform 등)
│   ├── renderer/                  # 렌더링 파이프라인
│   │   ├── Renderer.h/cpp         # Subsystem 구현체
│   │   ├── RenderGraph.h/cpp      # 프레임 그래프 (55KB)
│   │   ├── RGBuilder.h/cpp        # 패스 빌더 DSL
│   │   ├── CommandList.h          # GPU 명령 추상 인터페이스
│   │   ├── RenderBatchPolicy.h    # 인스턴스 배치 정책
│   │   ├── Mesh.h/cpp, Shader.h/cpp, Camera.h/cpp
│   │   └── opengl/
│   │       └── OpenGLCommandList.h/cpp
│   ├── gui/                       # ImGui 통합
│   │   └── ImGuiLayer.h/cpp
│   ├── bindings/                  # Python 바인딩 (pybind11)
│   │   ├── PythonModule.cpp
│   │   ├── ECSBindings.cpp
│   │   ├── EngineBindings.cpp
│   │   └── MathBindings.cpp
│   ├── app/                       # 샘플 애플리케이션
│   │   ├── main.cpp
│   │   └── DemoScene.cpp
│   ├── editor/                    # Python 에디터 (PySide6)
│   │   ├── main.py                # 메인 윈도우 (3분할 레이아웃)
│   │   ├── viewport.py
│   │   ├── motion_editor.py
│   │   ├── panels/                # UI 패널 8개
│   │   └── style/                 # QSS 테마
│   └── tests/                     # GoogleTest + RapidCheck 테스트 29개
└── .kiro/specs/                   # 사양 문서
```

---

## 3. 레이어드 아키텍처

```
┌─────────────────────────────────────────────────────────────┐
│                   Python Editor (PySide6)                    │  에디터 레이어
│       SceneHierarchy │ Inspector │ Viewport │ Timeline       │
└──────────────────────────┬──────────────────────────────────┘
                           │ ge_python 모듈 (pybind11)
┌──────────────────────────▼──────────────────────────────────┐
│                   Bindings Layer                             │  언어 바인딩
│      ECSBindings │ EngineBindings │ MathBindings             │
└──────────────────────────┬──────────────────────────────────┘
                           │ C++ API
┌──────────────────────────▼──────────────────────────────────┐
│                    ge_engine (Shared Library)                │  엔진 코어
│  ┌──────────┐  ┌─────────┐  ┌──────────┐  ┌─────────────┐  │
│  │  Engine  │  │   ECS   │  │ Renderer │  │  JobSystem  │  │
│  │  (Core)  │  │ (World) │  │ (Graph)  │  │  (Worker)   │  │
│  └────┬─────┘  └────┬────┘  └────┬─────┘  └──────┬──────┘  │
│  ┌────▼─────────────▼────────────▼────────────────▼──────┐  │
│  │                   Platform Layer                       │  │
│  │         IPlatform (Win32 / GLFW)                       │  │
│  └────────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
```

의존성 방향: **Editor → Bindings → [ECS, Engine, Renderer, Job] → Platform → OS**  
순환 의존성 없음. 상위 레이어만 하위 레이어를 참조합니다.

---

## 4. 모듈별 상세 분석

### 4.1 Core — 엔진 핵심

**핵심 클래스**: `Engine`

소유 관계:
- `IPlatform` — 플랫폼 구현체
- `FrameAllocator` — 16MB 프레임 스크래치 메모리
- `HighResolutionTimer`
- `JobSystem` — 멀티스레드 스케줄러
- `WorldManager` — ECS 월드 관리자
- `Subsystem[]` — 등록된 서브시스템 (type-indexed map)

**엔진 수명주기**:
```
Initialize() → Run() → [TickFrame() × N] → Shutdown()
                 TickFrame():
                  ├─ PollEvents()
                  ├─ Subsystem::Tick()
                  ├─ World::Update()
                  ├─ Subsystem::LateTick()
                  └─ FrameAllocator::Reset()
```

**메모리 관리**:

| 할당자 | 용도 | 특징 |
|---|---|---|
| `FrameAllocator` | 프레임당 임시 Job 데이터 | 선형 할당, 프레임 끝 일괄 해제 |
| `StackAllocator` | 레벨 로딩 등 스택형 임시 할당 | LIFO, 중첩 스코프 |
| `PoolAllocator` | 동일 크기 객체 빈번 할당 | O(1) 할당/해제 |
| `MemoryTracker` | 메모리 누수 추적 (디버그) | 파일/라인 정보 기록 |

**에러 처리** — `Result<T>` 표준 계약:
```cpp
// std::expected<T, EngineError> 별칭
Result<void> Engine::Initialize(const EngineConfig& config);
```
예외 미사용. 모든 실패는 `EngineError { code, message, component }` 구조체로 반환.

---

### 4.2 Platform — 플랫폼 추상화

**인터페이스**: `IPlatform`  
**구현체**: `Win32Platform` (WinAPI), `GLFWPlatform` (GLFW 3.3.8)

`PlatformFactory`가 빌드/런타임 조건에 따라 구현체를 선택합니다.  
에디터는 `InitializeFromWindowHandle(void* handle, ...)` API로 외부 HWND를 주입하여 Python 윈도우 내에 뷰포트를 임베드합니다.

---

### 4.3 Job System — 멀티스레드 스케줄러

**내부 구성** (컴포넌트 기반 분리):
```
JobSystem (퍼사드)
 ├── JobLifecycleManager   — Job 풀, 상태 관리
 ├── DependencyResolver    — 의존성 그래프, 위상 정렬
 ├── JobScheduler          — 워커 스레드 풀
 │    └── WorkStealingDeque (스레드별, lock-free CAS)
 └── ValidationLayer       — 순환 의존성 감지, 디버그 검증
```

**특성**:
- Work-Stealing으로 워커 간 부하 균등화
- `FrameAllocator` 통합 — Job 데이터를 힙이 아닌 스크래치 메모리에 할당
- `JobHandle` 배열로 DAG 형태 의존성 선언

```cpp
JobHandle h1 = jobSystem->Dispatch(taskA, dataA);
JobHandle h2 = jobSystem->Dispatch(taskB, dataB, &h1, 1); // h1 완료 후 실행
jobSystem->Wait(h2);
```

---

### 4.4 ECS — Entity-Component-System

**계층 구조**:
```
WorldManager
 └── World (1..N)
      ├── ECSRegistry
      │    ├── EntityManager         — Entity ID 생성/재활용
      │    ├── ComponentManager      — 컴포넌트 타입별 밀집 배열
      │    │    └── ComponentArray<T>
      │    └── ComponentTypeRegistry — 타입 해시 ↔ 메타데이터
      ├── System[] (우선순위 정렬)
      │    ├── SystemDependencyAnalyzer
      │    ├── ParallelGroupBuilder
      │    └── SystemDispatcher
      └── ScriptSystem (Python 스크립트)
```

**System 실행 모델**:
```
순차: [System A] → [System B] → [System C]

병렬:
  Group 1: [System A] ║ [System B]   (Write 충돌 없는 시스템)
  Group 2: [System C]               (Group 1 완료 후)
```

**Reflection 시스템**: `ComponentRegistry`로 필드 메타데이터(이름·타입·오프셋·UI 힌트) 등록, nlohmann/json 기반 직렬화/역직렬화, 에디터 인스펙터에서 단일 필드 부분 업데이트.

**Play-In-Editor (PIE)**: `World::Play()` / `Stop()` / `Pause()` 상태 전환, `m_Snapshot` (JSON)으로 Play 전 상태 스냅샷 저장 → Stop 시 자동 롤백.

**내장 컴포넌트**:
- `TransformComponent` — position(Vec3), rotation(Quaternion), scale(Vec3)
- `ScriptComponent` — Python 스크립트 경로 참조

---

### 4.5 Renderer — 렌더링 파이프라인

**RenderGraph 처리 순서**:
```
AddPass(name, setup, execute)
        ↓
  Compile()
   ├─ TopologicalSort()          (패스 실행 순서 결정)
   ├─ ComputeResourceLifetimes() (리소스 생존 범위 계산)
   ├─ ComputeAliasing()          (비겹침 리소스 메모리 공유)
   ├─ InsertBarriers()           (리소스 배리어 삽입)
   └─ CreatePhysicalResources()  (가상 → 실제 GPU 텍스처 매핑)
        ↓
  Execute(CommandList&)
   └─ execute_lambda(CommandList&) 호출
        ↓
  ReleaseTransientResources() / ReturnResourcesToPool()
```

**CommandList 추상화**:
```
CommandList (abstract)
 ├── OpenGLCommandList   (현재 구현된 백엔드)
 └── MockCommandList     (테스트용)
```
DirectX 12, Vulkan 백엔드는 `CommandList` 구현체로 추후 추가 가능.

**인스턴스 배치**: `RenderBatchKey` (meshId, materialId, shaderId) 복합 키로 동일 오브젝트 일괄 드로우.

---

### 4.6 GUI — ImGui 레이어

`ImGuiLayer` — `Subsystem` 구현체. ImGui 초기화·업데이트·렌더링 캡슐화.  
엔진 내부 디버그 UI 전용. 에디터 메인 UI는 Python PySide6로 분리.

---

### 4.7 Bindings — Python 인터페이스

**모듈명**: `ge_python` (pybind11 공유 모듈)

| 파일 | 노출 API |
|---|---|
| `ECSBindings.cpp` | Entity, TransformComponent, World, ECSRegistry, Reflection |
| `EngineBindings.cpp` | Engine, EngineConfig, 수명주기 |
| `MathBindings.cpp` | Vec3, Quaternion |

에디터는 **Stage 1** (더미 데이터로 실행) / **Stage 2** (`ge_python` 연결 시 실제 ECS 연동) 방식으로 graceful degradation 지원.

---

### 4.8 Editor — Python 에디터

**기술 스택**: Python 3.x + PySide6 (Qt6)

**메인 레이아웃** (3분할 + 하단 탭):
```
┌────────────────┬──────────────────┬────────────────┐
│ Scene          │   3D Viewport    │   Inspector     │
│ Hierarchy      │  (EngineViewport │   Panel         │
└────────────────┴──────────────────┴────────────────┘
│                    Motion Editor (선택 탭)           │
└─────────────────────────────────────────────────────┘
│                    Status Bar                        │
└─────────────────────────────────────────────────────┘
```

| 패널 | 역할 |
|---|---|
| `SceneHierarchyPanel` | Entity 트리 표시, 선택, 생성/삭제 |
| `InspectorPanel` | 선택 Entity의 컴포넌트 인스펙터 |
| `EngineViewport` | C++ 엔진 렌더링 결과 (HWND 임베드) |
| `EventTimeline` | 애니메이션 이벤트 타임라인 편집 |
| `MotionGraph` | 상태 머신 그래프 에디터 |
| `AnimationPreview` | 애니메이션 미리보기 |
| `SectionInspector` | 모션 섹션 상세 편집 |
| `StatusBar` | FPS, 메모리 사용량 등 상태 표시 |

---

## 5. 핵심 설계 패턴

### 5.1 Subsystem 플러그인 패턴

```cpp
engine.RegisterSubsystem<Renderer>(std::make_unique<Renderer>());
auto* renderer = engine.GetSubsystem<Renderer>(); // type_index 기반 O(1) 조회
```

### 5.2 제네릭 Handle<Tag> 타입 시스템 및 리소스 도메인 분리

```cpp
template<typename Tag>
struct Handle { uint32_t id; uint32_t generation; };

using TextureHandle    = Handle<TextureHandleTag>;       // Asset/Runtime Logical Reference
using RGTextureHandle  = Handle<RGTextureHandleTag>;     // RenderGraph Virtual Resource
using GPUTextureHandle = Handle<GPUTextureHandleTag>;    // Physical Backend Allocation
```
* **태그 분리**: 태그로 핸들 종류를 컴파일 타임에 구분하여 핸들 혼용을 타입 오류로 차단합니다.
* **도메인 계약**: 각 핸들은 명확히 다른 resource domain(논리 에셋 참조, 렌더그래프 가상 리소스, 물리 GPU 백엔드 할당)을 표현하며, domain 간 변환은 지정된 브리지 지점(예: `CreatePhysicalResources`)에서만 명시적으로 수행됩니다.

### 5.3 RenderGraph 프레임 그래프

선언적 패스 등록 → 컴파일 단계에서 위상 정렬·라이프타임 분석·메모리 에일리어싱 자동 처리.

### 5.4 ECS 선언적 스케줄링 및 단일 진실 출처 (Single Source of Truth)

```cpp
std::vector<size_t> GetReadComponentTypes() const override { ... }
std::vector<size_t> GetWriteComponentTypes() const override { ... }
```
* **스케줄링 계약**: `SystemDependencyAnalyzer`는 병렬 실행 금지 조건 및 스케줄링 제약의 단일 진실 출처(Single Source of Truth)입니다. 컴포넌트 접근 충돌(WAW, WAR, RAW)뿐 아니라 비컴포넌트 스케줄 제약(메인 스레드 affinity, 글로벌 상태 mutation, 외부 리소스 side effect 등)이 존재할 경우 반드시 Analyzer 단계에서 간선 또는 제약으로 모델링되어야 하며, `ParallelGroupBuilder`는 순수하게 그래프의 위상 정렬(Topological Sort)만 담당합니다.

### 5.5 컴포넌트 레지스트리 단일 파이프라인 (Component Registry Pipeline)

* **소유권 및 파이프라인**: 컴포넌트 등록의 Canonical Source는 단일 Entry Point(`ComponentTypeRegistry`)로 제한하며, 리플렉션 및 Python 바인딩 조회(`ComponentRegistry`)는 Projection/View 계층으로 작동합니다.
* **키 역할 분리**: `ComponentTypeID`는 런타임 저장소의 canonical dense key로 사용하고, `StringHash`는 reflection/binding 조회의 stable lookup key로 사용하며, `std::type_index`는 등록 시점 검증 보조 수단으로만 사용합니다.

---

## 6. 빌드 결과물 (Targets)

| 타겟명 | 타입 | 설명 |
|---|---|---|
| `ge_engine` | Shared Library (.dll) | 엔진 전체 (Core+Job+ECS+Renderer) |
| `ge_python` | Python 모듈 (.pyd) | pybind11 바인딩 |
| `simple_engine_test` | Executable | 샘플 앱 (DemoScene) |
| `ge_tests` | Executable | GoogleTest + RapidCheck 전체 테스트 |

---

## 7. 주요 의존성 (Third-Party)

모든 의존성은 CMake `FetchContent`로 빌드 시 자동 다운로드됩니다.

| 라이브러리 | 버전 | 용도 |
|---|---|---|
| **pybind11** | v2.11.1 | C++ ↔ Python 바인딩 |
| **nlohmann/json** | v3.11.3 | ECS 직렬화, World 스냅샷 |
| **fmt** | v10.1.1 | 로거 문자열 포맷팅 |
| **glm** | 0.9.9.8 | 수학 라이브러리 |
| **glew** | 2.2.0 | OpenGL 확장 로딩 |
| **glfw** | 3.3.8 | 크로스플랫폼 윈도우/입력 |
| **GoogleTest** | v1.14.0 | 단위 테스트 |
| **RapidCheck** | master | 속성 기반 테스트(PBT) |

---

## 8. 아키텍처 불변 원칙 (5대 원칙)

> 출처: `docs/ArchitecturePrinciples.md` (최종 개정: 2026-06-27)

| # | 원칙 | 핵심 규약 |
|---|---|---|
| 1 | **Read API는 상태를 변경하지 않는다** | Getter/Query는 const 메서드, 지연 초기화는 별도 Initialize() API |
| 2 | **죽은 코드는 Archive로 이동한다** | 주석 처리 코드 금지, Git 태그로 아카이브 |
| 3 | **Result<T>를 표준 오류 계약으로 사용한다** | std::expected<T,EngineError>, 예외·bool 반환 금지 |
| 4 | **Logger는 비활성 로그 비용을 발생시키지 않는다** | 컴파일/런타임 분기 전 인자 평가 차단 |
| 5 | **Singleton은 복사/이동 금지다** | 복사·이동 생성자/대입 = delete 명시 |

---

## 9. 데이터 흐름 — 한 프레임의 생애

```
[OS 이벤트]
     │ PollEvents()
     ▼
[InputEvent 큐]
     │
     ▼
[Engine::TickFrame()]
     │
     ├─ deltaTime 계산 (HighResolutionTimer)
     │
     ├─ Subsystem::Tick(dt) 순서대로
     │    └─ Renderer::BeginFrame()
     │
     ├─ World::Update(dt)
     │    ├─ 순차: System A → B → C
     │    └─ 병렬: [Group1: A || B] → [Group2: C]
     │              └─ JobSystem::Dispatch() → WorkStealingDeque → Worker Threads
     │
     ├─ Subsystem::LateTick(dt) 순서대로
     │    └─ Renderer::Render() → EndFrame()
     │         └─ RenderGraph::Compile() → Execute(CommandList)
     │              └─ OpenGLCommandList → GPU 드로우콜
     │
     └─ FrameAllocator::Reset()  ← Job 데이터 일괄 해제
```

---

## 10. 테스트 구조

**프레임워크**: GoogleTest (단위) + RapidCheck (속성 기반, PBT)  
**규모**: 테스트 파일 29개, 총 약 340KB

| 테스트 파일 | 대상 | 크기 |
|---|---|---|
| `SystemComponentQueryPropertyTests.cpp` | System 쿼리 속성 | 40KB |
| `ECSTests.cpp` | ECS 전체 통합 | 34KB |
| `SystemConcurrencyPropertyTests.cpp` | 병렬 ECS 안전성 | 20KB |
| `EngineTests.cpp` | Engine 수명주기 | 21KB |
| `ComponentTypeRegistryTests.cpp` | 컴포넌트 타입 등록 | 20KB |
| `RenderGraph*.cpp` (7개) | RenderGraph 전체 | ~70KB |
| `Memory*.cpp` (3개) | 메모리 할당자 | ~20KB |

RapidCheck PBT는 임의 입력 수백 개로 불변식을 검증합니다:
- System 실행 순서 불변성
- 컴포넌트 쿼리 완전성
- RenderGraph 배리어 정확성
- 메모리 할당자 경계 조건

---

## 11. 현재 구현 상태 요약

| 모듈 | 구현 완성도 | 비고 |
|---|---|---|
| **Core (Engine)** | 완성 | Subsystem 플러그인, FrameAllocator, 수명주기 |
| **Platform** | 완성 | Win32 + GLFW 양쪽 구현 |
| **Job System** | 완성 | Work-Stealing, 의존성 그래프, ValidationLayer |
| **ECS** | 완성 | 병렬 그룹, Reflection, PIE, Python ScriptSystem |
| **Renderer (RenderGraph)** | 완성 | 프레임 그래프, 배리어, 에일리어싱, 병렬 실행 |
| **OpenGL 백엔드** | 기초 | OpenGLCommandList 구현, 고급 기능 미완성 |
| **DirectX / Vulkan 백엔드** | 미착수 | CommandList 인터페이스는 준비됨 |
| **Python Bindings** | 완성 | ECS, Engine, Math API 노출 |
| **Editor (Python)** | 진행 중 | 레이아웃/패널 완성, 엔진 연동 Stage 2 |
| **GUI (ImGui)** | 기초 | ImGuiLayer 구조만 존재 |
| **Memory 할당자** | 완성 | Frame / Stack / Pool + MemoryTracker |

---

*이 문서는 `d:\ge` 소스 트리를 직접 분석하여 생성되었습니다. (2026-07-01)*
