# 레거시 백업 정보

**작성일**: 2026-07-28  
**목적**: GLFW 기반 메인 GUI 제거 전 백업 정보

---

## 백업 대상 파일

### 1. 레거시 메인 애플리케이션

**파일**:
- `engine/app/main.cpp` - GLFW 기반 메인 애플리케이션
- `engine/app/DemoScene.cpp` - 데모 씬 구현

**백업 방법**:
```bash
# Git을 사용하는 경우
git add engine/app/main.cpp engine/app/DemoScene.cpp
git commit -m "Backup: Legacy GLFW main before GUI integration removal"

# Git을 사용하지 않는 경우
mkdir -p backup/legacy_main
cp engine/app/main.cpp backup/legacy_main/
cp engine/app/DemoScene.cpp backup/legacy_main/
```

---

## 복구 절차

### Git 사용 시

```bash
# 복구 스크립트 실행
./scripts/restore_legacy.sh  # Linux/Mac
scripts\restore_legacy.bat   # Windows

# 또는 수동 복구
git checkout HEAD -- engine/app/main.cpp engine/app/DemoScene.cpp
```

### Git 미사용 시

```bash
# 백업에서 복구
cp backup/legacy_main/main.cpp engine/app/
cp backup/legacy_main/DemoScene.cpp engine/app/
```

---

## 재빌드 방법

### 레거시 모드로 빌드

```bash
cd engine/build
cmake -DBUILD_LEGACY_MAIN=ON ..
cmake --build .
```

### 일반 모드로 빌드 (Python 에디터)

```bash
cd engine/build
cmake ..
cmake --build .
```

---

## 롤백 기준

다음 경우 롤백 고려:
- Python 에디터 치명적 버그
- 성능 저하 30% 이상
- 호환성 문제 해결 불가
- 사용자 불만도 높음

---

## 연락처

복구에 문제가 있을 경우:
- 기술 리드: [연락처]
- 프로젝트 리드: [연락처]
