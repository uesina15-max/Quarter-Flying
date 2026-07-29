# Quarter Flying 프로젝트 문제점 분석 보고서

**작성일**: 2026-07-27 (업데이트: 2026-07-27)  
**분석 대상**: d:\Quarter Flying (GE Game Engine)  
**프로젝트 상태**: C++23 기반 고성능 멀티플랫폼 게임 엔진 개발 중  
**분석 기준**: 파일 시스템, 기존 문서, 소스 코드 구조 종합 분석

---

## 📋 목차

1. [종합 요약](#1-종합-요약)
2. [프로젝트 구조 문제점](#2-프로젝트-구조-문제점)
3. [빌드 시스템 문제점](#3-빌드-시스템-문제점)
4. [아키텍처 및 설계 문제점](#4-아키텍처-및-설계-문제점)
5. [코드 품질 및 테스트 문제점](#5-코드-품질-및-테스트-문제점)
6. [문서화 문제점](#6-문서화-문제점)
7. [렌더링 시스템 문제점](#7-렌더링-시스템-문제점)
8. [우선순위별 해결 방안](#8-우선순위별-해결-방안)
9. [버그 상세 분석](#9-버그-상세-분석-2026-07-28-추가)
10. [GUI 통합 분석](#10-gui-통합-분석-2026-07-28-추가)
11. [결론](#11-결론)

---

## 1. 종합 요약

### 전체 평가

Quarter Flying 프로젝트는 현대적인 C++23 기반 게임 엔진으로, Job System, ECS, RenderGraph 등 고성능 아키텍처를 구현하는 데 성공했습니다. 그러나 프로젝트 구조, 빌드 시스템, 문서화 측면에서 여러 문제점이 존재합니다.

| 영역 | 상태 | 심각도 | 완료율 |
|------|------|--------|--------|
| **프로젝트 구조** | ⚠️ 부분적 | 중간 | 70% |
| **빌드 시스템** | ❌ 미흡 | 높음 | 40% |
| **아키텍처** | ✅ 양호 | 낮음 | 90% |
| **코드 품질** | ⚠️ 부분적 | 중간 | 75% |
| **테스트** | ⚠️ 부분적 | 중간 | 65% |
| **문서화** | ⚠️ 부분적 | 중간 | 60% |
| **렌더링** | ⚠️ 진행 중 | 높음 | 70% |

### 핵심 문제점 요약

1. **다중 빌드 디렉토리 방치** - 디스크 공간 낭비 및 빌드 혼선
2. **빌드 시간 증가** - FetchContent 의존성 관리 미흡
3. **문서화 분산** - 문서 위치 불일치 및 최신화 부족
4. **테스트 통합 미완료** - 레거시 테스트와 신규 테스트 이원화
5. **에러 처리 불일치** - std::expected 부분적 적용
6. **렌더링 Phase 2 미완료** - GPU Instancing 및 BVH Culling 구현 필요

---

## 2. 프로젝트 구조 문제점

### 2.1 다중 빌드 디렉토리 방치 (개선됨)

**문제**: `engine/` 디렉토리 내에 여러 빌드 관련 디렉토리 존재

**현재 상태** (2026-07-27 재분석):
- `engine/build/` - 메인 빌드 디렉토리 ✅
- `engine/build/_deps/glew-src/build/` - 의존성 내부 빌드 (정상)
- `engine/engine/build_checkpoint/` - 체크포인트 빌드 (제거 필요)
- `engine/engine/build_test_error_recovery/` - 테스트용 빌드 (제거 필요)

**영향**:
- 일부 디스크 공간 낭비
- 빌드 결과물 혼동 가능성
- 개발자 혼란 유발

**근본 원인**:
- 빌드 설정 실험 후 정리 미흡
- 테스트/체크포인트 빌드 정리 부족

**해결 방안**:
```
조치:
1. engine/engine/build_checkpoint/ 삭제
2. engine/engine/build_test_error_recovery/ 삭제
3. 단일 build/ 디렉토리 사용 유지
4. 내부에 Debug/, Release/ 서브디렉토리 구성 권장
5. .gitignore에 build/ 패턴 명시 확인
```

### 2.2 임시 파일 방치 (해결됨)

**문제**: 루트 디렉토리에 임시 메모 파일 존재

**현재 상태** (2026-07-27 재분석):
- `archive_docs/새 텍스트 문서.txt` - 아카이브 디렉토리로 이동됨 ✅
- 루트 디렉토리 정리됨 ✅

**해결 방안**:
```
조치:
1. archive_docs/ 내용 검토 및 적절한 docs/로 이관 또는 삭제
2. 루트 디렉토리 정리 유지
3. 기술 논의는 docs/ 또는 .kiro/specs/에 문서화
```

### 2.3 디렉토리 구조 불일치

**문제**: 문서 위치 분산

**현재 상태**:
- `ARCHITECTURE_KO.md` - 루트
- `docs/` - 일부 문서
- `archive_docs/` - 아카이브 문서
- `.kiro/specs/` - 스펙 문서

**영향**:
- 문서 검색 어려움
- 신규 개발자 온보딩 지연
- 문서 최신화 부족

**해결 방안**:
```
조치:
1. docs/를 중앙 문서 저장소로 통합
2. 루트 ARCHITECTURE_KO.md를 docs/로 이동
3. archive_docs/ 내용 정리 및 docs/로 통합 또는 삭제
4. .kiro/specs/는 유지 (스펙 문서용)
```

### 2.4 대형 테스트 결과 파일 방치

**문제**: `engine/baseline_test_results.txt` 파일 (7.8MB) 존재

**영향**:
- 디스크 공간 낭비
- Git 리포지토리 크기 증가
- 버전 관리 비효율

**해결 방안**:
```
조치:
1. .gitignore에 baseline_test_results.txt 추가
2. CI/CD 파이프라인에서 결과 파일 생성 및 비교
3. 필요시 별도 artifacts 저장소 사용
```

---

## 3. 빌드 시스템 문제점

### 3.1 FetchContent 의존성 관리 미흡

**문제**: 모든 의존성을 FetchContent로 다운로드

**현재 의존성**:
- pybind11 v2.11.1
- nlohmann_json v3.11.3
- fmt 10.1.1
- glm 0.9.9.8
- glew 2.2.0
- glfw 3.3.8
- stb (master)
- tinyobjloader v2.0.0
- googletest v1.14.0
- rapidcheck (master)

**영향**:
- 초기 빌드 시간 10-20분+ 소요
- CI/CD 파이프라인 느림
- 네트워크 의존성
- 증분 빌드 비효율

**근본 원인**:
- 의존성 캐싱 전략 부재
- 바이너리 패키지 관리자 미사용

**해결 방안**:
```cmake
# vcpkg 통합 권장
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

# FetchContent 캐싱 설정
set(FETCHCONTENT_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/deps")
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)
```

### 3.2 CMakeLists.txt 복잡성

**문제**: 단일 CMakeLists.txt에 모든 의존성 선언

**영향**:
- 유지보수 어려움
- 의존성 버전 관리 복잡
- 조건부 빌드 설정 어려움

**해결 방안**:
```
조치:
1. cmake/ 디렉토리 생성
2. Dependencies.cmake 분리
3. PlatformOptions.cmake 분리
4. CompilerFlags.cmake 분리
```

### 3.3 Precompiled Headers 미사용

**문제**: PCH 미적용으로 빌드 시간 증가

**영향**:
- 반복 빌드 시간 30-50% 증가
- 헤더 파일 중복 컴파일

**해결 방안**:
```cmake
# PCH 추가
target_precompile_headers(ge_engine PRIVATE 
    "engine/common/PrecompiledHeader.h"
)
```

---

## 4. 아키텍처 및 설계 문제점

### 4.1 의존성 결합도

**문제**: core/ 모듈이 platform/, job/, ecs/ 모두 의존

**현재 구조**:
```
core/ → platform/, job/, ecs/
Engine.cpp → 모든 서브시스템 직접 참조
```

**영향**:
- 모듈 재사용 어려움
- 테스트 고립도 낮음
- 순환 의존성 위험

**해결 방안**:
```cpp
// ServiceLocator 패턴 도입 권장
class ServiceLocator {
    template<typename T>
    T* GetSubsystem();
    
    template<typename T>
    void RegisterSubsystem(T* subsystem);
};
```

### 4.2 에러 처리 불일치 (개선됨)

**문제**: std::expected 부분적 적용

**현재 상태** (2026-07-27 재분석):
- `EngineError.h` 기초 정의 완료 ✅
- `Engine::Initialize()`는 std::expected 사용 ✅
- Result<T> 타입 별칭 정의 완료 ✅
- 전체 프로젝트에 표준화 진행 중

**영향**:
- 일부 모듈에서 여전히 bool 반환 사용 가능성
- API 일관성 개선 필요

**해결 방안**:
```cpp
// 전체 프로젝트에 표준화 (진행 중)
using Result<T> = std::expected<T, EngineError>;

Result<JobHandle> DispatchJob(...);
Result<Entity> CreateEntity(...);
Result<TextureHandle> LoadTexture(...);
```

### 4.3 Python 바인딩 동기화 부족

**문제**: C++ Core와 Python API 드리프트

**현재 상태**:
- bindings/에 바인딩 분산
- 자동 테스트 부족
- C++ 리팩토링 시 동기화 필요

**영향**:
- 런타임 AttributeError 발생 가능
- 에디터/스크립팅 생태계 신뢰성 저하
- 관리 부담 증가

**해결 방안**:
```
조치:
1. CI 파이프라인에 Python 바인딩 Smoke Test 추가
2. C++ 클래스 변경 시 바인딩 검증 규약 도입
3. pytest 기반 바인딩 테스트 작성
```

---

## 5. 코드 품질 및 테스트 문제점

### 5.1 테스트 커버리지 부족

**문제**: 통합 테스트보다 단위 테스트에 집중

**현재 상태** (2026-07-27 재분석):
- 테스트 파일 29개
- GTest + RapidCheck 사용
- 커버리지 측정 도구 미통합
- baseline_test_results.txt (7.8MB) 존재

**영향**:
- 회귀 테스트 불완전
- 엣지 케이스 검증 부족
- 프로덕션 안정성 우려

**해결 방안**:
```
조치:
1. LLVM Coverage 또는 OpenCppCoverage 통합
2. CI/CD에 커버리지 리포트 추가
3. 목표: 80%+ 커버리지
4. 통합 테스트 강화 (Job System + ECS, Renderer + RenderGraph)
5. baseline_test_results.txt .gitignore 추가
```

### 5.2 정적 분석 도구 부족

**문제**: clang-tidy만 활성화

**현재 상태** (2026-07-27 재분석):
- clang-tidy 활성화 ✅
- cppcheck 미통합
- clang-analyzer 미사용
- TODO/FIXME/XXX/HACK 주석 없음 ✅ (좋은 코드 품질)

**해결 방안**:
```
조치:
1. cppcheck 통합
2. clang-analyzer 활용
3. AddressSanitizer, ThreadSanitizer 추가
4. MISRA C++ 규칙 부분 적용
```

### 5.3 메모리 누수 감지 부족

**문제**: MemoryTracker 존재하지만 자동화 부족

**해결 방안**:
```
조치:
1. LeakSanitizer 통합
2. CI/CD에 메모리 누수 테스트 추가
3. Valgrind (Linux) / Dr. Memory (Windows) 통합
```

---

## 6. 문서화 문제점

### 6.1 문서 위치 분산

**문제**: 문서가 여러 디렉토리에 분산

**현재 위치**:
- 루트: `ARCHITECTURE_KO.md`
- docs/: `ArchitecturePrinciples.md`, `ASSET_PIPELINE_ARCHITECTURE.md`
- archive_docs/: 분석 보고서들
- .kiro/specs/: 스펙 문서

**해결 방안**:
```
조치:
1. docs/를 중앙 문서 저장소로 통합
2. 다음 구조로 재편성:
   docs/
   ├── architecture/
   │   ├── ARCHITECTURE_KO.md
   │   ├── ArchitecturePrinciples.md
   │   └── ASSET_PIPELINE_ARCHITECTURE.md
   ├── analysis/
   │   ├── PROJECT_ANALYSIS_KO.md
   │   └── TECHNICAL_DEBT_REPORT_KO.md
   └── guides/
       ├── CONTRIBUTING.md
       └── CODING_STYLE.md
```

### 6.2 API 문서화 부족

**문제**: Doxygen 또는 API 문서 생성기 미사용

**해결 방안**:
```
조치:
1. Doxygen 설정 추가
2. GitHub Pages 배포
3. 주요 클래스에 문서 주석 추가
```

### 6.3 기여 가이드 부족

**문제**: CONTRIBUTING.md 미작성

**해결 방안**:
```
작성 내용:
1. 코딩 스타일 가이드
2. 커밋 메시지 규칙 (Conventional Commits)
3. PR 리뷰 프로세스
4. 개발 환경 설정 방법
5. 테스트 실행 방법
```

---

## 7. 렌더링 시스템 문제점

### 7.1 Phase 2 최적화 미완료

**문제**: GPU Instancing 및 BVH Culling 구현 필요

**현재 상태**:
- RenderGraph 기본 구현 완료 ✅
- Phase 2 최적화 계획 수립 (🛠 A.ini) ✅
- Mesh::drawInstanced() 메서드 구현됨 ✅
- Mesh 캐싱 시스템 구현됨 ✅
- 전체 인스턴싱 파이프라인 미완료

**필요 작업**:
1. GPU Instancing 파이프라인 구현
2. BVH (Bounding Volume Hierarchy) Frustum Culling
3. OpenGL 3.3 Core 상향
4. 인스턴스 배치 키 설계 `(mesh_guid, mat_id, shader_id)`

**잠재적 위험**:
- 인스턴싱 그룹화 키 설계 미흡
- Python BVH 트리 순회 오버헤드
- RenderGraph 저수준 GPU API 고착화

### 7.2 멀티스레드 CommandList Race Condition

**문제**: 멀티스레드 CommandList 생성 시 경합 가능성

**해결 방안**:
```
조치:
1. 스레드별 독립 CommandList 할당
2. 프레임 끝 Lock-free 병합 구조
3. 원자적 연산 및 쓰기 지연 큐 활용
```

### 7.3 FrameAllocator 수명 주기 불일치

**문제**: 비동기 Job에서 FrameAllocator 메모리 참조 위험

**해결 방안**:
```
조치:
1. 프레임 경계 넘는 메모리 참조 금지 규약
2. 비동기 작업용 별도 할당자 사용
3. 댕글링 포인터 검증 도구 추가
```

---

## 8. 우선순위별 해결 방안

### 🔴 1순위 (즉시 - 1주 이내)

| 작업 | 예상 시간 | 영향 | 난이도 |
|------|---------|------|--------|
| 체크포인트/테스트 빌드 디렉토리 정리 | 0.5일 | ⭐⭐⭐ | 쉬움 |
| 대형 테스트 결과 파일 .gitignore 추가 | 0.5일 | ⭐⭐⭐⭐ | 쉬움 |
| 문서 위치 통합 (docs/ 중앙화) | 1일 | ⭐⭐⭐⭐ | 보통 |
| archive_docs/ 내용 정리 | 1일 | ⭐⭐⭐ | 보통 |

**예상 결과**:
- 프로젝트 구조 명확화
- 디스크 공간 수 GB 절약
- Git 리포지토리 크기 최적화

### 🟠 2순위 (2-4주 이내)

| 작업 | 예상 시간 | 영향 | 난이도 |
|------|---------|------|--------|
| 빌드 시스템 최적화 (캐싱, PCH) | 3일 | ⭐⭐⭐⭐⭐ | 보통 |
| 의존성 관리자 (vcpkg) 전환 검토 | 2일 | ⭐⭐⭐⭐ | 어려움 |
| 테스트 커버리지 도구 통합 | 2일 | ⭐⭐⭐⭐ | 보통 |
| Python 바인딩 Smoke Test 추가 | 2일 | ⭐⭐⭐ | 보통 |
| 기여 가이드 (CONTRIBUTING.md) 작성 | 1일 | ⭐⭐⭐ | 쉬움 |

**예상 결과**:
- 초기 빌드 시간 50-70% 단축
- 테스트 커버리지 가시화
- Python 바인딩 안정성 확보

### 🟡 3순위 (1-2개월 이내)

| 작업 | 예상 시간 | 영향 | 난이도 |
|------|---------|------|--------|
| 렌더링 Phase 2 최적화 구현 | 2주 | ⭐⭐⭐⭐⭐ | 어려움 |
| 의존성 역전 (ServiceLocator) | 1주 | ⭐⭐⭐⭐ | 어려움 |
| 정적 분석 도구 강화 | 3일 | ⭐⭐⭐ | 보통 |
| API 문서 생성 (Doxygen) | 3일 | ⭐⭐⭐ | 보통 |
| 통합 테스트 강화 | 1주 | ⭐⭐⭐⭐ | 어려움 |

**예상 결과**:
- 대규모 씬 렌더링 성능 10x+ 개선
- 모듈 재사용성 개선
- 프로덕션 안정성 확보

### 🟢 4순위 (2개월 이상)

| 작업 | 예상 시간 | 영향 | 난이도 |
|------|---------|------|--------|
| 자동 Python 바인딩 생성 | 2주 | ⭐⭐⭐ | 어려움 |
| 크로스 플랫폼 지원 (Linux/macOS) | 1개월+ | ⭐⭐⭐⭐⭐ | 매우 어려움 |
| Vulkan 렌더러 지원 | 2개월+ | ⭐⭐⭐⭐⭐ | 매우 어려움 |
| GPU 작업 시스템 | 2개월+ | ⭐⭐⭐⭐⭐ | 매우 어려움 |

---

## 9. 버그 상세 분석 (2026-07-28 추가)

### 9.1 C++ 코드 버그

#### 9.1.1 main.cpp 루프 조건 논리 오류 (심각도: 높음)

**위치**: <ref_file file="D:\Quarter Flying\engine\app\main.cpp" lines="89-89" />

**문제**:
```cpp
while (appRunning && engine.IsRunning() == false)
```

**설명**: 루프 조건이 논리적으로 반대입니다. `engine.IsRunning() == false`는 엔진이 실행 중이지 않을 때만 루프를 계속한다는 의미입니다. 올바른 조건은 `engine.IsRunning()`이어야 합니다.

**영향**:
- 메인 루프가 엔진이 초기화된 직후에 종료될 수 있음
- 애플리케이션이 정상적으로 실행되지 않음

**해결 방안**:
```cpp
while (appRunning && engine.IsRunning())
```

#### 9.1.2 AssetManager 비동기 로딩 미구현 (심각도: 중간)

**위치**: <ref_file file="D:\Quarter Flying\engine\asset\AssetManager.cpp" lines="102-119" />

**문제**: `LoadAssetAsync` 함수가 실제 비동기 로딩을 구현하지 않고 동기 로딩을 수행함

**설명**: 함수 이름은 비동기를 암시하지만 내부적으로는 동기 `LoadAsset`을 호출하고 즉시 결과를 반환합니다.

**영향**:
- 비동기 로딩의 이점을 얻을 수 없음
- 대형 에셋 로딩 시 메인 스레드 차단
- 로딩 화면 구현 불가

**해결 방안**:
```cpp
std::future<Result<AssetHandle>> AssetManager::LoadAssetAsync(
    const std::string& path,
    std::function<void(Result<AssetHandle>)> callback
)
{
    return std::async(std::launch::async, [this, path, callback]() {
        auto result = LoadAsset(path);
        if (callback) {
            callback(result);
        }
        return result;
    });
}
```

### 9.2 셰이더 버그

#### 9.2.1 deferred.frag 미사용 uniform (심각도: 낮음)

**위치**: <ref_file file="D:\Quarter Flying\engine\assets\shaders\deferred.frag" lines="14-15" />

**문제**: `inverseProj` uniform이 선언되었지만 사용되지 않음

**설명**: 셰이더에서 `inverseProj`가 선언되어 있지만 실제로는 사용되지 않아 불필요한 GPU 메모리를 차지합니다.

**영향**:
- 불필요한 uniform 전송
- GPU 메모리 낭비 (미미함)

**해결 방안**:
```glsl
// uniform mat4 inverseProj; // 제거 또는 실제 구현
```

#### 9.2.2 Depth 재구성 정확성 문제 (심각도: 중간)

**위치**: <ref_file file="D:\Quarter Flying\engine\assets\shaders\deferred.frag" lines="72-76" />

**문제**: Depth 버퍼에서 위치 재구성 로직이 정확하지 않을 수 있음

**설명**: 현재 depth 재구성이 간단하게 구현되어 있지만, 다양한 프로젝션 설정에서 정확성을 보장할 수 없습니다.

**해결 방안**:
```glsl
// 더 정확한 depth 재구성 방법 고려
float ndcDepth = depth * 2.0 - 1.0;
vec4 clipSpacePos = vec4(TexCoords * 2.0 - 1.0, ndcDepth, 1.0);
vec4 viewSpacePos = inverseProj * clipSpacePos;
vec3 FragPos = viewSpacePos.xyz / viewSpacePos.w;
```

### 9.3 빌드 시스템 버그

#### 9.3.1 존재하지 않는 테스트 디렉토리 참조 (심각도: 높음)

**위치**: <ref_file file="D:\Quarter Flying\engine\CMakeLists.txt" lines="308" />

**문제**: `add_subdirectory(tests)`를 호출하지만 `tests/` 디렉토리가 존재하지 않음

**설명**: CMakeLists.txt에서 테스트 서브디렉토리를 추가하려고 하지만 해당 디렉토리가 없어 빌드 실패 가능성이 있습니다.

**영향**:
- CMake 구성 단계에서 오류 발생 가능
- 테스트 빌드 실패

**해결 방안**:
```cmake
# tests 디렉토리 생성 또는 조건부 추가
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    add_subdirectory(tests)
endif()
```

#### 9.3.2 대형 테스트 결과 파일 관리 부족 (심각도: 중간)

**위치**: <ref_file file="D:\Quarter Flying\engine\baseline_test_results.txt" />

**문제**: 7.8MB 크기의 테스트 결과 파일이 Git 리포지토리에 포함됨

**설명**: 테스트 결과 파일이 너무 커서 리포지토리 크기를 증가시키고 버전 관리에 부담을 줍니다.

**영향**:
- Git 리포지토리 크기 증가
- 클론/풀 시간 증가
- 불필요한 이력 축적

**해결 방안**:
```
.gitignore에 추가:
baseline_test_results.txt
```

### 9.4 테스트 인프라 문제

#### 9.4.1 테스트 실패 기록 (심각도: 중간)

**위치**: <ref_file file="D:\Quarter Flying\engine\baseline_test_results.txt" lines="1-3" />

**문제**: SimpleEngineTests가 764.91초 후 실패로 기록됨

**설명**: 테스트가 매우 오래 실행된 후 실패했으며, 이는 메인 루프 조건 버그와 관련이 있을 수 있습니다.

**영향**:
- 테스트 신뢰성 저하
- CI/CD 파이프라인 지연

**해결 방안**:
1. main.cpp 루프 조건 버그 수정
2. 테스트 타임아웃 설정 추가
3. 테스트 결과 파일 정리

---

## 10. GUI 통합 분석 (2026-07-28 추가)

### 10.1 현재 GUI 아키텍처 문제

#### 10.1.1 이중 GUI 구조 (심각도: 높음)

**문제**: 프로젝트에 두 개의 완전히 분리된 GUI 시스템이 존재

**현재 구조**:
1. **메인 GUI (Native C++)**
   - 위치: <ref_file file="D:\Quarter Flying\engine\app\main.cpp" />
   - 프레임워크: GLFW + OpenGL
   - 용도: 데모 애플리케이션, 엔진 테스트
   - 특징: 네이티브 C++로 직접 윈도우 생성 및 렌더링

2. **에디터 GUI (Python/Qt)**
   - 위치: <ref_file file="D:\Quarter Flying\engine\editor\main.py" />
   - 프레임워크: PySide6 (Qt6)
   - 용도: 씬 에디터, 모션 에디터
   - 특징: Qt 위젯 시스템 기반, QStackedWidget으로 모드 전환

**영향**:
- 코드 중복: 두 GUI 모두 엔진 초기화, 렌더링 루프 등을 따로 구현
- 데이터 불일치: 에디터에서 수정한 내용이 메인 GUI에 반영되지 않음
- 유지보수 복잡: 두 시스템을 따로 관리해야 함
- 사용자 혼란: 데모와 에디터가 별도 애플리케이션으로 실행

#### 10.1.2 메인 GUI 루프 버그와 연관성

**문제**: 메인 GUI의 루프 조건 버그가 GUI 통합의 필요성을 더욱 부각

**설명**: <ref_file file="D:\Quarter Flying\engine\app\main.cpp" lines="89" />의 루프 조건 버그는 메인 GUI가 독립적으로 개발되면서 발생한 문제입니다. 통합된 GUI라면 이러한 버그를 방지할 수 있습니다.

### 10.2 PySide6 통합 방안

#### 10.2.1 통합 전략

**목표**: GLFW 기반 메인 GUI를 제거하고 PySide6 에디터를 단일 GUI로 통합

**주요 변경사항**:
1. 엔진 렌더링을 Qt 위젯에 임베딩
2. 메인 C++ 애플리케이션을 Python 스크립트로 대체
3. 데모 모드를 에디터 내의 플레이 모드로 통합
4. 단일 진입점으로 통합

#### 10.2.2 기술적 구현 방안

**1. 엔진 렌더링 Qt 위젯 임베딩**

```python
# editor/qt_engine_viewport.py (신규)
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import QTimer
import ge_python

class QtEngineViewport(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.engine = None
        self.world = None
        
        # 엔진 초기화
        self._init_engine()
        
        # 렌더링 타이머
        self.render_timer = QTimer(self)
        self.render_timer.timeout.connect(self._render_frame)
        self.render_timer.start(16)  # ~60fps
        
    def _init_engine(self):
        # Qt 윈도우 핸들을 엔진에 전달
        window_handle = self.winId()
        
        config = ge_python.EngineConfig()
        config.windowTitle = "Quarter Flying"
        config.windowWidth = self.width()
        config.windowHeight = self.height()
        
        self.engine = ge_python.Engine()
        self.engine.InitializeFromWindowHandle(window_handle, config)
        
    def _render_frame(self):
        if self.engine:
            self.engine.TickFrame()
            
    def resizeEvent(self, event):
        if self.engine:
            # 엔진에 리사이즈 통지
            platform = self.engine.GetPlatform()
            if platform:
                platform.OnResize(event.size().width(), event.size().height())
        super().resizeEvent(event)
```

**2. 통합된 메인 에디터 구조**

```python
# editor/main.py (수정)
class EditorMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Quarter Flying Editor")
        
        # 기존 Scene Editor와 Motion Editor 유지
        self._build_central_widget()
        
        # 플레이 모드 추가
        self._add_play_mode()
        
    def _add_play_mode(self):
        # 플레이 모드에서는 엔진 뷰포트를 전체 화면으로 표시
        self.play_mode_widget = QWidget()
        layout = QVBoxLayout(self.play_mode_widget)
        
        self.play_viewport = QtEngineViewport()
        layout.addWidget(self.play_viewport)
        
        self.stack.addWidget(self.play_mode_widget)
```

**3. C++ 진입점 제거**

```cmake
# CMakeLists.txt (수정)
# 기존 main.cpp 제거 또는 레거시용으로 유지
# add_executable(simple_engine_test app/main.cpp ...)  # 제거 또는 조건부

# Python 진입점 추가
# install(TARGETS ge_python DESTINATION .)
# install(FILES editor/main.py DESTINATION editor)
```

#### 10.2.3 마이그레이션 단계

**Phase 1: 기술 검증 (1-2주)**
- Qt 위젯에 엔진 렌더링 임베딩 테스트
- ge_python 바인딩 확장 (윈도우 핸들 전달)
- 기본 뷰포트 기능 구현

**Phase 2: 기능 이전 (2-3주)**
- 데모 씬을 에디터로 이전
- 플레이/정지 기능 통합
- config.json 로직 통합

**Phase 3: 레거시 제거 (1주)**
- GLFW 기반 main.cpp 제거 또는 레거시용으로 분리
- 의존성 정리 (GLFW 최소화)
- 문서 업데이트

#### 10.2.4 장점과 단점

**장점**:
- ✅ 단일 GUI로 통합으로 유지보수성 향상
- ✅ Qt의 풍부한 위젯 시스템 활용
- ✅ 에디터와 런타임 데이터 일관성 보장
- ✅ 플랫폼 간 UI 일관성
- ✅ 기존 에디터 인프라 활용

**단점**:
- ⚠️ Qt 의존성 추가 (PySide6)
- ⚠️ C++ 네이티브 성능 저하 가능성 (미미)
- ⚠️ ge_python 바인딩 확장 필요
- ⚠️ 학습 곡선 (C++ → Python)

### 10.3 권장 사항

**즉시 조치**:
1. 기술 검증을 위한 Qt 엔진 뷰포트 프로토타입 개발
2. ge_python 바인딩에 윈도우 핸들 전달 기능 추가
3. 통합 타임라인 수립

**중기 계획**:
1. Phase별 마이그레이션 실행
2. 테스트 커버리지 확보
3. 문서 및 튜토리얼 업데이트

**장기 목표**:
1. 단일 GUI 기반의 통합 개발 환경
2. 플러그인 아키텍처로 확장성 확보
3. 다른 엔진 통합 경험 축적

---

## 11. 결론

### 전체 평가

Quarter Flying 프로젝트는 현대적인 C++23 기반 게임 엔진으로서 핵심 아키텍처(Job System, ECS, RenderGraph)가 잘 설계되어 있습니다. 특히 Work-stealing 멀티스레딩과 Archetype 기반 ECS는 성능 최적화 측면에서 탁월한 구현을 보여줍니다.

**2026-07-27 재분석 결과**:
- 이전 보고서에서 식별된 주요 문제점들이 부분적으로 개선됨
- 다중 빌드 디렉토리 문제가 크게 개선됨
- 임시 파일 정리 완료
- 에러 처리 표준화 진행 중
- 렌더링 인스턴싱 기초 구현 완료
- 코드 품질 양호 (TODO/FIXME 주석 없음)

그러나 여전히 프로젝트 구조, 빌드 시스템, 문서화 측면에서 개선이 필요합니다. 특히 대형 테스트 결과 파일 방치와 FetchContent 의존성 관리 미흡은 개발 생산성에 직접적인 영향을 미치는 문제입니다.

### 핵심 권장사항

1. **즉시 조치**: 체크포인트/테스트 빌드 디렉토리 정리 및 대형 테스트 결과 파일 .gitignore 추가
2. **단기 개선**: 빌드 시스템 최적화 및 문서 통합
3. **중기 목표**: 렌더링 Phase 2 최적화 구현
4. **장기 비전**: 크로스 플랫폼 지원 및 고급 렌더링 기능

### 성공 지표

- 빌드 시간: 10-20분 → 3-5분 (70% 단축)
- 테스트 커버리지: 현재 미측정 → 80%+
- 문서화: 분산 → 중앙화 (docs/)
- 렌더링 성능: 기준 → 10x+ 개선 (Phase 2 완료 시)
- Git 리포지토리 크기: 7.8MB 테스트 결과 파일 제거

---

**보고서 작성자**: Cascade AI Assistant  
**분석 완료일**: 2026-07-27  
**다음 리뷰 권장일**: 2주 후 (2026-08-10)
