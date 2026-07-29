"""
editor/qt_engine_viewport.py

Qt 위젯에 엔진 렌더링을 임베딩하는 프로토타입 구현
"""

from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel
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
            result = self.engine.InitializeFromWindowHandle(window_handle, config)
            
            if result:
                self.initialized = True
                print("[QtEngineViewport] Engine initialized successfully")
            else:
                print(f"[QtEngineViewport] Engine initialization failed")
                
        except Exception as e:
            print(f"[QtEngineViewport] Engine initialization error: {e}")
    
    def _render_frame(self):
        """렌더링 프레임"""
        if self.engine and self.initialized:
            try:
                self.engine.TickFrame()
            except Exception as e:
                print(f"[QtEngineViewport] Render error: {e}")
    
    def resizeEvent(self, event):
        """리사이즈 이벤트 처리"""
        if self.engine and self.initialized:
            try:
                # 엔진에 리사이즈 통지
                platform = self.engine.GetPlatform()
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
                self.engine.Shutdown()
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
