# Quarter Flying Game Engine

C++23 기반 고성능 멀티플랫폼 게임 엔진

## 시작 방법

### 에디터 모드

**Windows**:
```bash
# 방법 1: 스크립트 사용
scripts\start_editor.bat

# 방법 2: 직접 실행
cd engine
python editor/main.py
```

**Linux/Mac**:
```bash
# 방법 1: 스크립트 사용
./scripts/start_editor.sh

# 방법 2: 직접 실행
cd engine
python3 editor/main.py
```

## 빌드 방법

### 필수 요구사항

- CMake 3.15+
- C++23 지원 컴파일러 (MSVC 2022+, GCC 11+, Clang 14+)
- Python 3.8+ (에디터용)
- PySide6 (에디터용)

### 빌드 단계

```bash
cd engine
mkdir build && cd build
cmake ..
cmake --build .
```

### Python 에디터 설정

```bash
# PySide6 설치
pip install PySide6

# 에디터 실행
cd engine
python editor/main.py
```

## 프로젝트 구조

```
Quarter Flying/
├── engine/              # 엔진 소스 코드
│   ├── core/          # 코어 시스템
│   ├── platform/      # 플랫폼 추상화
│   ├── renderer/      # 렌더링 시스템
│   ├── ecs/           # ECS 아키텍처
│   ├── job/           # Job 시스템
│   ├── asset/         # 에셋 관리
│   ├── bindings/      # Python 바인딩
│   ├── editor/        # PySide6 에디터
│   ├── assets/        # 에셋 파일
│   └── CMakeLists.txt
├── scripts/           # 유틸리티 스크립트
├── docs/              # 문서
└── README.md
```

## 주요 기능

- **고성능 ECS**: 메모리 효율적인 엔티티 컴포넌트 시스템
- **Job System**: 멀티스레딩 작업 스케줄링
- **RenderGraph**: 현대적인 렌더링 파이프라인
- **Python 통합**: PySide6 기반 단일 에디터
- **3-모드 UI**: Scene Editor / Play Mode / Motion Editor
- **데모 씬 통합**: scene.json 로드/저장
- **설정 관리**: config.json 기반 설정
- **멀티플랫폼**: Windows, Linux, macOS 지원

## 개발 문서

- [최종 보고서](FINAL_SINGLE_GUI_REPORT.md) - 단일 GUI 통합 완료
- [아키텍처](docs/ARCHITECTURE_KO.md)
- [문제 분석 보고서](PROBLEM_ANALYSIS_REPORT.md)
- [GUI 통합 계획](GUI_INTEGRATION_PLAN.md)
- [Phase 1 기술 검증](engine/editor/PHASE1_TECHNICAL_VERIFICATION.md)
- [Phase 2 기능 이전](engine/editor/PHASE2_FUNCTIONAL_MIGRATION.md)
- [Phase 3 레거시 제거](engine/editor/PHASE3_LEGACY_REMOVAL.md)

## 에디터 사용법

### 기본 모드

1. **씬 로드**: File → Open Scene → scene.json 선택
2. **오브젝트 선택**: 씬 계층에서 오브젝트 클릭
3. **속성 편집**: 인스펙터에서 속성 수정
4. **플레이 테스트**: Play 버튼 클릭

### 플레이 모드

1. **플레이 시작**: Play 버튼 또는 F5
2. **일시정지**: Pause 버튼 또는 F6
3. **정지**: Stop 버튼 또는 F7

### 모션 에디터

1. **모드 전환**: Motion Editor 탭 클릭
2. **애니메이션 편집**: 타임라인 조작
3. **그래프 편집**: 커브 에디터 사용

## 라이선스

이 프로젝트는 Apache License 2.0 하에 라이센스됩니다. 자세한 내용은 [LICENSE](LICENSE) 파일을 참조하십시오.

## 기여

## 기여

기여 방법과 컨트리뷰션 가이드라인 추가 필요

## 연락처

[연락처 정보 추가 필요]
