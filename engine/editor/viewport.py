import sys
import os
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import QTimer, Qt
from engine_binding import binding as ge_python

from style.theme import COLORS

class EngineViewport(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFocusPolicy(Qt.StrongFocus)
        self.setMouseTracking(True)

        # 네이티브 렌더링 서페이스(HWND) 임베딩 위젯: Qt가 이 위젯 위에 자체 배경을
        # 다시 그리지 않도록 한다. 이게 없으면 전역 스타일시트(QWidget { background-color })가
        # 매 paint 이벤트마다 엔진이 그린 프레임 위를 패널 배경색으로 덮어써서
        # 실제로는 렌더링이 되고 있어도 항상 검은(패널색) 화면으로만 보인다.
        self.setAttribute(Qt.WA_NativeWindow, True)
        self.setAttribute(Qt.WA_PaintOnScreen, True)
        self.setAttribute(Qt.WA_NoSystemBackground, True)
        self.setAutoFillBackground(False)

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
                self.setStyleSheet(f"background-color: {COLORS['bg_base']}; border: 2px solid {COLORS['accent_danger']};")

    def tick(self):
        if self.initialized:
            self.engine.TickFrame()

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.initialized:
            # 엔진 쪽 glViewport/카메라 종횡비가 창 크기 변화를 전혀 모르고 있었다 - 특히
            # Motion Editor로 전환할 때처럼 Qt가 이 위젯을 다른(크기가 다른) 레이아웃으로
            # 재부모 이동시키는 경우 반드시 필요하다(그렇지 않으면 예전 크기 기준으로 그려져
            # 화면이 검게 보이거나 잘린다). PushInputEvent는 스레드 세이프한 비동기 큐라서
            # 여기서 바로 불러도 안전하다.
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.WindowResize
            e.windowWidth = self.width()
            e.windowHeight = self.height()
            self.engine.PushInputEvent(e)

    def paintEngine(self):
        # Qt의 QPainter 기반 페인트 엔진을 비활성화 — 렌더링은 네이티브 HWND에 엔진이
        # 직접 그린다 (WA_PaintOnScreen과 짝을 이루는 표준 패턴).
        return None

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
