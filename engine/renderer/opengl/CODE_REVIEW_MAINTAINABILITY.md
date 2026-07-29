# OpenGL 백엔드 코드 검토 및 유지보수성 평가

## 검토 대상
- `OpenGLResourceManager.h/cpp`
- `OpenGLCommandList.h/cpp`

## 발견된 버그

### 심각도: 높음 (High)

#### 1. 핸들 타입 불일치 (OpenGLCommandList.cpp:332)
```cpp
OpenGLVertexArray* vertexArray = resourceManager->GetVertexArray(GPUTextureHandle(vao, 0));
```
**문제**: VAO 핸들을 `GPUTextureHandle`로 변환하고 있습니다. VAO는 텍스처가 아닌 별도의 리소스 타입입니다.
**영향**: 타입 시스템의 안전성이 깨지며, 잘못된 핸들 매핑으로 인한 런타임 오류 가능성
**수정 필요**: 별도의 `GPUVertexArrayHandle` 타입 도입 또는 핸들 시스템 재설계

#### 2. Depth/Stencil Attachment 불일치 (OpenGLResourceManager.cpp:176)
```cpp
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTex->glId, 0);
```
**문제**: `Depth24Stencil8` 포맷인 경우 `GL_DEPTH_ATTACHMENT` 대신 `GL_DEPTH_STENCIL_ATTACHMENT`를 사용해야 합니다.
**영향**: Depth-Stencil 버퍼가 제대로 작동하지 않음
**수정 필요**: 텍스처 포맷에 따라 적절한 attachment 포인트 선택

#### 3. 리소스 파괴 순서 문제 (OpenGLResourceManager.cpp:439-477)
```cpp
void OpenGLResourceManager::DestroyAllResources()
{
    // Destroy all textures
    for (auto& pair : textures) { ... }
    
    // Destroy all framebuffers
    for (auto& pair : framebuffers) { ... }
    // ...
}
```
**문제**: 텍스처를 먼저 삭제한 후 프레임버퍼를 삭제합니다. 프레임버퍼가 텍스처를 참조하고 있을 경우 문제 발생.
**영향**: OpenGL 컨텍스트 오류 가능성
**수정 필요**: 프레임버퍼를 먼저 삭제한 후 텍스처 삭제

### 심각도: 중간 (Medium)

#### 4. 하드코딩된 정점 속성 (OpenGLResourceManager.cpp:259-270)
```cpp
// Position: vec3 (offset 0, stride vertexStride)
glEnableVertexAttribArray(0);
glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, vertexStride, (void*)0);

// UV: vec2 (offset 12, stride vertexStride)
glEnableVertexAttribArray(1);
glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, vertexStride, (void*)12);
```
**문제**: 정점 형식이 하드코딩되어 있어 유연하지 않습니다.
**영향**: 다른 정점 형식 지원 불가, 확장성 저하
**수정 필요**: 정점 형식 구조체 도입 및 유연한 속성 설정

#### 5. Finalize/Reset 로직 모호성 (OpenGLCommandList.cpp:248-261)
```cpp
void OpenGLCommandList::Finalize()
{
    if (isOpen) { Close(); }
    Reset(); // 다음 프레임을 위해 상태 초기화
}
```
**문제**: `Finalize()`가 프레임 종료와 다음 프레임 준비를 동시에 수행합니다.
**영향**: 책임 분명성 저하, 디버깅 어려움
**수정 필요**: 프레임 종료와 준비 분리

### 심각도: 낮음 (Low)

#### 6. null 포인터 체크 부족 (OpenGLResourceManager.cpp:155)
```cpp
OpenGLTexture* colorTex = GetTexture(colorTexture);
// colorTex가 null인지 체크하지 않고 사용
glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex->glId, 0);
```
**문제**: `CreateTexture` 실패 시 null 포인터 역참조 가능성
**영향**: 크래시 가능성
**수정 필요**: null 체크 추가

---

## 유지보수성 평가

### 평가 기준
1. **로직 얽힘 정도**: 코드 간의 의존성이 무비판적으로 얽혀있는지
2. **함수 집중도**: 필요 이상으로 함수가 한 파일에 모여있는지
3. **책임 분리**: 단일 책임 원칙 준수 여부
4. **확장성**: 새로운 기능 추가 용이성
5. **테스트 가능성**: 단위 테스트 작성 용이성

### 평가 결과

#### 1. 로직 얽힘 정도: **중간 (Medium)**

**문제점**:
- `OpenGLCommandList`가 `OpenGLResourceManager`에 강하게 의존
- 리소스 생성/파괴 로직이 ResourceManager와 CommandList 사이에 분산
- 핸들 타입 불일치로 인한 타입 캐스팅 필요

**긍정적 측면**:
- 명확한 인터페이스 분리 (CommandList 추상화)
- 리소스 관리가 ResourceManager로 캡슐화됨

**개선 제안**:
- 핸들 타입 시스템 재설계로 타입 안전성 강화
- 리소스 라이프사이클 관리를 한 곳으로 집중

#### 2. 함수 집중도: **양호 (Good)**

**분석**:
- `OpenGLResourceManager.cpp` (480줄): 4가지 리소스 타입 관리
- `OpenGLCommandList.cpp` (368줄): 명령 리스트 기능
- 각 파일이 명확한 책임을 가짐
- 함수 길이가 적절함 (대부분 20-50줄)

**개선 제안**:
- 정점 속성 설정을 별도 함수로 분리
- 셰이더 컴파일 로직을 별도 클래스로 분리 고려

#### 3. 책임 분리: **중간 (Medium)**

**문제점**:
- `OpenGLCommandList`가 리소스 관리자에 직접 접근
- 상태 관리와 명령 기록이 혼재
- 리소스 생성 파라미터가 여러 곳에 분산

**긍정적 측면**:
- ResourceManager가 리소스 라이프사이클 전담
- CommandList가 GPU 명령 추상화

**개선 제안**:
- CommandList에서 ResourceManager 직접 접근 제거
- 팩토리 패턴 도입으로 리소스 생성 캡슐화

#### 4. 확장성: **낮음 (Low)**

**문제점**:
- 정점 형식이 하드코딩됨
- 텍스처 파라미터가 고정됨
- 새로운 리소스 타입 추가 시 코드 수정 필요
- 셰이더 스테이지 확장 어려움

**긍정적 측면**:
- CommandList 인터페이스가 확장 가능
- ResourceManager 구조가 확장에 용이

**개선 제안**:
- 정점 형식 구조체 도입
- 텍스처 디스크립터 구조체 도입
- 플러그인 아키텍처 고려

#### 5. 테스트 가능성: **중간 (Medium)**

**문제점**:
- OpenGL 컨텍스트 의존성으로 단위 테스트 어려움
- 상태 의존성 (isOpen 플래그 등)
- 글로벌 OpenGL 상태에 의존

**긍정적 측면**:
- MockCommandList가 이미 존재
- 인터페이스가 잘 정의됨

**개선 제안**:
- OpenGL 래퍼 인터페이스 도입
- 의존성 주입 패턴 적용
- 상태less 함수로 리팩토링

---

## 종합 유지보수성 점수: **62/100**

### 상세 점수
- 로직 얽힘 정도: 15/25
- 함수 집중도: 20/25
- 책임 분리: 12/25
- 확장성: 8/25
- 테스트 가능성: 7/25

### 등급: **C (보통)**

**요약**: 코드 구조는 전반적으로 양호하지만, 확장성과 테스트 가능성이 부족합니다. 핸들 타입 시스템의 불일치와 하드코딩된 정점 형식이 주요 문제입니다. 즉시 수정이 필요한 버그가 3건 있으며, 중간 심각도의 개선 사항이 2건 있습니다.

---

## 우선 순위별 수정 계획

### 1단계: 심각도 높음 버그 수정 (즉시)
1. 핸들 타입 시스템 재설계
2. Depth/Stencil attachment 수정
3. 리소스 파괴 순서 수정

### 2단계: 심각도 중간 개선 (1주 내)
1. 정점 형식 구조체 도입
2. Finalize/Reset 로직 분리

### 3단계: 장기적 개선 (1개월 내)
1. 확장성 개선 (디스크립터 구조체)
2. 테스트 가능성 개선 (래퍼 인터페이스)
3. 책임 분리 개선 (팩토리 패턴)
