import sys
import os
from PySide6.QtWidgets import QWidget
from PySide6.QtCore import QEvent, QTimer, Qt, Signal
from engine_binding import binding as ge_python

from style.theme import COLORS
from core.editor_camera import OrbitCamera

class EngineViewport(QWidget):
    frame_profile = Signal(object)
    profile_enabled = False
    # 키보드 포커스를 얻거나 잃었다(Play 화면의 "클릭하면 입력 받음" 안내가 쓴다, E-3).
    focus_changed = Signal(bool)

    def __init__(self, parent=None, editor_camera: bool = False):
        """editor_camera=True면 마우스로 조작하는 에디터 카메라(core/editor_camera.py)로 그린다
        (Scene 뷰포트). False면 ECS Main Camera를 따라가는 게임 카메라(Play 뷰포트)."""
        super().__init__(parent)
        self._orbit = OrbitCamera() if editor_camera else None
        self._use_editor_camera = editor_camera
        self._drag_mode = None          # 'orbit' | 'pan' | None
        self._last_mouse = None
        # 좌클릭 선택·드래그 이동(core/viewport_manipulation.py). main.py가 엔진 연결 뒤 넣고, 켜질 조건(Edit 모드의
        # Scene Editor 화면)도 main이 정한다 - Play 중 좌클릭은 게임 입력이고 Motion Editor에는 씬 오브젝트가 없다.
        self.manipulator = None
        self.manipulation_enabled = lambda: False
        self._manipulating = False
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
            config.windowWidth = self._to_pixels(self.width())
            config.windowHeight = self._to_pixels(self.height())
            # RenderableComponent.meshPath("assets/models/...")의 기준 = 현재 프로젝트 폴더
            # (샘플 프로젝트면 engine/). 프로젝트를 바꾸면 Engine.SetAssetRoot로 갱신한다.
            from project import current_project
            config.assetRoot = str(current_project().root)
            
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
            if self.profile_enabled:
                self.frame_profile.emit(self.engine.GetFrameCosts())

    def _to_pixels(self, logical) -> int:
        """Qt 논리 좌표 -> 실제 화면 픽셀. 엔진은 픽셀로 일한다(glViewport, 게임 UI 배치, 기즈모 잡기 허용 거리).

        증상: 디스플레이 배율이 150%면 3D 화면과 게임 UI가 위젯의 왼쪽 아래 2/3에만 그려지고 나머지는 빈 영역이었다.
        클릭 위치도 그만큼 어긋났다. 에러 없음. 배율 100%에서는 두 좌표가 같아서 드러나지 않았다
        (QT_SCALE_FACTOR=1.5로 재현). 이 변환은 엔진으로 넘어가는 크기·마우스 좌표에만 쓴다.
        """
        return round(logical * self.devicePixelRatioF())

    def _push_window_size(self):
        e = ge_python.InputEvent()
        e.type = ge_python.InputEventType.WindowResize
        e.windowWidth = self._to_pixels(self.width())
        e.windowHeight = self._to_pixels(self.height())
        self.engine.PushInputEvent(e)

    def event(self, event):
        # 배율이 다른 모니터로 창을 옮기면 논리 크기는 그대로라 resizeEvent가 오지 않을 수 있다.
        if event.type() == QEvent.Type.DevicePixelRatioChange and self.initialized:
            self._push_window_size()
        return super().event(event)

    def resizeEvent(self, event):
        super().resizeEvent(event)
        if self.initialized:
            # 엔진 쪽 glViewport/카메라 종횡비가 창 크기 변화를 전혀 모르고 있었다 - 특히
            # Motion Editor로 전환할 때처럼 Qt가 이 위젯을 다른(크기가 다른) 레이아웃으로
            # 재부모 이동시키는 경우 반드시 필요하다(그렇지 않으면 예전 크기 기준으로 그려져
            # 화면이 검게 보이거나 잘린다). PushInputEvent는 스레드 세이프한 비동기 큐라서
            # 여기서 바로 불러도 안전하다.
            self._push_window_size()

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

    def editor_camera_view(self):
        """(target, distance, yaw, pitch) 또는 에디터 카메라가 없으면 None. 프로젝트 세션(project/session.py)이 저장한다."""
        if self._orbit is None:
            return None
        return list(self._orbit.target), self._orbit.distance, self._orbit.yaw, self._orbit.pitch

    def set_editor_camera_view(self, target, distance: float, yaw: float, pitch: float):
        """세션에서 에디터 카메라를 복원한다. 엔진 초기화 전에 불러도 된다 - 초기화 때(showEvent) 이 값을 보낸다."""
        if self._orbit is None:
            return
        self._orbit.target = [float(v) for v in target]
        self._orbit.distance = float(distance)
        self._orbit.yaw = float(yaw)
        self._orbit.pitch = float(pitch)
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
        if self._manipulating:
            pos = event.position()
            self.manipulator.move(*self._ndc(pos), (pos.x(), pos.y()), bool(event.modifiers() & Qt.ShiftModifier))
        elif (event.buttons() == Qt.NoButton and self.initialized and self.manipulator is not None
              and self.manipulation_enabled()):
            self.manipulator.hover(*self._ndc(event.position()))   # 기즈모 화살표 강조
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseMove
            e.mouseX = self._to_pixels(event.position().x())
            e.mouseY = self._to_pixels(event.position().y())
            # 델타 계산은 엔진 내부에서 처리되거나 여기서 계산해서 전달 가능
            self.engine.PushInputEvent(e)

    def mousePressEvent(self, event):
        if self.uses_editor_camera:
            self._drag_mode = self._camera_drag_mode(event)
            self._last_mouse = event.position() if self._drag_mode else None
        self.setFocus()   # F 키(카메라 리셋)를 받으려면 포커스가 필요하다
        if (event.button() == Qt.LeftButton and not event.modifiers() & (Qt.AltModifier | Qt.ControlModifier)
                and self.initialized and self.manipulator is not None and self.manipulation_enabled()):
            pos = event.position()
            try:
                self.manipulator.press(*self._ndc(pos), (pos.x(), pos.y()))
                self._manipulating = True
            except Exception as e:
                print(f"[Viewport] 선택 실패: {e}")
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonDown
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = self._to_pixels(event.position().x())
            e.mouseY = self._to_pixels(event.position().y())
            self.engine.PushInputEvent(e)

    def mouseReleaseEvent(self, event):
        self._drag_mode = None
        self._last_mouse = None
        if self._manipulating and event.button() == Qt.LeftButton:
            self._end_manipulation()
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.MouseButtonUp
            e.mouseButton = self._map_mouse_button(event.button())
            e.mouseX = self._to_pixels(event.position().x())
            e.mouseY = self._to_pixels(event.position().y())
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

    def focusInEvent(self, event):
        super().focusInEvent(event)
        self.focus_changed.emit(True)

    def focusOutEvent(self, event):
        # 포커스가 다른 위젯/창으로 가면 눌린 키의 keyReleaseEvent가 이 위젯으로 오지 않는다.
        # 예전에는 아무것도 하지 않아서, W를 누른 채 Hierarchy를 클릭하면 엔진에는 W가 계속 눌린
        # 상태로 남았다(Play 중이면 캐릭터·카메라가 계속 움직임). 상태 정리는 엔진 한 곳
        # (Engine::HandleFocusLost)에서 하고, 여기서는 이벤트만 넣는다 - Player 창의 WM_KILLFOCUS와 같은 경로.
        super().focusOutEvent(event)
        self.focus_changed.emit(False)
        if self._manipulating:
            self._end_manipulation()   # 놓기 이벤트가 오지 않는다 - 드래그(병합 세션)를 여기서 닫는다
        if self.initialized:
            e = ge_python.InputEvent()
            e.type = ge_python.InputEventType.FocusLost
            self.engine.PushInputEvent(e)

    def _ndc(self, pos):
        """위젯 좌표 -> 정규화 장치 좌표(-1..1, y 위가 +). 비율이라 DPI 배율과 무관하다."""
        w, h = max(self.width(), 1), max(self.height(), 1)
        return 2.0 * pos.x() / w - 1.0, 1.0 - 2.0 * pos.y() / h

    def _end_manipulation(self):
        self._manipulating = False
        try:
            self.manipulator.release()
        except Exception as e:
            print(f"[Viewport] 드래그 정리 실패: {e}")

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
