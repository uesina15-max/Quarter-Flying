# GUI 통합 실행 계획서
# GLFW → PySide6 단일 GUI 통합

**작성일**: 2026-07-28  
**프로젝트**: Quarter Flying Game Engine  
**목표**: GLFW 기반 메인 GUI 제거 및 PySide6 에디터로 단일 GUI 통합  
**예상 기간**: 4-6주  
**우선순위**: 높음

---

## 📋 목차

1. [개요](#1-개요)
2. [전제 조건](#2-전제-조건)
3. [Phase 1: 기술 검증 (1-2주)](#3-phase-1-기술-검증-1-2주)
4. [Phase 2: 기능 이전 (2-3주)](#4-phase-2-기능-이전-2-3주)
5. [Phase 3: 레거시 제거 (1주)](#5-phase-3-레거시-제거-1주)
6. [검증 및 롤백 계획](#6-검증-및-롤백-계획)
7. [리스크 관리](#7-리스크-관리)
8. [성공 기준](#8-성공-기준)

---

## 1. 개요

### 1.1 목적

현재 프로젝트의 이중 GUI 구조를 해소하고 PySide6 기반의 단일 GUI로 통합하여:
- 유지보수성 향상
- 데이터 일관성 보장
- 개발 효율성 증대
- 사용자 경험 개선

### 1.2 현재 상태

**기존 구조**:
- `engine/app/main.cpp`: GLFW + OpenGL 기반 메인 GUI
- `engine/editor/main.py`: PySide6 기반 에디터 GUI

**목표 구조**:
- `engine/editor/main.py`: PySide6 기반 단일 GUI (통합)
- `engine/app/main.cpp`: 레거시용으로 보존 또는 제거

### 1.3 통합 범위

**포함**:
- 엔진 렌더링 Qt 위젯 임베딩
- 데모 씬 기능 이전
- 플레이/정지 기능 통합
- 설정 파일 로직 통합

**제외**:
- Python 바인딩 완전 재작성 (기존 ge_python 활용)
- Qt 테마 완전 변경 (기존 테마 유지)
- 에디터 패널 구조 변경 (기존 구조 유지)

---

## 2. 전제 조건

### 2.1 기술 요구사항

- **Python**: 3.8+
- **PySide6**: 6.0+
- **ge_python**: 현재 바인딩 유지 및 확장
- **CMake**: 3.15+
- **C++ 컴파일러**: C++23 지원

### 2.2 인력 요구사항

- **C++ 개발자**: 1인 (ge_python 바인딩 확장)
- **Python/Qt 개발자**: 1인 (GUI 통합)
- **테스터**: 1인 (기능 검증)

### 2.3 환경 설정

```bash
# 필수 Python 패키지
pip install PySide6
pip install nlohmann_json  # 이미 설치됨

# 빌드 환경 확인
cmake --version
python --version
```

---

## 3. Phase 1: 기술 검증 (1-2주)

### 3.1 목표

Qt 위젯에 엔진 렌더링을 임베딩하는 기술적 가능성 검증

### 3.2 작업 목록

#### Task 1.1: ge_python 바인딩 확장 (3-4일)

**파일**: `engine/bindings/EngineBindings.cpp`

**작업 내용**:
```cpp
// EngineBindings.cpp에 추가
#include <PySide6/QtWidgets/QWidget>

// 윈도우 핸들 전달 기능 추가
namespace Engine {
    namespace Python {
        // Engine::InitializeFromWindowHandle 바인딩
        void bind_engine_from_window_handle(py::module& m) {
            m.def("initialize_from_window_handle", 
                [](Engine& engine, void* window_handle, const EngineConfig& config) {
                    return engine.InitializeFromWindowHandle(window_handle, config);
                },
                py::arg("engine"),
                py::arg("window_handle"), 
                py::arg("config"),
                "Initialize engine from existing window handle"
            );
        }
    }
}
```

**검증 방법**:
```python
# test_window_handle.py
import ge_python
from PySide6.QtWidgets import QWidget

widget = QWidget()
window_handle = widget.winId()

engine = ge_python.Engine()
config = ge_python.EngineConfig()
result = engine.initialize_from_window_handle(window_handle, config)

assert result.has_value(), "Engine initialization failed"
print("✓ Window handle binding test passed")
```

**완료 기준**:
- [ ] 바인딩 컴파일 성공
- [ ] Python 테스트 통과
- [ ] 메모리 누수 없음

#### Task 1.2: Qt 엔진 뷰포트 프로토타입 (2-3일)

**파일**: `engine/editor/qt_engine_viewport.py` (신규)

**작업 내용**:
```python
"""
editor/qt_engine_viewport.py

Qt 위젯에 엔진 렌더링을 임베딩하는 프로토타입 구현
"""

from PySide6.QtWidgets import QWidget, QVBoxLayout
from PySide6.QtCore import QTimer, Qt
import sys
import os

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False
    print("[QtEngineViewport] ge_python not found - running in dummy mode")


class QtEngineViewport(QWidget):
    """
    Qt 위젯에 엔진 렌더링을 임베딩하는 클래스
    """
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.engine = None
        self.world = None
        self.initialized = False
        
        # 레이아웃 설정
        self._setup_ui()
        
        # 엔진 초기화
        if HAS_ENGINE:
            self._init_engine()
        
        # 렌더링 타이머
        self.render_timer = QTimer(self)
        self.render_timer.timeout.connect(self._render_frame)
        self.render_timer.start(16)  # ~60fps
        
    def _setup_ui(self):
        """UI 초기화"""
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        
        if not HAS_ENGINE:
            # 더미 모드 표시
            from PySide6.QtWidgets import QLabel
            label = QLabel("Engine Integration Test\n(ge_python required)")
            label.setAlignment(Qt.AlignCenter)
            label.setStyleSheet("color: #334455; font-size: 14px;")
            layout.addWidget(label)
    
    def _init_engine(self):
        """엔진 초기화"""
        try:
            # Qt 윈도우 핸들 획득
            window_handle = int(self.winId())
            
            # 엔진 설정
            config = ge_python.EngineConfig()
            config.windowTitle = "Qt Engine Viewport"
            config.windowWidth = self.width()
            config.windowHeight = self.height()
            config.numWorkerThreads = 0
            config.logFrameInterval = 60
            
            # 엔진 생성 및 초기화
            self.engine = ge_python.Engine()
            result = self.engine.initialize_from_window_handle(window_handle, config)
            
            if result.has_value():
                self.initialized = True
                print("[QtEngineViewport] Engine initialized successfully")
            else:
                error = result.error()
                print(f"[QtEngineViewport] Engine initialization failed: {error.message}")
                
        except Exception as e:
            print(f"[QtEngineViewport] Engine initialization error: {e}")
    
    def _render_frame(self):
        """렌더링 프레임"""
        if self.engine and self.initialized:
            try:
                self.engine.tick_frame()
            except Exception as e:
                print(f"[QtEngineViewport] Render error: {e}")
    
    def resizeEvent(self, event):
        """리사이즈 이벤트 처리"""
        if self.engine and self.initialized:
            try:
                # 엔진에 리사이즈 통지
                platform = self.engine.get_platform()
                if platform:
                    # platform.on_resize 메서드가 있다고 가정
                    # 실제 구현은 ge_python 바인딩에 따라 다름
                    pass
            except Exception as e:
                print(f"[QtEngineViewport] Resize error: {e}")
        
        super().resizeEvent(event)
    
    def closeEvent(self, event):
        """종료 이벤트 처리"""
        if self.engine:
            try:
                self.engine.shutdown()
                print("[QtEngineViewport] Engine shutdown complete")
            except Exception as e:
                print(f"[QtEngineViewport] Shutdown error: {e}")
        
        super().closeEvent(event)


# 테스트용 메인 함수
if __name__ == "__main__":
    from PySide6.QtWidgets import QApplication
    
    app = QApplication(sys.argv)
    
    window = QtEngineViewport()
    window.setWindowTitle("Qt Engine Viewport Test")
    window.resize(1280, 720)
    window.show()
    
    sys.exit(app.exec())
```

**검증 방법**:
```bash
cd engine/editor
python qt_engine_viewport.py
```

**완료 기준**:
- [ ] Qt 위젯에 엔진 렌더링 표시
- [ ] 리사이즈 정상 작동
- [ ] 종료 시 엔진 정상 종료
- [ ] 메모리 누수 없음

#### Task 1.3: 기존 뷰포트와 호환성 검증 (2-3일)

**파일**: `engine/editor/viewport.py`

**작업 내용**:
```python
# viewport.py 수정 - 기존 EngineViewport와 새 QtEngineViewport 호환성 확인

from qt_engine_viewport import QtEngineViewport

# 기존 EngineViewport가 QtEngineViewport를 사용하도록 수정
class EngineViewport(QtEngineViewport):
    """
    기존 EngineViewport를 QtEngineViewport로 대체
    하위 호환성 유지
    """
    def __init__(self, parent=None):
        super().__init__(parent)
        # 추가적인 에디터 기능 유지
```

**검증 방법**:
```python
# test_viewport_compatibility.py
from editor.viewport import EngineViewport
from editor.main import EditorMainWindow

# 기존 에디터가 새 뷰포트로 정상 작동하는지 확인
app = EditorMainWindow()
app.show()
# 정상 작동 확인
```

**완료 기준**:
- [ ] 기존 에디터 정상 작동
- [ ] 뷰포트 기능 저하 없음
- [ ] 패널 간 통신 정상

### 3.3 Phase 1 완료 기준

- [ ] ge_python 바인딩 확장 완료
- [ ] Qt 엔진 뷰포트 프로토타입 작동
- [ ] 기존 에디터와 호환성 확인
- [ ] 기술 검증 보고서 작성

---

## 4. Phase 2: 기능 이전 (2-3주)

### 4.1 목표

GLFW 메인 GUI의 기능을 PySide6 에디터로 이전

### 4.2 작업 목록

#### Task 2.1: 데모 씬 이전 (3-4일)

**파일**: `engine/editor/demo_scene_integration.py` (신규)

**작업 내용**:
```python
"""
editor/demo_scene_integration.py

C++ DemoScene 기능을 Python으로 이전
"""

from PySide6.QtCore import QObject, Signal
import json
import os

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


class DemoSceneIntegration(QObject):
    """
    C++ DemoScene의 Python 래퍼
    """
    
    scene_loaded = Signal(object)  # 씬 로드 완료 시그널
    scene_error = Signal(str)     # 에러 시그널
    
    def __init__(self, parent=None):
        super().__init__(parent)
        self.scene_objects = []
        self.lights = []
        self.camera = None
        
    def load_scene(self, scene_path):
        """
        scene.json 로드
        """
        try:
            if not os.path.exists(scene_path):
                raise FileNotFoundError(f"Scene file not found: {scene_path}")
            
            with open(scene_path, 'r') as f:
                scene_data = json.load(f)
            
            # 오브젝트 로드
            if 'objects' in scene_data:
                self._load_objects(scene_data['objects'])
            
            # 라이트 로드
            if 'lights' in scene_data:
                self._load_lights(scene_data['lights'])
            
            self.scene_loaded.emit(scene_data)
            print(f"[DemoScene] Scene loaded: {scene_path}")
            
        except Exception as e:
            error_msg = f"Failed to load scene: {e}"
            self.scene_error.emit(error_msg)
            print(f"[DemoScene] {error_msg}")
    
    def _load_objects(self, objects_data):
        """오브젝트 데이터 로드"""
        self.scene_objects.clear()
        
        for obj_data in objects_data:
            scene_object = {
                'name': obj_data.get('name', 'unnamed'),
                'model': obj_data.get('model', ''),
                'material': obj_data.get('material', {}),
                'transform': obj_data.get('transform', {}),
                'instances': obj_data.get('instances', []),
                'grid': obj_data.get('grid', None)
            }
            self.scene_objects.append(scene_object)
    
    def _load_lights(self, lights_data):
        """라이트 데이터 로드"""
        self.lights.clear()
        
        for light_data in lights_data:
            light = {
                'position': light_data.get('position', [0, 0, 0]),
                'color': light_data.get('color', [1, 1, 1]),
                'intensity': light_data.get('intensity', 1.0)
            }
            self.lights.append(light)
    
    def get_scene_objects(self):
        """씬 오브젝트 반환"""
        return self.scene_objects
    
    def get_lights(self):
        """라이트 반환"""
        return self.lights
```

**파일**: `engine/editor/main.py` 수정

**작업 내용**:
```python
# main.py에 데모 씬 통합
from demo_scene_integration import DemoSceneIntegration

class EditorMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        # ... 기존 코드 ...
        
        # 데모 씬 통합
        self.demo_scene = DemoSceneIntegration()
        self.demo_scene.scene_loaded.connect(self._on_scene_loaded)
        self.demo_scene.scene_error.connect(self._on_scene_error)
        
        # 기본 씬 로드
        self._load_default_scene()
    
    def _load_default_scene(self):
        """기본 씬 로드"""
        scene_path = "assets/scene.json"
        if os.path.exists(scene_path):
            self.demo_scene.load_scene(scene_path)
    
    def _on_scene_loaded(self, scene_data):
        """씬 로드 완료 처리"""
        self.lbl_scene.setText(f"  {scene_data.get('name', 'Untitled Scene')}  ")
        
        # 씬 계층 패널 업데이트
        if hasattr(self, 'scene_panel'):
            self.scene_panel.load_from_scene_data(scene_data)
    
    def _on_scene_error(self, error_msg):
        """씬 로드 에러 처리"""
        QMessageBox.critical(self, "Scene Load Error", error_msg)
```

**검증 방법**:
```python
# test_demo_scene.py
from editor.demo_scene_integration import DemoSceneIntegration

demo = DemoSceneIntegration()
demo.load_scene("assets/scene.json")

assert len(demo.get_scene_objects()) > 0, "No objects loaded"
assert len(demo.get_lights()) > 0, "No lights loaded"
print("✓ Demo scene integration test passed")
```

**완료 기준**:
- [ ] scene.json 정상 로드
- [ ] 씬 계층 패널에 오브젝트 표시
- [ ] 라이트 정보 정상 표시
- [ ] 에러 처리 정상 작동

#### Task 2.2: 플레이/정지 기능 통합 (2-3일)

**파일**: `engine/editor/main.py` 수정

**작업 내용**:
```python
# main.py에 플레이 모드 통합
class EditorMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        # ... 기존 코드 ...
        
        self.is_playing = False
        self._setup_play_mode()
    
    def _setup_play_mode(self):
        """플레이 모드 설정"""
        # 플레이 모드 위젯
        self.play_mode_widget = QWidget()
        play_layout = QVBoxLayout(self.play_mode_widget)
        play_layout.setContentsMargins(0, 0, 0, 0)
        
        # 플레이 뷰포트
        self.play_viewport = QtEngineViewport()
        play_layout.addWidget(self.play_viewport)
        
        # 스택에 추가
        self.stack.addWidget(self.play_mode_widget)
    
    def _on_play(self):
        """플레이 버튼 처리"""
        if not self.is_playing:
            self.is_playing = True
            self.act_play.setChecked(True)
            
            # 플레이 모드로 전환
            self.stack.setCurrentWidget(self.play_mode_widget)
            
            # 엔진 플레이 시작
            if self.play_viewport.engine:
                self.play_viewport.engine.run()
            
            print("[Editor] Play mode started")
        else:
            self._on_pause()
    
    def _on_pause(self):
        """일시정지 처리"""
        self.is_playing = False
        self.act_play.setChecked(False)
        
        # 엔진 일시정지
        if self.play_viewport.engine:
            # 엔진 일시정지 기능 (구현 필요)
            pass
        
        print("[Editor] Paused")
    
    def _on_stop(self):
        """정지 처리"""
        self.is_playing = False
        self.act_play.setChecked(False)
        
        # 씬 에디터 모드로 복귀
        self.stack.setCurrentWidget(self.scene_editor_widget)
        
        # 엔진 정지
        if self.play_viewport.engine:
            self.play_viewport.engine.shutdown()
            # 엔진 재초기화
        
        print("[Editor] Stopped")
```

**검증 방법**:
1. 에디터에서 플레이 버튼 클릭
2. 플레이 모드로 전환 확인
3. 정지 버튼으로 복귀 확인

**완료 기준**:
- [ ] 플레이 모드 정상 전환
- [ ] 엔진 렌더링 플레이 모드에서 작동
- [ ] 정지 시 정상 복귀
- [ ] 상태 표시 정확

#### Task 2.3: 설정 파일 통합 (1-2일)

**파일**: `engine/editor/config_manager.py` (신규)

**작업 내용**:
```python
"""
editor/config_manager.py

config.json 로직 통합
"""

import json
import os


class ConfigManager:
    """
    설정 파일 관리자
    """
    
    def __init__(self, config_path="config.json"):
        self.config_path = config_path
        self.config = self._load_default_config()
        self.load_config()
    
    def _load_default_config(self):
        """기본 설정 로드"""
        return {
            "window": {
                "title": "Quarter Flying Editor",
                "width": 1600,
                "height": 900,
                "vsync": True
            },
            "paths": {
                "assetRoot": "./",
                "shaderRoot": "./assets/shaders/"
            },
            "engine": {
                "numWorkerThreads": 0,
                "logFrameInterval": 60
            }
        }
    
    def load_config(self):
        """설정 파일 로드"""
        if os.path.exists(self.config_path):
            try:
                with open(self.config_path, 'r') as f:
                    loaded_config = json.load(f)
                    self._merge_config(loaded_config)
                print(f"[Config] Loaded config from {self.config_path}")
            except Exception as e:
                print(f"[Config] Failed to load config: {e}")
    
    def _merge_config(self, loaded_config):
        """설정 병합"""
        def merge_dict(default, loaded):
            for key, value in loaded.items():
                if key in default and isinstance(default[key], dict) and isinstance(value, dict):
                    merge_dict(default[key], value)
                else:
                    default[key] = value
        
        merge_dict(self.config, loaded_config)
    
    def save_config(self):
        """설정 파일 저장"""
        try:
            with open(self.config_path, 'w') as f:
                json.dump(self.config, f, indent=4)
            print(f"[Config] Saved config to {self.config_path}")
        except Exception as e:
            print(f"[Config] Failed to save config: {e}")
    
    def get(self, *keys, default=None):
        """설정 값 가져오기"""
        value = self.config
        for key in keys:
            if isinstance(value, dict) and key in value:
                value = value[key]
            else:
                return default
        return value
    
    def set(self, *keys, value):
        """설정 값 설정"""
        config = self.config
        for key in keys[:-1]:
            if key not in config:
                config[key] = {}
            config = config[key]
        config[keys[-1]] = value
```

**파일**: `engine/editor/main.py` 수정

**작업 내용**:
```python
# main.py에 설정 관리자 통합
from config_manager import ConfigManager

class EditorMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        
        # 설정 관리자
        self.config_manager = ConfigManager()
        
        # 설정 적용
        self._apply_config()
        
        # ... 기존 코드 ...
    
    def _apply_config(self):
        """설정 적용"""
        # 윈도우 설정
        window_title = self.config_manager.get("window", "title", default="Quarter Flying Editor")
        window_width = self.config_manager.get("window", "width", default=1600)
        window_height = self.config_manager.get("window", "height", default=900)
        
        self.setWindowTitle(window_title)
        self.resize(window_width, window_height)
        
        # 엔진 설정
        asset_root = self.config_manager.get("paths", "assetRoot", default="./")
        shader_root = self.config_manager.get("paths", "shaderRoot", default="./assets/shaders/")
        
        # PathResolver 초기화
        try:
            import ge_python
            ge_python.PathResolver.Init(asset_root, shader_root)
        except ImportError:
            print("[Editor] ge_python not available for PathResolver")
    
    def closeEvent(self, event):
        """종료 시 설정 저장"""
        self.config_manager.save_config()
        super().closeEvent(event)
```

**검증 방법**:
```python
# test_config_manager.py
from editor.config_manager import ConfigManager

config = ConfigManager("test_config.json")
config.set("window", "width", 1920)
config.save_config()

config2 = ConfigManager("test_config.json")
assert config2.get("window", "width") == 1920
print("✓ Config manager test passed")

# 테스트 파일 정리
import os
os.remove("test_config.json")
```

**완료 기준**:
- [ ] config.json 정상 로드
- [ ] 설정 변경 시 저장
- [ ] 기본값 정상 작동
- [ ] 엔진 PathResolver 연동

### 4.3 Phase 2 완료 기준

- [ ] 데모 씬 기능 완전 이전
- [ ] 플레이/정지 기능 통합 완료
- [ ] 설정 파일 통합 완료
- [ ] 기능 이전 검증 보고서 작성

---

## 5. Phase 3: 레거시 제거 (1주)

### 5.1 목표

GLFW 기반 메인 GUI 제거 및 정리

### 5.2 작업 목록

#### Task 3.1: CMakeLists.txt 수정 (1-2일)

**파일**: `engine/CMakeLists.txt`

**작업 내용**:
```cmake
# 기존 main.cpp 실행 파일 제거 또는 조건부
# option(BUILD_LEGACY_MAIN "Build legacy GLFW main" OFF)

# if(BUILD_LEGACY_MAIN)
#     # Legacy main executable (debugging/testing only)
#     add_executable(legacy_main app/main.cpp app/DemoScene.cpp)
#     target_link_libraries(legacy_main PRIVATE ge_engine)
# endif()

# Python 에디터를 메인 실행 파일로 설정
install(TARGETS ge_python DESTINATION .)
install(DIRECTORY editor/ DESTINATION editor)
install(DIRECTORY assets/ DESTINATION assets)

# 시작 스크립트 추가
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/run_editor.bat.in
    ${CMAKE_CURRENT_BINARY_DIR}/run_editor.bat
    @ONLY
)
```

**파일**: `engine/run_editor.bat.in` (신규)

**작업 내용**:
```batch
@echo off
cd /d "@CMAKE_CURRENT_BINARY_DIR@"
python editor/main.py
pause
```

**검증 방법**:
```bash
cd engine/build
cmake --build . --config Release
./run_editor.bat
```

**완료 기준**:
- [ ] CMake 정상 구성
- [ ] 레거시 main.cpp 빌드 제외
- [ ] 에디터 시작 스크립트 작동
- [ ] 설치 타겟 정상 작동

#### Task 3.2: GLFW 의존성 최소화 (1-2일)

**파일**: `engine/CMakeLists.txt`

**작업 내용**:
```cmake
# GLFW를 optional로 변경
# find_package(glfw3 QUIET)
# if(NOT glfw3_FOUND)
#     message(WARNING "GLFW not found - editor mode only")
# endif()

# 플랫폼 의존성 조정
# GLFW가 없어도 에디터는 작동하도록 수정
```

**검증 방법**:
```bash
# GLFW 없이 빌드 테스트
cmake -DGLFW_ROOT=OFF ..
cmake --build .
```

**완료 기준**:
- [ ] GLFW 없이 빌드 성공
- [ ] 에디터 정상 작동
- [ ] 레거시 코드 분리 명확

#### Task 3.3: 문서 및 스크립트 업데이트 (1-2일)

**파일**: `README.md`

**작업 내용**:
```markdown
# Quarter Flying Game Engine

## 시작 방법

### 에디터 모드 (기본)
```bash
cd engine
python editor/main.py
```

### 레거시 모드 (디버깅용)
```bash
cd engine/build
./legacy_main  # BUILD_LEGACY_MAIN=ON 시에만
```

## 빌드 방법
```bash
cd engine
mkdir build && cd build
cmake ..
cmake --build .
```
```

**파일**: `scripts/start_editor.bat` (신규)

**작업 내용**:
```batch
@echo off
cd /d "%~dp0\engine"
python editor/main.py
```

**파일**: `scripts/start_editor.sh` (신규)

**작업 내용**:
```bash
#!/bin/bash
cd "$(dirname "$0")/engine"
python editor/main.py
```

**검증 방법**:
```bash
# 시작 스크립트 테스트
cd scripts
./start_editor.bat  # Windows
./start_editor.sh   # Linux/Mac
```

**완료 기준**:
- [ ] README 업데이트 완료
- [ ] 시작 스크립트 작동
- [ ] 문서 일관성 확보

#### Task 3.4: 백업 및 롤백 준비 (1일)

**작업 내용**:
```bash
# 레거시 코드 백업
cd engine
git add app/main.cpp
git commit -m "Backup: Legacy GLFW main before removal"

# 레거시 분기 생성
git branch legacy_glfw_main
git push origin legacy_glfw_main

# 복구 스크립트 작성
# scripts/restore_legacy.sh
```

**파일**: `scripts/restore_legacy.sh` (신규)

**작업 내용**:
```bash
#!/bin/bash
echo "Restoring legacy GLFW main..."
git checkout legacy_glfw_main -- app/main.cpp
echo "Legacy main restored. Please rebuild with BUILD_LEGACY_MAIN=ON"
```

**검증 방법**:
```bash
# 복구 스크립트 테스트
./scripts/restore_legacy.sh
# 복구 확인
```

**완료 기준**:
- [ ] 레거시 코드 백업 완료
- [ ] 복구 스크립트 작동
- [ ] 롤백 절차 문서화

### 5.3 Phase 3 완료 기준

- [ ] CMakeLists 수정 완료
- [ ] GLFW 의존성 최소화
- [ ] 문서 업데이트 완료
- [ ] 백업 및 복구 준비 완료

---

## 6. 검증 및 롤백 계획

### 6.1 검증 계획

#### 6.1.1 기능 검증 체크리스트

**기본 기능**:
- [ ] 에디터 정상 시작
- [ ] 씬 로드/저장
- [ ] 오브젝트 조작
- [ ] 플레이/정지
- [ ] 설정 저장/로드

**렌더링 기능**:
- [ ] 엔진 뷰포트 정상 렌더링
- [ ] 리사이즈 반영
- [ ] 셰이더 재로드
- [ ] 성능 저하 없음

**호환성 기능**:
- [ ] 기존 에디터 패널 작동
- [ ] Python 바인딩 호환
- [ ] 데이터 일관성

#### 6.1.2 성능 검증

**측정 항목**:
- 시작 시간: 기존 대비 ±10% 이내
- 메모리 사용: 기존 대비 ±20% 이내
- 렌더링 FPS: 기존 대비 ±5% 이내
- 응답 시간: UI 지연 없음

**측정 방법**:
```python
# performance_test.py
import time
import psutil
import os

def measure_startup_time():
    start = time.time()
    # 에디터 시작
    # ...
    end = time.time()
    return end - start

def measure_memory_usage():
    process = psutil.Process(os.getpid())
    return process.memory_info().rss / 1024 / 1024  # MB

def measure_rendering_fps():
    # FPS 측정
    # ...
    pass
```

#### 6.1.3 사용자 테스트

**테스트 시나리오**:
1. 에디터 시작 및 기본 탐색
2. 씬 로드 및 오브젝트 확인
3. 플레이 모드 테스트
4. 설정 변경 및 저장
5. 에디터 종료

**피드백 수집**:
- 사용성 평가
- 버그 리포트
- 성능 체감

### 6.2 롤백 계획

#### 6.2.1 롤백 기준

다음 경우 롤백 고려:
- 치명적 버그 발생
- 성능 저하 30% 이상
- 호환성 문제 해결 불가
- 사용자 불만도 높음

#### 6.2.2 롤백 절차

**1단계: 즉시 롤백 (긴급)**
```bash
# 복구 스크립트 실행
./scripts/restore_legacy.sh

# 레거시 모드로 빌드
cd engine/build
cmake -DBUILD_LEGACY_MAIN=ON ..
cmake --build .
```

**2단계: 안정화 롤백 (계획)**
```bash
# 이전 커밋으로 체크아웃
git checkout previous_stable_commit

# 재빌드
cd engine/build
cmake ..
cmake --build .
```

**3단계: 부분 롤백 (선택)**
- 특정 기능만 롤백
- 하이브리드 모드 운영

#### 6.2.3 롤백 후 조치

1. 원인 분석 보고서 작성
2. 개선 계획 수립
3. 재시작 여부 결정
4. 이해관계자 통보

---

## 7. 리스크 관리

### 7.1 기술적 리스크

#### 리스크 1: Qt 엔진 임베딩 실패
- **확률**: 중간 (30%)
- **영향**: 높음
- **완화 조치**:
  - Phase 1에서 철저한 기술 검증
  - 대안 방안 (OpenGL 위젯 직접 사용) 준비
  - 전문가 컨설팅

#### 리스크 2: 성능 저하
- **확률**: 중간 (25%)
- **영향**: 중간
- **완화 조치**:
  - 성능 프로파일링 도구 활용
  - 최적화 단계 별도 마련
  - 하드웨어 가속 활용

#### 리스크 3: Python 바인딩 호환성
- **확률**: 낮음 (15%)
- **영향**: 높음
- **완화 조치**:
  - 점진적 바인딩 확장
  - 테스트 커버리지 확대
  - 레거시 인터페이스 유지

### 7.2 프로젝트 리스크

#### 리스크 4: 일정 지연
- **확률**: 중간 (40%)
- **영향**: 중간
- **완화 조치**:
  - 버퍼 시간 확보 (20%)
  - 우선순위 동적 조정
  - 병렬 작업 최대화

#### 리스크 5: 인력 부족
- **확률**: 낮음 (10%)
- **영향**: 높음
- **완화 조치**:
  - 교차 훈련 진행
  - 외부 전문가 활용
  - 문서화 철저

### 7.3 비즈니스 리스크

#### 리스크 6: 사용자 저항
- **확률**: 중간 (35%)
- **영향**: 중간
- **완화 조치**:
  - 베타 테스터 모집
  - 튜토리얼 제공
  - 피드백 반영

---

## 8. 성공 기준

### 8.1 기술적 성공 기준

- [ ] Qt 엔진 뷰포트 안정적 작동
- [ ] 기능 이전 100% 완료
- [ ] 성능 저하 10% 이내
- [ ] 버그 0개 (치명적)

### 8.2 사용자 경험 성공 기준

- [ ] 시작 시간 3초 이내
- [ ] UI 반응성 100ms 이내
- [ ] 학습 곡선 완만
- [ ] 사용자 만족도 80% 이상

### 8.3 프로젝트 성공 기준

- [ ] 일정 준수 (±1주)
- [ ] 예산 준수
- [ ] 문서화 완료
- [ ] 지식 전달 완료

---

## 9. 다음 단계

### 9.1 즉시 조치 (이번 주)

1. **기술 검증 시작**
   - ge_python 바인딩 확장 착수
   - Qt 엔진 뷰포트 프로토타입 개발

2. **팀 준비**
   - 기술 워크샵 개최
   - 역할 분담 확정
   - 개발 환경 설정

3. **이해관자자 동의**
   - 계획서 승인
   - 리소스 할당
   - 일정 확정

### 9.2 중기 조치 (다음 주)

1. **Phase 1 착수**
   - 기술 검증 본격 시작
   - 주간 진행 회의

2. **모니터링 시스템**
   - 진행 상황 추적
   - 리스크 모니터링
   - 품질 지표 측정

### 9.3 장기 조치 (다음 달)

1. **Phase 2-3 착수**
   - 기능 이전 시작
   - 레거시 제거 준비

2. **성과 측정**
   - 중간 평가
   - 피드백 수집
   - 계획 조정

---

## 10. 부록

### 10.1 용어 사전

- **Qt 위젯**: Qt 프레임워크의 UI 컴포넌트
- **임베딩**: 한 시스템 안에 다른 시스템을 포함
- **ge_python**: C++ 엔진의 Python 바인딩
- **레거시**: 이전 버전의 시스템

### 10.2 참고 문서

- Qt 공식 문서: https://doc.qt.io/
- PySide6 문서: https://doc.qt.io/forpython/
- 프로젝트 아키텍처: `ARCHITECTURE_KO.md`
- 기존 문제 분석: `PROBLEM_ANALYSIS_REPORT.md`

### 10.3 연락처

- **프로젝트 리드**: [이름/이메일]
- **기술 리드**: [이름/이메일]
- **QA 리드**: [이름/이메일]

---

**문서 버전**: 1.0  
**최종 수정**: 2026-07-28  
**승인자**: [승인자 이름/직함]  
**다음 검토**: [검토 일자]
