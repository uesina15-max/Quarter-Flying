# 레거시 아카이브 정보

**작성일**: 2026-07-28  
**상태**: 아카이브 파일들이 정리됨 (2026-07-29 확인)  
**목적**: 레거시 GLFW 기반 메인 애플리케이션 보존 기록

---

## 정리된 아카이브

### 아카이브 1: QML 실험

**이전 위치**: `engine/editor/qml_archive/`

**내용**:
- Qt Quick/QML 기반 현대화 실험
- Material Design 3 UI 프로토타입
- Python QML 브리지 구현

**파일** (이전):
- Main.qml, ViewportPanel.qml, SceneTreeItem.qml
- InspectorPanel.qml, Theme.qml
- qml_launcher.py, qml_engine_integration.py
- QML_MODERNIZATION_PLAN.md, QML_MODERNIZATION_REPORT.md

**현재 상태**: ✅ 정리 완료 (2026-07-29 확인)
**정리 이유**: Python 에디터로 전환 완료, QML 실험 종료

### 아카이브 2: 레거시 GLFW 메인

**이전 위치**: `engine/app/archive2/`

**내용**:
- GLFW 기반 메인 애플리케이션
- C++23 메인 루프
- 데모 씬 구현

**파일** (이전):
- main.cpp - GLFW 기반 메인 애플리케이션
- DemoScene.cpp - 데모 씬 구현

**현재 상태**: ✅ 정리 완료 (2026-07-29 확인)
**정리 이유**: Python 에디터가 유일한 진입점으로 전환

---

## 복구 방법 (Git 히스토리에서)

### QML 실험 복구

```bash
# Git 히스토리에서 복구
git checkout HEAD~N -- engine/editor/qml/
# N은 해당 파일이 존재했던 커밋 수
```

### 레거시 GLFW 메인 복구

```bash
# Git 히스토리에서 복구
git checkout HEAD~N -- engine/app/
# N은 해당 파일이 존재했던 커밋 수

# CMakeLists.txt 수정 필요
# BUILD_LEGACY_MAIN 옵션 추가 및 GLFW 의존성 복구
```

---

## 빌드 시스템 변경 내역

### 이전 (레거시 지원)

```cmake
option(BUILD_LEGACY_MAIN "Build legacy GLFW main executable" OFF)
if(BUILD_LEGACY_MAIN)
    add_executable(legacy_main app/main.cpp app/DemoScene.cpp)
    target_link_libraries(legacy_main PRIVATE ge_engine)
endif()
```

### 현재 (Python 에디터 전용)

```cmake
# GLFW 의존성 제거
# Python 에디터가 유일한 진입점
install(TARGETS ge_python ...)
install(DIRECTORY editor/ ...)
```

---

## 정리 일지

- **2026-07-28**: 아카이브 문서 작성
- **2026-07-29**: 아카이브 파일들 정리 완료 확인
- **2026-07-29**: 레거시 코드 및 의존성 제거 완료
- **2026-07-30**: 빌드 아티팩트 정리 완료
  - 대형 테스트 결과 파일 제거 (baseline_test_results.txt, 7.8MB)
  - .gitignore에 이미 포함되어 있어 정리 완료
  - 중복 빌드 디렉토리 정리 권장 (engine/build, engine/build_debug)
    - 수동 정리 필요: CMake/빌드 프로세스 실행 중인지 확인 후 제거

---

## 추가 정리 권장 사항

### 중복 빌드 디렉토리

현재 다음과 같은 중복 빌드 디렉토리가 존재:
- `build/` - 루트 빌드 디렉토리 (활성 사용 중)
- `engine/build/` - 엔진 빌드 디렉토리 (정리 권장)
- `engine/build_debug/` - 디버그 빌드 디렉토리 (정리 권장)

**정리 방법**:
```bash
# 빌드 프로세스가 실행 중이 아닌지 확인 후
rm -rf engine/build
rm -rf engine/build_debug
```

**참고**: 이러한 빌드 디렉토리는 이미 .gitignore에 포함되어 있으므로 Git 저장소에는 영향이 없습니다. 디스크 공간 절약을 위해 정리하는 것을 권장합니다.

---

## 참고 문서

- [미사용 및 레거시 분석 보고서](UNUSED_AND_LEGACY_REPORT.md)
- [문제 분석 보고서](PROBLEM_ANALYSIS_REPORT.md)
- [README](README.md)
