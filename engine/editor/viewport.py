import sys
import os
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import QTimer, Qt
from engine_binding import binding as ge_python

from style.theme import COLORS
from core.editor_camera import OrbitCamera

class EngineViewport(QWidget):
    def __init__(self, parent=None, editor_camera: bool = False):
        """editor_camera=True면 마우스로 조작하는 에디터 카메라(core/editor_camera.py)로 그린다
        (Scene 뷰포트). False면 ECS Main Camera를 따라가는 게임 카메라(Play 뷰포트)."""
        super().__init__(parent)
        self._orbit = OrbitCamera() if editor_camera else None
        self._use_editor_camera = editor_camera
        self._drag_mode = None          # 'orbit' | 'pan' | None
        self._last_mouse = None
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
            # RenderableComponent.meshPath("assets/models/...")의 기준 = engine/ 디렉터리.
            # cwd와 무관하게 이 파일(engine/editor/viewport.py) 위치에서 계산한다.
            config.assetRoot = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            
            # winId()는 HWND를 반환 (Windows 기준)
            hwnd = int(self.winId())
            try:
                self.engine.InitializeFromWindowHandle(hwnd, config)
                self.initialized = True
                self._apply_view_camera()
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

    # ── 에디터 카메라 ─────────────────────────────────────────────────────────
    @property
    def has_editor_camera(self) -> bool:
        return self._orbit is not None

    @property
    def uses_editor_camera(self) -> bool:
        return self._orbit is not None and self._use_editor_camera

    def set_use_editor_camera(self, use: bool):
        """뷰포트를 에디터 카메라(True)와 게임 카메라=ECS Main Camera(False) 사이에서 전환한다."""
        if self._orbit is None:
            return
        self._use_editor_camera = bool(use)
        self._apply_view_camera()

    def reset_editor_camera(self):
        if self._orbit is not None:
            self._orbit.reset()
            self._push_editor_camera()

    def _apply_view_camera(self):
        if not self.initialized:
            return
        self.engine.SetViewCamera(ge_python.ViewCamera.Editor if self.uses_editor_camera
                                  else ge_python.ViewCamera.Game)
        self._push_editor_camera()

    def _push_editor_camera(self):
        if self.initialized and self._orbit is not None:
            (ex, ey, ez), (tx, ty, tz) = self._orbit.view()
            self.engine.SetEditorCameraView(ex, ey, ez, tx, ty, tz)

    def _camera_drag_mode(self, event):
        button, mods = event.button(), event.modifiers()
        if button == Qt.MiddleButton or (button == Qt.RightButton and mods & Qt.ShiftModifier):
            return 'pan'
        if button == Qt.RightButton or (button == Qt.LeftButton and mods & Qt.AltModifier):
            return 'orbit'
        return None

    def wheelEvent(self, event):
        if self.uses_editor_camera:
            self._orbit.zoom(event.angleDelta().y() / 120.0)
            self._push_editor_camera()
            event.accept()
            return
        # 게임 카메라로 보는 중(Play 등)에는 엔진 입력으로 넘긴다(휠 한 칸 = 1.0, InputState.h).
        # 예전에는 휠 이벤트를 엔진에 보내지 않아서 게임 쪽 줌(CameraOrbitControlComponent)이 불가능했다.
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseWheel
            e.mouseWheelDelta = event.angleDelta().y() / 120.0
            self.engine.PushInputEvent(e)
            event.accept()
            return
        super().wheelEvent(event)

    def mouseMoveEvent(self, event):
        if self.uses_editor_camera and self._drag_mode and self._last_mouse is not None:
            pos = event.position()
            dx, dy = pos.x() - self._last_mouse.x(), pos.y() - self._last_mouse.y()
            self._last_mouse = pos
            if self._drag_mode == 'orbit':
                self._orbit.orbit(dx, dy)
            else:
                self._orbit.pan(dx, dy, self.height())
            self._push_editor_camera()
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseMove
            e.mouseX = event.x()
            e.mouseY = event.y()
            # 델타 계산은 엔진 내부에서 처리되거나 여기서 계산해서 전달 가능
            self.engine.PushInputEvent(e)

    def mousePressEvent(self, event):
        if self.uses_editor_camera:
            self._drag_mode = self._camera_drag_mode(event)
            self._last_mouse = event.position() if self._drag_mode else None
        self.setFocus()   # F 키(카메라 리셋)를 받으려면 포커스가 필요하다
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonDown
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = event.x()
            e.mouseY = event.y()
            self.engine.PushInputEvent(e)

    def mouseReleaseEvent(self, event):
        self._drag_mode = None
        self._last_mouse = None
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonUp
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = event.x()
            e.mouseY = event.y()
            self.engine.PushInputEvent(e)

    def keyPressEvent(self, event):
        if self.uses_editor_camera and event.key() == Qt.Key_F:
            self.reset_editor_camera()
        key = self._map_key(event.key())
        if self.initialized and key is not None and not event.isAutoRepeat():
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.KeyDown
            e.keyCode = key
            self.engine.PushInputEvent(e)

    def keyReleaseEvent(self, event):
        key = self._map_key(event.key())
        if self.initialized and key is not None and not event.isAutoRepeat():
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.KeyUp
            e.keyCode = key
            self.engine.PushInputEvent(e)

    def _map_mouse_button(self, qt_button):
        if qt_button == Qt.LeftButton: return ge_python.MouseButton.Left
        if qt_button == Qt.RightButton: return ge_python.MouseButton.Right
        if qt_button == Qt.MiddleButton: return ge_python.MouseButton.Middle
        return ge_python.MouseButton.Left

    _KEY_NAMES = None   # Qt 키 -> 엔진 KeyCode 이름 (처음 쓸 때 만든다)

    @classmethod
    def _key_table(cls):
        if cls._KEY_NAMES is None:
            table = {getattr(Qt, f"Key_{c}"): c for c in "ABCDEFGHIJKLMNOPQRSTUVWXYZ"}
            table.update({getattr(Qt, f"Key_{i}"): f"Num{i}" for i in range(10)})
            table.update({getattr(Qt, f"Key_F{i}"): f"F{i}" for i in range(1, 13)})
            table.update({
                Qt.Key_Escape: "Escape", Qt.Key_Space: "Space", Qt.Key_Return: "Enter", Qt.Key_Enter: "Enter",
                Qt.Key_Tab: "Tab", Qt.Key_Backspace: "Backspace", Qt.Key_Shift: "Shift",
                Qt.Key_Control: "Control", Qt.Key_Alt: "Alt",
                Qt.Key_Left: "Left", Qt.Key_Up: "Up", Qt.Key_Right: "Right", Qt.Key_Down: "Down",
            })
            cls._KEY_NAMES = table
        return cls._KEY_NAMES

    def _map_key(self, qt_key):
        """Qt 키 -> 엔진 KeyCode. 모르는 키는 None(엔진에 보내지 않음).

        예전에는 모르는 키를 전부 KeyCode.A로 보냈다. 그래서 Shift나 방향키를 눌러도 게임에는
        'A가 눌림'으로 전달됐다.
        """
        name = self._key_table().get(qt_key)
        if name is None:
            return None
        return getattr(ge_python.KeyCode, name, None)
