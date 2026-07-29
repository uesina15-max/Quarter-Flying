import sys
import os
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import QTimer, Qt
import ge_python

class EngineViewport(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMouseTracking(True)
        
        self.engine = ge_python.Engine()
        self.initialized = False
        
        # 틱 타이머 (약 60 FPS)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.tick)
        self.timer.start(16)

    def showEvent(self, event):
        if not self.initialized:
            config = ge_python.EngineConfig()
            config.windowWidth = self.width()
            config.windowHeight = self.height()
            
            # winId()는 HWND를 반환 (Windows 기준)
            hwnd = int(self.winId())
            try:
                self.engine.InitializeFromWindowHandle(hwnd, config)
                self.initialized = True
                print(f"Engine Viewport initialized on HWND: {hwnd}")
            except Exception as e:
                print(f"[Error] Engine initialization failed on HWND {hwnd}: {e}")
                self.initialized = False
                # Fallback 상태 표시
                self.setStyleSheet("background-color: #2b1111; border: 2px solid #ff4444;")

    def tick(self):
        if self.initialized:
            self.engine.TickFrame()

    def mouseMoveEvent(self, event):
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseMove
            e.mouseX = event.x()
            e.mouseY = event.y()
            # 델타 계산은 엔진 내부에서 처리되거나 여기서 계산해서 전달 가능
            self.engine.PushInputEvent(e)

    def mousePressEvent(self, event):
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonDown
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = event.x()
            e.mouseY = event.y()
            self.engine.PushInputEvent(e)

    def mouseReleaseEvent(self, event):
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonUp
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = event.x()
            e.mouseY = event.y()
            self.engine.PushInputEvent(e)

    def keyPressEvent(self, event):
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.KeyDown
            e.keyCode = self._map_key(event.key())
            self.engine.PushInputEvent(e)

    def keyReleaseEvent(self, event):
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.KeyUp
            e.keyCode = self._map_key(event.key())
            self.engine.PushInputEvent(e)

    def _map_mouse_button(self, qt_button):
        if qt_button == Qt.LeftButton: return ge_python.MouseButton.Left
        if qt_button == Qt.RightButton: return ge_python.MouseButton.Right
        if qt_button == Qt.MiddleButton: return ge_python.MouseButton.Middle
        return ge_python.MouseButton.Left

    def _map_key(self, qt_key):
        # 복잡한 매핑은 나중에 확장
        if qt_key == Qt.Key_A: return ge_python.KeyCode.A
        if qt_key == Qt.Key_W: return ge_python.KeyCode.W
        if qt_key == Qt.Key_S: return ge_python.KeyCode.S
        if qt_key == Qt.Key_D: return ge_python.KeyCode.D
        if qt_key == Qt.Key_Escape: return ge_python.KeyCode.Escape
        return ge_python.KeyCode.A # Default fallback
