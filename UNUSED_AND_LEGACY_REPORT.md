# 미사용 파일 및 레거시 흔적 분석 보고서

**작성일**: 2026-07-29  
**분석 대상**: Quarter Flying Game Engine 프로젝트  
**분석 범위**: 전체 프로젝트 파일 및 소스 코드

---

## 📋 목차

1. [미사용 파일 및 디렉토리](#1-미사용-파일-및-디렉토리)
2. [레거시 코드 흔적](#2-레거시-코드-흔적)
3. [오래된 주석 및 TODO/FIXME](#3-오래된-주석-및-todofixme)
4. [정리 권장 사항](#4-정리-권장-사항)
5. [유지 보수 필요 항목](#5-유지-보수-필요-항목)

---

## 1. 미사용 파일 및 디렉토리

### 1.1 아카이브된 파일 (이미 제거됨)

**상태**: ✅ 정리 완료  
**위치**: `engine/ARCHIVE_INFO.md`에 문서화되어 있으나 실제 파일은 이미 제거됨

#### 아카이브 1: QML 실험
- **이전 위치**: `engine/editor/qml_archive/`
- **내용**: Qt Quick/QML 기반 현대화 실험
- **현재 상태**: 파일들이 존재하지 않음 (이미 정리됨)
- **보존 이유**: 나중에 Qt Quick/QML로 전환할 경우 참조용

#### 아카이브 2: 레거시 GLFW 메인
- **이전 위치**: `engine/app/archive2/`
- **내용**: GLFW 기반 메인 애플리케이션
- **현재 상태**: 파일들이 존재하지 않음 (이미 정리됨)
- **보존 이유**: 디버깅 및 레퍼런스용

### 1.2 빌드 아티팩트

**상태**: ⚠️ .gitignore에 포함되어 있으나 정리 필요

#### 빌드 디렉토리
- `build/` - CMake 빌드 출력 디렉토리
- `engine/build/` - 추가 빌드 디렉토리
- `engine/build_*/` - 체크포인트 빌드 디렉토리

**정리 권장**: .gitignore에 이미 포함되어 있으나 디스크 공간 절약을 위해 정리 권장

#### 대형 테스트 결과 파일
- `engine/baseline_test_results.txt` (7.8MB)

**정리 권장**: .gitignore에 추가 및 제거 권장

### 1.3 중복 의존성

**상태**: ⚠️ 중복된 의존성 라이브러리 존재

#### 의존성 중복
- `build/deps/` - 루트 빌드 의존성
- `engine/build/_deps/` - 엔진 빌드 의존성

**정리 권장**: 단일 빌드 디렉토리 사용 권장

---

## 2. 레거시 코드 흔적

### 2.1 소스 코드 내 레거시 주석

**상태**: ✅ 대부분 정리됨  
**발견 위치**: `engine/` 디렉토리 내 프로젝트 소스 코드

#### 검색 결과
- 프로젝트 소스 코드 (`engine/core`, `engine/renderer`, `engine/platform` 등)에서는 레거시 관련 주석 없음
- 모든 레거시 참조는 외부 의존성 라이브러리에서만 발견됨

### 2.2 CMakeLists.txt 레거시 참조

**상태**: ✅ 정리됨  
**위치**: `engine/CMakeLists.txt`

#### 이전 레거시 코드 (제거됨)
```cmake
# 4. Simple test executable (optional - legacy GLFW main)
# Removed legacy GLFW main (using Qt-based editor instead)
```

#### 현재 상태
- GLFW 의존성이 완전히 제거됨
- Python 에디터가 유일한 진입점으로 설정됨
- 레거시 메인 애플리케이션 빌드 옵션 제거됨

### 2.3 빌드 시스템 변경 내역

#### 이전 (레거시 지원)
```cmake
option(BUILD_LEGACY_MAIN "Build legacy GLFW main executable" OFF)
if(BUILD_LEGACY_MAIN)
    add_executable(legacy_main app/main.cpp app/DemoScene.cpp)
    target_link_libraries(legacy_main PRIVATE ge_engine)
endif()
```

#### 현재 (Python 에디터 전용)
```cmake
# GLFW 의존성 제거
# Python 에디터가 유일한 진입점
install(TARGETS ge_python ...)
install(DIRECTORY editor/ ...)
```

---

## 3. 오래된 주석 및 TODO/FIXME

### 3.1 프로젝트 소스 코드

**상태**: ✅ 정리됨  
**검색 결과**: 프로젝트 소스 코드에서 TODO/FIXME 주석 없음

#### 검색된 파일
- `engine/core/*.cpp` - TODO/FIXME 없음
- `engine/renderer/*.cpp` - TODO/FIXME 없음
- `engine/platform/*.cpp` - TODO/FIXME 없음
- `engine/ecs/*.cpp` - TODO/FIXME 없음
- `engine/job/*.cpp` - TODO/FIXME 없음

### 3.2 외부 의존성 라이브러리

**상태**: ⚠️ 외부 라이브러리에 TODO/FIXME 존재  
**위치**: `build/_deps/` 및 `engine/build/_deps/`

#### 발견된 TODO/FIXME (외부 라이브러리)

**GLM 라이브러리**:
- `glm-src/test/core/core_type_vec2.cpp:245` - 사양 검토 필요한 TODO
- `glm-src/test/core/core_type_vec1.cpp:78` - 사양 검토 필요한 TODO

**JSON 라이브러리**:
- `json-src/tests/thirdparty/Fuzzer/test/StrncmpTest.cpp:13` - 다른 크기 검토 필요
- `json-src/tests/thirdparty/Fuzzer/test/MemcmpTest.cpp:11` - 다른 크기 검토 필요
- `json-src/tests/thirdparty/Fuzzer/FuzzerUtilWindows.cpp:161` - 효율성 개선 필요
- `json-src/tests/thirdparty/Fuzzer/FuzzerUtilDarwin.cpp:102` - 하드코딩된 쉘 경로 수정 필요
- `json-src/tests/thirdparty/Fuzzer/FuzzerTraceState.cpp:141` - std::set 효율성 개선 필요
- `json-src/tests/thirdparty/Fuzzer/FuzzerSHA1.cpp:63` - doxygen 문서화 필요
- `json-src/tests/src/unit-udt.cpp:196` - 예외 테스트 필요
- `json-src/tests/src/unit-element_access2.cpp:1494` - 테스트 케이스 병합 필요

**정리 권장**: 외부 라이브러리는 해당 프로젝트에 이슈 보고 권장

---

## 4. 정리 권장 사항

### 4.1 즉시 정리 권장

#### 대형 테스트 결과 파일 제거
```bash
# .gitignore에 추가
echo "baseline_test_results.txt" >> .gitignore

# 파일 제거
rm engine/baseline_test_results.txt
```

#### 중복 빌드 디렉토리 정리
```bash
# 단일 빌드 디렉토리 사용 권장
# 루트 build/ 디렉토리만 사용
rm -rf engine/build
rm -rf engine/build_*
```

### 4.2 ARCHIVE_INFO.md 업데이트

**현재 문제**: 실제 파일이 존재하지 않음에도 아카이브 정보 문서화

**권장 사항**:
```markdown
# 레거시 아카이브 정보

**작성일**: 2026-07-28  
**상태**: 아카이브 파일들이 정리됨 (2026-07-29 확인)

## 정리된 아카이브

### 아카이브 1: QML 실험
- **이전 위치**: `engine/editor/qml_archive/`
- **현재 상태**: ✅ 정리 완료
- **정리일**: 2026-07-29

### 아카이브 2: 레거시 GLFW 메인
- **이전 위치**: `engine/app/archive2/`
- **현재 상태**: ✅ 정리 완료
- **정리일**: 2026-07-29
```

### 4.3 라이센스 파일 정리

**현재 상태**: 중복된 라이센스 파일 존재

**권장 사항**:
- 프로젝트 루트의 `LICENSE` 파일만 유지
- 의존성 라이브러리의 라이센스 파일은 해당 라이브러리 관리에 맡김

---

## 5. 유지 보수 필요 항목

### 5.1 모니터링 필요

#### 의존성 업데이트
- 정기적으로 외부 의존성 라이브러리 업데이트 확인
- 보안 패치 적용

#### 빌드 시스템 최적화
- FetchContent 캐싱 전략 개선
- 빌드 시간 단축

### 5.2 문서화 필요

#### API 문서
- 엔진 API 문서화 진행
- 사용자 가이드 작성

#### 개발 문서
- 아키텍처 문서 최신화
- 기여 가이드라인 작성

### 5.3 테스트 강화

#### 커버리지 확대
- 기존 테스트 커버리지 분석
- 테스트 케이스 추가

#### CI/CD 통합
- 자동화된 테스트 파이프라인 구축
- 정적 분석 도구 통합

---

## 📊 요약

### 정리 상태

| 항목 | 상태 | 비고 |
|------|------|------|
| 아카이브 파일 | ✅ 정리 완료 | 실제 파일들이 제거됨 |
| 레거시 코드 | ✅ 정리 완료 | 소스 코드에서 레거시 참조 제거됨 |
| TODO/FIXME | ✅ 정리 완료 | 프로젝트 소스 코드에 없음 |
| 빌드 아티팩트 | ⚠️ 정리 필요 | 대형 파일 및 중복 디렉토리 |
| 외부 의존성 | ⚠️ 모니터링 필요 | TODO/FIXME 존재 (외부 라이브러리) |

### 권장 작업

1. **즉시 수행**: 대형 테스트 결과 파일 제거
2. **단기 계획**: 중복 빌드 디렉토리 정리
3. **중기 계획**: 문서화 강화 및 테스트 커버리지 확대
4. **장기 계획**: CI/CD 파이프라인 구축

---

**작성자**: Devin AI Assistant  
**검토일**: 2026-07-29