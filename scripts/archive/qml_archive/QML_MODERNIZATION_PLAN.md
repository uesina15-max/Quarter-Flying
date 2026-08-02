# Qt Quick/QML 현대화 계획서

**작성일**: 2026-07-28  
**프로젝트**: Quarter Flying Game Engine  
**목표**: Qt Widgets → Qt Quick/QML 전환  
**예상 기간**: 2-3주  
**우선순위**: 높음

---

## 📋 목차

1. [개요](#1-개요)
2. [Qt Quick/QML 전환 이유](#2-qt-quickqml-전환-이유)
3. [기술 스택](#3-기술-스택)
4. [아키텍처 설계](#4-아키텍처-설계)
5. [Phase별 구현 계획](#5-phase별-구현-계획)
6. [UI/UX 현대화](#6-uiux-현대화)
7. [성능 최적화](#7-성능-최적화)
8. [리스크 관리](#8-리스크-관리)

---

## 1. 개요

### 1.1 목적

현재 PySide6 Qt Widgets 기반 에디터를 Qt Quick/QML로 전환하여:
- 더 현대적인 UI/UX 제공
- 하드웨어 가속 그래픽 활용
- 향상된 성능 및 반응성
- 크로스 플랫폼 확장성

### 1.2 현재 상태

**기존 구조**:
- PySide6 Qt Widgets 기반
- Python 로직으로 UI 구성
- 기본적인 네이티브 룩앤필

**목표 구조**:
- Qt Quick/QML 기반
- 선언적 UI 정의
- 모던 플랫 디자인
- 애니메이션 및 트랜지션

---

## 2. Qt Quick/QML 전환 이유

### 2.1 기술적 이점

**Qt Quick/QML 장점**:
- ✅ 하드웨어 가속 OpenGL/Vulkan 렌더링
- ✅ 선언적 UI 정의 (코드 감소)
- ✅ 더 나은 성능 (특히 복잡한 UI)
- ✅ 플루이드 애니메이션 및 트랜지션
- ✅ 모바일/임베디드 크로스 플랫폼
- ✅ CSS 스타일링 유사성
- ✅ 머티미디어 지원

### 2.2 사용자 경험 이점

**UI/UX 개선**:
- 🎨 현대적 플랫 디자인
- 🌈 다크 모드 기본 지원
- ✨ 부드러운 애니메이션
- 📱 터치 친화적 인터페이스
- 🎯 직관적인 사용자 경험

---

## 3. 기술 스택

### 3.1 핵심 기술

- **Qt Quick**: 6.0+
- **QML**: 선언적 UI 언어
- **PySide6**: Python 바인딩
- **Qt Quick Controls**: 현대적 UI 컴포넌트
- **Qt Quick Layouts**: 반응형 레이아웃
- **Qt Quick 3D**: 3D 뷰포트 (선택적)

### 3.2 C++ 통합

- **Qt Quick QML 모듈**: C++ QML 타입 노출
- **Q_INVOKABLE**: C++ 메서드 QML 호출
- **Q_PROPERTY**: C++ 속성 QML 바인딩
- **시그널/슬롯**: 이벤트 기반 통신

---

## 4. 아키텍처 설계

### 4.1 계층 구조

```
QML UI Layer (Qt Quick)
├── Main.qml (메인 윈도우)
├── EditorLayout.qml (에디터 레이아웃)
├── ScenePanel.qml (씬 계층 패널)
├── InspectorPanel.qml (인스펙터 패널)
├── ViewportPanel.qml (뷰포트 패널)
└── Components/ (재사용 컴포넌트)

C++ Business Logic Layer
├── EngineIntegration (엔진 통합)
├── SceneManagement (씬 관리)
├── AssetManagement (에셋 관리)
└── ProjectManagement (프로젝트 관리)

Python Glue Layer
├── QML Bridge (QML-C++ 연결)
├── Event Handling (이벤트 처리)
└── State Management (상태 관리)
```

### 4.2 데이터 흐름

```
QML UI ←→ Python Bridge ←→ C++ Engine
    ↓            ↓              ↓
User Interaction  Logic Control  Rendering
```

---

## 5. Phase별 구현 계획

### Phase 1: 기반 구축 (3-4일)

**Task 1.1: Qt Quick 프로젝트 설정**
- PySide6 Qt Quick 모듈 설치
- QML 엔진 설정
- 기본 윈도우 생성

**Task 1.2: QML-C++ 바인딩 기초**
- C++ QML 모듈 구조
- 기본 타입 노출
- 시그널/슬롯 연결

**Task 1.3: 기본 레이아웃 구현**
- 메인 윈도우 QML
- 스플리터 레이아웃
- 기본 패널 구조

### Phase 2: 핵심 패널 QML 구현 (5-7일)

**Task 2.1: 엔진 뷰포트 QML**
- QQuickWidget 또는 QQuickWindow
- 엔진 렌더링 통합
- 입력 이벤트 처리

**Task 2.2: 씬 계층 QML**
- TreeView QML 컴포넌트
- 드래그 앤 드롭
- 컨텍스트 메뉴

**Task 2.3: 인스펙터 QML**
- PropertyEditor QML
- 다이나믹 폼 생성
- 실시간 업데이트

### Phase 3: 고급 기능 (3-4일)

**Task 3.1: 애니메이션 및 트랜지션**
- 패널 슬라이드 애니메이션
- 모드 전환 트랜지션
- 로딩 스피너

**Task 3.2: 테마 및 스타일링**
- Material Design 3 테마
- 다크/라이트 모드
- 커스텀 스타일

**Task 3.3: 성능 최적화**
- QML 컴파일
- 리소스 최적화
- 메모리 관리

---

## 6. UI/UX 현대화

### 6.1 디자인 원칙

**Material Design 3**:
- 플랫 디자인
- 네이티브 리징
- 애니메이션 강조
- 다크 모드 기본

### 6.2 컬러 시스템

```qml
// Theme.qml
QtObject {
    readonly property color primary: "#6200EE"
    readonly property color secondary: "#03DAC6"
    readonly property color background: "#121212"
    readonly property color surface: "#1E1E1E"
    readonly property color error: "#B00020"
    
    readonly property color onPrimary: "#FFFFFF"
    readonly property color onSecondary: "#000000"
    readonly property color onBackground: "#FFFFFF"
    readonly property color onSurface: "#FFFFFF"
    readonly property color onError: "#FFFFFF"
}
```

### 6.3 타이포그래피

```qml
// Typography.qml
QtObject {
    readonly property font h1: Qt.font({family: "Roboto", pixelSize: 32, weight: Font.Bold})
    readonly property font h2: Qt.font({family: "Roboto", pixelSize: 24, weight: Font.Bold})
    readonly property font body: Qt.font({family: "Roboto", pixelSize: 14, weight: Font.Normal})
    readonly property font caption: Qt.font({family: "Roboto", pixelSize: 12, weight: Font.Normal})
}
```

---

## 7. 성능 최적화

### 7.1 QML 최적화

- **QML 컴파일**: `.qml.qmlc` 파일 생성
- **로딩 최적화**: 동적 로딩, 지연 로딩
- **메모리 관리**: 객체 풀링, 자동 해제

### 7.2 렌더링 최적화

- **OpenGL/Vulkan**: 하드웨어 가속 활용
- **비동기 렌더링**: 백그라운드 스레드
- **뷰포트 캐싱**: 불필요한 렌더링 방지

---

## 8. 리스크 관리

### 8.1 기술적 리스크

**리스크**: QML 학습 곡선  
**완화**: 템플릿 활용, 점진적 전환

**리스크**: 성능 저하  
**완화**: 프로파일링, 최적화 단계 별도

**리스크**: C++ 통합 복잡성  
**완화**: 기존 바인딩 활용, 단순화된 인터페이스

### 8.2 프로젝트 리스크

**리스크**: 일정 지연  
**완화**: MVP 접근, 핵심 기능 우선

**리스크**: 사용자 저항  
**완화**: 베타 테스트, 피드백 수집

---

## 9. 성공 기준

### 9.1 기술적 기준

- ✅ QML UI 100% 작동
- ✅ 기존 기능 모두 유지
- ✅ 성능 저하 10% 이내
- ✅ 메모리 사용량 20% 이내 증가

### 9.2 사용자 경험 기준

- ✅ 현대적 UI/UX
- ✅ 부드러운 애니메이션
- ✅ 직관적인 인터페이스
- ✅ 다크 모드 지원

---

## 10. 다음 단계

### 10.1 즉시 조치

1. **Qt Quick 학습**
   - Qt Quick 튜토리얼
   - QML 기본 문법
   - Qt Quick Controls

2. **개발 환경 설정**
   - Qt Creator 설치
   - Qt Quick Designer 설정
   - QML 디버깅 환경

3. **프로토타입 개발**
   - 기본 QML 윈도우
   - 단순 레이아웃
   - C++ 통합 테스트

---

**문서 버전**: 1.0  
**최종 수정**: 2026-07-28  
**승인자**: [승인자 이름/직함]
