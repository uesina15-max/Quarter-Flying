"""
Quarter Flying Editor — Main Window
3분할 레이아웃: 씬 계층 뷰 | 3D 뷰포트 | 인스펙터

Stage 1: 더미 데이터로 동작 (엔진 모듈 없어도 시작 가능)
Stage 2: 엔진 모듈 연결 시 실제 ECS 연동
"""

import sys
import os
import time
import json

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QSplitter,
    QVBoxLayout, QHBoxLayout, QLabel, QToolBar,
    QSizePolicy, QMenuBar, QMenu, QStatusBar, QStackedWidget, QPushButton,
    QDockWidget
)
from PySide6.QtCore import Qt, QTimer, QSize
from PySide6.QtGui import QAction, QFont, QIcon, QKeySequence

# 패널 임포트
from panels.scene_hierarchy import SceneHierarchyPanel
from panels.inspector import InspectorPanel
from panels.status_bar import StatusBar
from panels.prefab_browser import PrefabBrowserPanel
from style.theme import apply_theme, apply_dark_title_bar, COLORS

# 데모 씬 통합 임포트
from demo_scene_integration import DemoSceneIntegration

# 설정 관리자 임포트
from config_manager import ConfigManager
from core.scene_action_playback import ScenePlaybackController
from demo_scene_seed import seed_demo_scene
from core.editor_camera import look_rotation_quaternion, forward_from_quaternion, quaternion_from_euler_degrees
from scene_instantiation import expand_scene_objects, instantiate_scene

# 모션 에디터 임포트
try:
    from motion_editor import MotionEditorWidget
    HAS_MOTION_EDITOR = True
except Exception as e:
    print(f"[Editor] Motion Editor 로드 실패: {e}")
    HAS_MOTION_EDITOR = False

# 뷰포트 임포트 (엔진 모듈 없으면 더미 뷰포트 사용)
try:
    from viewport import EngineViewport
    HAS_VIEWPORT = True
except Exception as e:
    print(f"[Editor] 뷰포트 로드 실패 (더미 모드): {e}")
    HAS_VIEWPORT = False

from engine_binding import binding as ge_python, HAS_ENGINE, to_entity


# ============================================================
# 더미 뷰포트 (엔진 모듈 없이도 UI 테스트 가능)
# ============================================================

class DummyViewport(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 300)
        self.setStyleSheet(f"background-color: {COLORS['bg_base']};")

        layout = QVBoxLayout(self)
        layout.setAlignment(Qt.AlignCenter)

        icon = QLabel("🎮")
        icon.setAlignment(Qt.AlignCenter)
        icon.setStyleSheet("font-size: 48px; background: transparent;")
        layout.addWidget(icon)

        msg = QLabel("3D Viewport\n(엔진 모듈 로드 필요)")
        msg.setAlignment(Qt.AlignCenter)
        msg.setStyleSheet(
            f"color: {COLORS['text_dim']}; font-size: 14px; "
            "background: transparent; line-height: 1.6;"
        )
        layout.addWidget(msg)

    def tick(self):
        pass  # 더미


# ============================================================
# 메인 에디터 윈도우
# ============================================================

class EditorMainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Quarter Flying Editor")
        self.resize(1600, 900)
        self.setMinimumSize(1000, 600)

        self._engine = None      # 엔진 모듈의 Engine 인스턴스 (연동 후)
        self._world = None       # 엔진 모듈의 World 인스턴스  (연동 후)
        self.is_playing = False  # 플레이 모드 상태
        
        # 설정 관리자
        self.config_manager = ConfigManager()

        self._build_menu()
        self._build_toolbar()
        self._build_central_widget()
        self._build_prefab_browser_dock()
        self._build_status_bar()
        self._connect_panels()

        # 엔진 연결 시도
        self._try_connect_engine()

        # 데모 씬 통합
        self.demo_scene = DemoSceneIntegration()
        self.demo_scene.scene_loaded.connect(self._on_scene_loaded)
        self.demo_scene.scene_error.connect(self._on_scene_error)
        
        # 설정 적용
        self._apply_config()
        
        # 기본 씬 로드
        self._load_default_scene()

        # FPS 상태 갱신 타이머
        self._fps_timer = QTimer(self)
        self._fps_timer.timeout.connect(self._on_fps_tick)
        self._fps_timer.start(16)  # ~60fps

        # Sound Lite Phase 6 (docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.4).
        # Play 중 ActionPlayerComponent를 가진 엔티티의 액션을 재생한다. 엔진에는
        # Action 개념이 없으므로(범위 정의서 §1.2) 파이썬이 재생한다 - §6.2의 (b) 확정.
        self._scene_playback = ScenePlaybackController()
        self._last_tick_time = time.perf_counter()

    # ----------------------------------------------------------------
    # 메뉴바
    # ----------------------------------------------------------------

    def _build_menu(self):
        mb = self.menuBar()

        # File
        file_menu = mb.addMenu("File")
        act_new   = QAction("New Scene",    self, shortcut="Ctrl+N")
        act_open  = QAction("Open Scene…",  self, shortcut="Ctrl+O")
        act_save  = QAction("Save Scene",   self, shortcut="Ctrl+S")
        act_saveas = QAction("Save As…",    self, shortcut="Ctrl+Shift+S")
        act_exit  = QAction("Exit",         self, shortcut="Alt+F4")
        act_exit.triggered.connect(self.close)
        file_menu.addActions([act_new, act_open, act_save, act_saveas])
        file_menu.addSeparator()
        file_menu.addAction(act_exit)

        # Edit
        edit_menu = mb.addMenu("Edit")
        self._act_undo = QAction("Undo", self, shortcut="Ctrl+Z")
        self._act_redo = QAction("Redo", self, shortcut="Ctrl+Y")
        self._act_undo.triggered.connect(self._on_undo)
        self._act_redo.triggered.connect(self._on_redo)
        edit_menu.addActions([self._act_undo, self._act_redo])
        self._update_undo_redo_actions()

        # View
        view_menu = mb.addMenu("View")
        act_reset = QAction("Reset Layout", self)
        act_reset.triggered.connect(self._reset_layout)
        view_menu.addAction(act_reset)

        # Motion (New)
        motion_menu = mb.addMenu("Motion")
        act_import = QAction("Import FBX…", self)
        act_save_action = QAction("Save Action…", self)
        act_load_action = QAction("Load Action…", self)
        
        act_import.triggered.connect(lambda: self.motion_editor.do_import_fbx() if hasattr(self, 'motion_editor') else None)
        act_save_action.triggered.connect(lambda: self.motion_editor.do_save_action() if hasattr(self, 'motion_editor') else None)
        act_load_action.triggered.connect(lambda: self.motion_editor.do_load_action() if hasattr(self, 'motion_editor') else None)
        
        motion_menu.addActions([act_import, act_save_action, act_load_action])

        # Engine
        engine_menu = mb.addMenu("Engine")
        act_play   = QAction("▶  Play",   self, shortcut="Ctrl+P")
        act_pause  = QAction("⏸  Pause",  self)
        act_stop   = QAction("⏹  Stop",   self, shortcut="Ctrl+Shift+P")
        act_play.triggered.connect(self._on_play)
        act_stop.triggered.connect(self._on_stop)
        engine_menu.addActions([act_play, act_pause, act_stop])

        # Help
        help_menu = mb.addMenu("Help")
        act_about = QAction("About Quarter Flying Editor", self)
        help_menu.addAction(act_about)

    # ----------------------------------------------------------------
    # 툴바
    # ----------------------------------------------------------------

    def _build_toolbar(self):
        tb = QToolBar("Main Toolbar")
        tb.setMovable(False)
        tb.setIconSize(QSize(16, 16))
        tb.setToolButtonStyle(Qt.ToolButtonTextOnly)
        self.addToolBar(tb)
        
        # 모드 전환 버튼
        self.btn_mode_scene = QPushButton("🎮 Scene Editor")
        self.btn_mode_scene.setObjectName("btn_mode_active")
        self.btn_mode_scene.clicked.connect(lambda: self._set_mode(0))
        
        self.btn_mode_play = QPushButton("▶  Play Mode")
        self.btn_mode_play.setObjectName("btn_mode_inactive")
        self.btn_mode_play.clicked.connect(lambda: self._on_play())
        
        self.btn_mode_motion = QPushButton("🎬 Motion Editor")
        self.btn_mode_motion.setObjectName("btn_mode_inactive")
        self.btn_mode_motion.clicked.connect(lambda: self._set_mode(2))
        
        tb.addWidget(self.btn_mode_scene)
        tb.addWidget(self.btn_mode_play)
        tb.addWidget(self.btn_mode_motion)
        tb.addSeparator()

        self.act_play = QAction("▶  Play", self)
        self.act_play.setCheckable(True)
        self.act_play.triggered.connect(self._on_play)

        act_pause = QAction("⏸  Pause", self)
        act_stop  = QAction("⏹  Stop",  self)
        act_stop.triggered.connect(self._on_stop)

        tb.addActions([self.act_play, act_pause, act_stop])
        tb.addSeparator()

        # 뷰포트 카메라: 기본은 에디터 카메라(우클릭 드래그 회전 / 휠클릭 드래그 이동 / 휠 줌 / F 리셋).
        # 체크하면 ECS Main Camera 시점(게임에서 보이는 화면)으로 본다.
        self.act_game_camera = QAction("🎥 게임 카메라로 보기", self)
        self.act_game_camera.setCheckable(True)
        self.act_game_camera.setToolTip(
            "체크: ECS Main Camera 시점으로 보기\n"
            "해제: 에디터 카메라 - 우클릭(또는 Alt+좌클릭) 드래그 회전, "
            "휠클릭(또는 Shift+우클릭) 드래그 이동, 휠 줌, F 리셋")
        self.act_game_camera.toggled.connect(self._on_toggle_game_camera)
        act_reset_camera = QAction("⟲ 카메라 리셋", self)
        act_reset_camera.setToolTip("에디터 카메라를 기본 시점으로 되돌립니다 (뷰포트에서 F)")
        act_reset_camera.triggered.connect(self._on_reset_editor_camera)
        act_align_camera = QAction("📌 선택 카메라를 이 시점으로", self)
        act_align_camera.setToolTip(
            "선택한 카메라 엔티티의 위치/회전을 지금 에디터 카메라 시점으로 맞춥니다 (Undo 가능)")
        act_align_camera.triggered.connect(self._on_align_camera_to_view)
        act_look_through = QAction("👁 선택 카메라 시점으로 보기", self)
        act_look_through.setToolTip("에디터 카메라를 선택한 카메라 엔티티의 위치/방향으로 옮깁니다 (Align의 반대)")
        act_look_through.triggered.connect(self._on_look_through_selected_camera)
        tb.addActions([self.act_game_camera, act_reset_camera, act_align_camera, act_look_through])
        tb.addSeparator()

        # 씬 이름
        self.lbl_scene = QLabel("  Untitled Scene  ")
        self.lbl_scene.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px; padding: 0 8px;")
        tb.addWidget(self.lbl_scene)

    # ----------------------------------------------------------------
    # 중앙 위젯 — 3분할 Splitter
    # ----------------------------------------------------------------

    def _build_central_widget(self):
        central = QWidget()
        self.setCentralWidget(central)
        root_layout = QVBoxLayout(central)
        root_layout.setContentsMargins(0, 0, 0, 0)
        root_layout.setSpacing(0)

        self.stack = QStackedWidget()
        root_layout.addWidget(self.stack)

        # --- Page 0: Scene Editor (기존 코드 그대로) ---
        self.scene_editor_widget = QWidget()
        scene_layout = QVBoxLayout(self.scene_editor_widget)
        scene_layout.setContentsMargins(0, 0, 0, 0)
        scene_layout.setSpacing(0)
        
        self.main_splitter = QSplitter(Qt.Horizontal)
        self.main_splitter.setHandleWidth(2)
        scene_layout.addWidget(self.main_splitter)

        # 1. 씬 계층 뷰
        self.scene_panel = SceneHierarchyPanel()
        self.scene_panel.setMinimumWidth(180)
        self.scene_panel.setMaximumWidth(350)
        self.scene_panel.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Expanding)
        self.main_splitter.addWidget(self.scene_panel)

        # 2. 뷰포트
        if HAS_VIEWPORT and HAS_ENGINE:
            # Scene 뷰포트는 마우스로 조작하는 에디터 카메라를 쓴다(툴바에서 게임 카메라로 전환 가능).
            self.viewport = EngineViewport(editor_camera=True)
        else:
            self.viewport = DummyViewport()
        self.viewport.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        self.main_splitter.addWidget(self.viewport)

        # 3. 인스펙터
        self.inspector = InspectorPanel()
        self.inspector.setMinimumWidth(220)
        self.inspector.setMaximumWidth(420)
        self.inspector.setSizePolicy(QSizePolicy.Fixed, QSizePolicy.Expanding)
        self.main_splitter.addWidget(self.inspector)

        # 비율 설정 (20% / 55% / 25%)
        self.main_splitter.setStretchFactor(0, 0)
        self.main_splitter.setStretchFactor(1, 1)
        self.main_splitter.setStretchFactor(2, 0)
        self.main_splitter.setSizes([240, 900, 300])

        self.stack.addWidget(self.scene_editor_widget)

        # --- Page 1: Play Mode (Phase 2 통합) ---
        self.play_mode_widget = QWidget()
        play_layout = QVBoxLayout(self.play_mode_widget)
        play_layout.setContentsMargins(0, 0, 0, 0)
        play_layout.setSpacing(0)
        
        # Play 페이지에는 뷰포트를 따로 만들지 않는다. Play에 들어갈 때 Scene 뷰포트(씬 World를
        # 가진 엔진)를 이 레이아웃으로 옮기고 게임 카메라로 전환한다(_move_viewport_to_play).
        # 예전에는 여기서 두 번째 EngineViewport(= 두 번째 Engine, 빈 World)를 만들었다. 그래서
        # Play 버튼을 누르면 씬 World는 숨겨진 Scene 뷰포트에서 돌고, 화면에는 씬이 없는 엔진이
        # 보였다. 즉 Play 화면에 게임이 나온 적이 없다(docs/INGAME_CAMERA_PLAN.md C0).
        self._play_layout = play_layout
        if not (HAS_VIEWPORT and HAS_ENGINE):
            play_layout.addWidget(DummyViewport())
        
        self.stack.addWidget(self.play_mode_widget)

        # --- Page 2: Motion Editor ---
        if HAS_MOTION_EDITOR:
            # Scene Editor와 같은 EngineViewport 인스턴스를 그대로 넘긴다 - Motion Editor
            # 전용 뷰포트를 새로 만들지 않는다(착수 계약서 §C11: "단일 EngineViewport
            # 인스턴스". 뷰포트를 하나 더 만들면 이미 알려진 "두 번째 뷰포트 초기화" 계열
            # 버그를 또 만들 위험이 있다 - CLAUDE.md 참고).
            shared_viewport = self.viewport if (HAS_VIEWPORT and HAS_ENGINE and isinstance(self.viewport, EngineViewport)) else None
            self.motion_editor = MotionEditorWidget(shared_viewport=shared_viewport)
            self.stack.addWidget(self.motion_editor)
        else:
            dummy_motion = QWidget()
            dl = QVBoxLayout(dummy_motion)
            dl.addWidget(QLabel("Motion Editor 로드 실패"))
            self.stack.addWidget(dummy_motion)

    def _set_mode(self, index: int):
        prev_index = self.stack.currentIndex()

        # 공유 뷰포트는 Scene(0) / Play(1) / Motion Editor(2) 페이지 사이를 옮겨 다닌다(§C11).
        # 먼저 원래 자리(Scene 스플리터)로 되돌린 다음, 새 페이지가 Scene이 아니면 거기로 옮긴다.
        # 그래서 Motion Editor -> Play 같은 직접 전환도 같은 경로를 탄다.
        if index != prev_index:
            if prev_index == 2 and getattr(self, 'motion_editor', None) is not None:
                self._move_viewport_to_scene_editor()
            elif prev_index == 1:
                self._move_viewport_back_from_play()
            if index == 2 and getattr(self, 'motion_editor', None) is not None:
                self._move_viewport_to_motion_editor()
            elif index == 1:
                self._move_viewport_to_play()

        self.stack.setCurrentIndex(index)

        # 버튼 상태 초기화 (btn_mode_play도 포함 — Play/Pause/Stop이 이 메서드를 거치지
        # 않고 objectName만 직접 바꾸면 unpolish/polish가 빠져 스타일이 갱신되지 않는다)
        self.btn_mode_scene.setObjectName("btn_mode_inactive")
        self.btn_mode_motion.setObjectName("btn_mode_inactive")
        self.btn_mode_play.setObjectName("btn_mode_inactive")

        if index == 0:  # Scene Editor
            self.btn_mode_scene.setObjectName("btn_mode_active")
        elif index == 1:  # Play Mode
            self.btn_mode_scene.setObjectName("btn_mode_active")  # Play는 Scene 모드의 확장
            self.btn_mode_play.setObjectName("btn_mode_active")
        elif index == 2:  # Motion Editor
            self.btn_mode_motion.setObjectName("btn_mode_active")

        # 스타일 강제 갱신 (objectName 변경만으로는 QSS가 재적용되지 않음)
        for btn in (self.btn_mode_scene, self.btn_mode_motion, self.btn_mode_play):
            btn.style().unpolish(btn)
            btn.style().polish(btn)

    def _move_viewport_to_motion_editor(self):
        """공유 뷰포트를 Scene Editor 스플리터에서 빼서 Motion Editor로 옮기고 Motion
        렌더 모드로 전환한다."""
        if not (HAS_VIEWPORT and HAS_ENGINE and isinstance(self.viewport, EngineViewport)):
            return
        if not hasattr(self, '_viewport_placeholder'):
            self._viewport_placeholder = QWidget()
            self._viewport_placeholder.setStyleSheet(f"background-color: {COLORS['bg_base']};")
        # attach_viewport()가 뷰포트를 Motion Editor 레이아웃으로 재부모 이동시키면
        # main_splitter에서는 Qt가 알아서 빠진다 - 그 빈 자리를 placeholder로 채운다.
        self.motion_editor.attach_viewport()
        self.main_splitter.insertWidget(1, self._viewport_placeholder)
        self.motion_editor.activate_motion_preview()

    def _viewport_is_engine(self) -> bool:
        return HAS_VIEWPORT and HAS_ENGINE and isinstance(self.viewport, EngineViewport)

    def _move_viewport_to_play(self):
        """공유 뷰포트를 Play 페이지로 옮기고 게임 카메라(ECS Main Camera)로 전환한다."""
        if not self._viewport_is_engine():
            return
        if not hasattr(self, '_viewport_placeholder'):
            self._viewport_placeholder = QWidget()
            self._viewport_placeholder.setStyleSheet(f"background-color: {COLORS['bg_base']};")
        self._play_layout.addWidget(self.viewport)          # 재부모 이동 - 스플리터에서는 자동으로 빠진다
        self.main_splitter.insertWidget(1, self._viewport_placeholder)
        self.viewport.set_use_editor_camera(False)

    def _move_viewport_back_from_play(self):
        """Play 페이지에서 Scene 스플리터로 되돌리고, 툴바 설정대로 카메라를 복원한다."""
        if not self._viewport_is_engine():
            return
        if hasattr(self, '_viewport_placeholder'):
            self._viewport_placeholder.setParent(None)
        self.main_splitter.insertWidget(1, self.viewport)
        self.viewport.set_use_editor_camera(not self.act_game_camera.isChecked())

    def _move_viewport_to_scene_editor(self):
        """공유 뷰포트를 Motion Editor에서 빼서 Scene Editor 스플리터로 되돌리고 Scene
        렌더 모드로 전환한다."""
        if not (HAS_VIEWPORT and HAS_ENGINE and isinstance(self.viewport, EngineViewport)):
            return
        self.motion_editor.deactivate_motion_preview()
        if hasattr(self, '_viewport_placeholder'):
            self._viewport_placeholder.setParent(None)
        self.main_splitter.insertWidget(1, self.viewport)

    # ----------------------------------------------------------------
    # 상태 바
    # ----------------------------------------------------------------

    def _build_status_bar(self):
        self.status = StatusBar()
        # QMainWindow 기본 상태바 교체
        self.setStatusBar(None)
        self.centralWidget().layout().addWidget(self.status)

    # ----------------------------------------------------------------
    # 프리팹 브라우저 Dock (docs/PREFAB_IMPLEMENTATION_PLAN.md §3 Phase 3)
    # ----------------------------------------------------------------

    def _build_prefab_browser_dock(self):
        """QDockWidget으로 붙인다 - main_splitter(3분할 고정 레이아웃) 안이 아니라
        QMainWindow의 도킹 영역에 둬서, Scene Editor/Play Mode/Motion Editor 어느
        모드로 전환해도(self.stack) 항상 접근 가능하고 사용자가 옮기거나 띄울 수도
        있다."""
        self.prefab_browser = PrefabBrowserPanel()
        dock = QDockWidget("Prefabs", self)
        dock.setObjectName("prefab_browser_dock")
        dock.setWidget(self.prefab_browser)
        dock.setFeatures(QDockWidget.DockWidgetMovable | QDockWidget.DockWidgetFloatable)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock)

    # ----------------------------------------------------------------
    # 패널 시그널 연결
    # ----------------------------------------------------------------

    def _connect_panels(self):
        # 씬 계층 뷰 → 인스펙터
        self.scene_panel.entity_selected.connect(self.inspector.on_entity_selected)
        self.scene_panel.entity_deselected.connect(self.inspector.on_entity_deselected)

        # 씬 계층 뷰 → 상태 바
        self.scene_panel.entity_selected.connect(
            lambda eid: self.status.set_selection(eid, f"Entity_{eid}")
        )
        self.scene_panel.entity_deselected.connect(
            lambda: self.status.set_selection(-1)
        )

        # Create Prefab(Scene Hierarchy) → 상태 바 + 프리팹 목록 즉시 갱신
        self.scene_panel.prefab_capture_status.connect(self.status.log)
        self.scene_panel.prefab_capture_status.connect(
            lambda _msg: self.prefab_browser.refresh()
        )

        # 부모 변경(드래그)/삭제(Scene Hierarchy) → 상태 바 + Undo 메뉴 활성화
        self.scene_panel.hierarchy_status.connect(self.status.log)
        self.scene_panel.hierarchy_status.connect(lambda _msg: self._update_undo_redo_actions())

        # Instantiate(Prefab Browser) → 상태 바
        self.prefab_browser.status_message.connect(self.status.log)

        # Revert(Inspector) → 상태 바
        self.inspector.prefab_revert_status.connect(self.status.log)

    # ----------------------------------------------------------------
    # 2단계: 엔진 ECS 연동
    # ----------------------------------------------------------------

    def _try_connect_engine(self):
        """
        엔진 모듈이 있으면 엔진 ECS를 패널에 연결.
        없으면 더미 모드로 계속 동작.
        """
        if not HAS_ENGINE:
            self.status.set_engine_online(False)
            self.status.log("더미 모드로 실행 중 (엔진 모듈 없음)")
            return

        try:
            # EngineViewport가 이미 엔진을 초기화했으면 거기서 가져옴
            if HAS_VIEWPORT and isinstance(self.viewport, EngineViewport):
                # showEvent 이후에 연결되어야 하므로 지연 연결
                connect_timer = QTimer(self)
                connect_timer.setSingleShot(True)
                connect_timer.timeout.connect(self._connect_engine_later)
                connect_timer.start(500)
            else:
                self.status.set_engine_online(False)
        except Exception as e:
            print(f"[Editor] 엔진 연결 실패: {e}")
            self.status.set_engine_online(False)

    def _connect_engine_later(self):
        """뷰포트 초기화 후 ECS 연동.

        세 단계로 나누고, 각 단계가 실패하면 자기 이름으로 로그를 남긴다:
          1. _activate_world()        - World 확보 + 활성화
          2. _wire_panels_to_engine() - registry/EditorAPI를 패널에 연결
          3. seed_demo_scene()        - 검증용 기본 엔티티 생성 (demo_scene_seed.py)
        원래는 이 셋이 바깥 try 하나에 묶여 있었다. 그래서 3번(예: Main Camera 생성)이
        실패해도 "ECS 연결 실패"로 기록돼서, 연결은 멀쩡한데 원인을 연결 쪽에서 찾게 만들었다.
        """
        if not (HAS_VIEWPORT and isinstance(self.viewport, EngineViewport)):
            return
        if not self.viewport.initialized:
            return

        try:
            engine, world, registry = self._activate_world()
        except Exception as e:
            self._log_engine(f"World 활성화 실패: {type(e).__name__}: {e}")
            return

        try:
            editor_api = self._wire_panels_to_engine(engine, world, registry)
        except Exception as e:
            self._log_engine(f"패널-엔진 연결 실패: {type(e).__name__}: {e}")
            return

        # 3단계 실패는 seed_demo_scene()이 단계별로 이미 로그를 남기므로 여기서는 잡지 않는다
        # (잡을 예외는 인자 누락 같은 호출 순서 버그뿐이고, 그건 드러나야 한다).
        if not getattr(self, '_demo_scene_seeded', False):
            seed_demo_scene(editor_api, registry, self._log_engine)
            self._demo_scene_seeded = True
            self._instantiate_loaded_scene(editor_api, registry)

    def _instantiate_loaded_scene(self, editor_api, registry):
        """이미 파싱해 둔 scene.json 오브젝트를 ECS 엔티티로 만든다 (scene_instantiation.py 참고)."""
        scene_objects = self.demo_scene.get_scene_objects() if hasattr(self, 'demo_scene') else []
        if not scene_objects:
            return
        try:
            specs = expand_scene_objects(scene_objects)
        except ValueError as e:
            # scene.json 형식 오류는 파일을 고쳐야 하는 문제라 엔티티를 하나도 만들지 않는다.
            self._log_engine(f"[SceneLoad] scene.json 형식 오류 - {e}")
            return
        instantiate_scene(editor_api, registry, specs, self._log_engine)

    def _activate_world(self):
        """뷰포트의 엔진에서 World를 확보해 active로 만든다. (engine, world, registry)를 반환."""
        engine = self.viewport.engine
        world = engine.GetActiveWorld()
        if world is None:
            world = engine.CreateWorld("Untitled Scene")
        # WorldManager::Update()는 activeWorld만 틱한다(engine/ecs/WorldManager.cpp) -
        # CreateWorld()가 World를 만들어주긴 하지만 자동으로 active로 만들지는 않는다.
        # 이걸 안 부르면 World::Update()가 절대 안 불려서 그 안에 등록된 ECS System(예:
        # RenderSystem)이 전부 조용히 죽은 코드가 된다 - registry 조작(엔티티/컴포넌트
        # 생성)은 active 여부와 무관하게 항상 되므로, Scene Hierarchy에 엔티티가 보이는
        # 것만으로는 이 버그가 전혀 티가 안 났다.
        engine.SetActiveWorld(world)
        registry = world.GetRegistry()
        if registry is None:
            raise RuntimeError("World.GetRegistry()가 None을 반환했습니다")
        return engine, world, registry

    def _wire_panels_to_engine(self, engine, world, registry):
        """registry와 EditorAPI를 각 패널에 연결한다. 연결된 editor_api를 반환."""
        self.scene_panel.connect_registry(registry)
        self.inspector.connect_registry(registry)

        self._engine = engine
        self._world = world
        self.status.set_engine_online(True)
        self._log_engine("엔진 ECS 연결 완료")

        editor_api = ge_python.EditorAPI.get_instance()
        editor_api.set_registry(registry)
        self._editor_api = editor_api
        self.inspector.connect_editor(editor_api)
        self.scene_panel.connect_editor(editor_api)
        self.prefab_browser.set_editor_api(editor_api)
        self._update_undo_redo_actions()
        return editor_api

    def _log_engine(self, msg: str):
        """엔진 연동 관련 메시지를 상태바와 콘솔에 같이 남긴다."""
        self.status.log(msg)
        print(f"[Editor] {msg}")

    def _on_undo(self):
        if not HAS_ENGINE:
            return
        try:
            from engine_binding import binding as ge_python
            ge_python.CommandManager.get_instance().undo()
            self._after_edit_command()
        except ge_python.EngineError as e:
            self.status.log(f"Undo failed: {e}")

    def _on_redo(self):
        if not HAS_ENGINE:
            return
        try:
            from engine_binding import binding as ge_python
            ge_python.CommandManager.get_instance().redo()
            self._after_edit_command()
        except ge_python.EngineError as e:
            self.status.log(f"Redo failed: {e}")

    def _after_edit_command(self):
        self._update_undo_redo_actions()
        if self.inspector._current_entity_id != -1:
            self.inspector.on_entity_selected(self.inspector._current_entity_id)

    def _update_undo_redo_actions(self):
        if not HAS_ENGINE:
            self._act_undo.setEnabled(False)
            self._act_redo.setEnabled(False)
            return
        try:
            from engine_binding import binding as ge_python
            cm = ge_python.CommandManager.get_instance()
            self._act_undo.setEnabled(cm.can_undo())
            self._act_redo.setEnabled(cm.can_redo())
        except Exception:
            self._act_undo.setEnabled(False)
            self._act_redo.setEnabled(False)

    # ----------------------------------------------------------------
    # 레이아웃 리셋
    # ----------------------------------------------------------------

    def _on_toggle_game_camera(self, checked: bool):
        viewport = getattr(self, 'viewport', None)
        if HAS_VIEWPORT and isinstance(viewport, EngineViewport) and viewport.has_editor_camera:
            viewport.set_use_editor_camera(not checked)
            self.status.log("뷰포트: 게임 카메라(Main Camera)" if checked else "뷰포트: 에디터 카메라")

    def _on_align_camera_to_view(self):
        """선택한 카메라 엔티티를 에디터 카메라 시점으로 옮긴다 (docs/INGAME_CAMERA_PLAN.md C7 Align to View).

        이동과 회전을 트랜잭션 하나로 묶어서 Undo 한 번에 되돌아가게 한다.
        """
        viewport = getattr(self, 'viewport', None)
        editor_api = getattr(self, '_editor_api', None)
        if not (HAS_VIEWPORT and isinstance(viewport, EngineViewport) and viewport.has_editor_camera
                and editor_api is not None and self._world is not None):
            self.status.log("Align to View: 엔진이 연결된 Scene 뷰포트에서만 쓸 수 있습니다")
            return
        entity_id = self.inspector._current_entity_id
        if entity_id is None or entity_id < 0:
            self.status.log("Align to View: 먼저 카메라 엔티티를 선택하세요")
            return
        registry = self._world.GetRegistry()
        entity = to_entity(entity_id)
        cam_json = registry.GetComponentJson(entity, "CameraComponent")
        if not cam_json or cam_json == "{}":
            self.status.log(f"Align to View: '{registry.GetEntityName(entity)}'에 CameraComponent가 없습니다")
            return

        eye, target = viewport._orbit.view()
        qx, qy, qz, qw = look_rotation_quaternion(eye, target)
        editor_api.begin_transaction("Align Camera to View")
        try:
            editor_api.move_entity(entity, ge_python.Vec3(*eye))
            editor_api.rotate_entity(entity, ge_python.Quaternion(qx, qy, qz, qw))
            editor_api.commit_transaction()
        except Exception as e:
            editor_api.cancel_transaction()
            self.status.log(f"Align to View 실패: {type(e).__name__}: {e}")
            return
        self._update_undo_redo_actions()
        self.inspector.on_entity_selected(entity_id)
        self.status.log(f"'{registry.GetEntityName(entity)}' 카메라를 현재 에디터 시점으로 맞췄습니다")

    def _on_look_through_selected_camera(self):
        """에디터 카메라를 선택한 카메라 엔티티 시점으로 옮긴다 (docs/INGAME_CAMERA_PLAN.md C7).

        카메라 프리뷰(작은 PIP 창) 대신 넣은 기능이다. PIP는 오프스크린 렌더 타깃과 두 번째 렌더 패스가
        필요해서 보류했다. 에디터 카메라를 옮길 뿐 엔티티는 바꾸지 않으므로 Undo 대상이 아니다.
        """
        viewport = getattr(self, 'viewport', None)
        if not (HAS_VIEWPORT and isinstance(viewport, EngineViewport) and viewport.has_editor_camera
                and self._world is not None):
            self.status.log("선택 카메라 시점으로 보기: 엔진이 연결된 Scene 뷰포트에서만 쓸 수 있습니다")
            return
        entity_id = self.inspector._current_entity_id
        if entity_id is None or entity_id < 0:
            self.status.log("선택 카메라 시점으로 보기: 먼저 카메라 엔티티를 선택하세요")
            return
        registry = self._world.GetRegistry()
        entity = to_entity(entity_id)
        if registry.GetComponentJson(entity, "CameraComponent") in ("", "{}"):
            self.status.log(f"'{registry.GetEntityName(entity)}'에 CameraComponent가 없습니다")
            return
        t = json.loads(registry.GetComponentJson(entity, "TransformComponent"))
        forward = forward_from_quaternion(quaternion_from_euler_degrees(t["rotation"]))
        viewport._orbit.look_from(t["position"], forward)
        if self.act_game_camera.isChecked():
            self.act_game_camera.setChecked(False)   # 에디터 카메라 보기로 전환(toggled -> set_use_editor_camera)
        viewport._push_editor_camera()
        self.status.log(f"에디터 카메라를 '{registry.GetEntityName(entity)}' 시점으로 옮겼습니다")

    def _on_reset_editor_camera(self):
        viewport = getattr(self, 'viewport', None)
        if HAS_VIEWPORT and isinstance(viewport, EngineViewport):
            viewport.reset_editor_camera()

    def _reset_layout(self):
        self.main_splitter.setSizes([240, 900, 300])

    # ----------------------------------------------------------------
    # 엔진 제어 (Phase 2 플레이 모드 통합)
    # ----------------------------------------------------------------

    def _on_play(self):
        """플레이 모드 시작/전환"""
        if not self.is_playing:
            self.is_playing = True
            self.act_play.setChecked(True)

            # 플레이 모드로 전환 (버튼 활성 상태 갱신 포함 — Motion Editor 등 다른
            # 모드에서 넘어올 때 이전 버튼 강조가 남아있지 않도록 _set_mode로 일원화)
            self._set_mode(1)

            # 엔진 플레이 시작
            if self._engine and HAS_ENGINE:
                try:
                    # 월드 플레이 시작
                    if self._world:
                        self._world.Play()
                        # Sound Lite Phase 6 - 액션 재생 시작(§2.4). 액션 경로가 상대
                        # 경로면 engine/assets 기준으로 푼다.
                        assets_dir = os.path.join(os.path.dirname(_EDITOR_DIR), "assets")
                        started = self._scene_playback.start(self._world.GetRegistry(), assets_dir)
                        if started:
                            self.status.log(f"액션 재생 시작: {started}개")
                    self.status.log("Play mode started")
                except Exception as e:
                    print(f"[Editor] Play start error: {e}")
                    self.status.log(f"Play error: {e}")
            else:
                self.status.log("Play mode (no engine)")
            
            print("[Editor] Play mode started")
        else:
            self._on_pause()

    def _on_pause(self):
        """일시정지 처리"""
        self.is_playing = False
        self.act_play.setChecked(False)
        self.btn_mode_play.setObjectName("btn_mode_inactive")
        self.btn_mode_play.style().unpolish(self.btn_mode_play)
        self.btn_mode_play.style().polish(self.btn_mode_play)

        # Sound Lite Phase 6 - 일시정지하면 울리던 소리도 멈춘다. 커서는 그대로 두므로
        # 다시 Play를 누르면 이어서 재생된다(_on_play가 start()로 다시 만들긴 하지만,
        # 멈춘 순간 소리가 계속 나는 것부터 막는다).
        self._scene_playback.sound_player.stop_all()

        # 엔진 일시정지
        if self._engine and HAS_ENGINE:
            try:
                if self._world:
                    self._world.Pause()
                self.status.log("Paused")
            except Exception as e:
                print(f"[Editor] Pause error: {e}")
        
        print("[Editor] Paused")

    def _on_stop(self):
        """정지 처리"""
        self.is_playing = False
        self.act_play.setChecked(False)

        # Sound Lite Phase 6 - 소리가 Scene 모드로 새어나가지 않게 한다.
        self._scene_playback.stop()

        # 씬 에디터 모드로 복귀 (버튼 활성 상태 갱신 포함)
        self._set_mode(0)

        # 엔진 정지
        if self._engine and HAS_ENGINE:
            try:
                if self._world:
                    self._world.Stop()
                self.status.log("Stopped")
            except Exception as e:
                print(f"[Editor] Stop error: {e}")
        
        self.inspector.set_play_mode(False)
        
        # 롤백된 데이터를 UI에 반영하기 위해 위젯 리프레시
        if self.inspector._current_entity_id != -1:
            self.inspector.on_entity_selected(self.inspector._current_entity_id)
            
        self.status.log("Play 모드 종료")

    # ----------------------------------------------------------------
    # 데모 씬 통합 메서드
    # ----------------------------------------------------------------

    def _load_default_scene(self):
        """기본 씬 로드"""
        # cwd(현재 작업 디렉터리)가 아니라 이 파일 위치(_EDITOR_DIR) 기준 상대경로로
        # 계산해야 한다 — "engine/editor"에서 실행하든 다른 곳에서 실행하든
        # 항상 "engine/assets/scene.json"을 가리키게 함.
        scene_path = os.path.join(os.path.dirname(_EDITOR_DIR), "assets", "scene.json")
        if os.path.exists(scene_path):
            self.demo_scene.load_scene(scene_path)
        else:
            print(f"[Editor] Default scene not found: {scene_path}")

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
        if not HAS_ENGINE:
            print("[Editor] 엔진 모듈이 없어 PathResolver를 초기화하지 않았습니다")
        else:
            try:
                from engine_binding import binding as ge_python
                ge_python.PathResolver.Init(asset_root, shader_root)
                print(f"[Editor] PathResolver initialized: {asset_root}, {shader_root}")
            except Exception as e:
                print(f"[Editor] PathResolver initialization failed: {e}")

    def _on_scene_loaded(self, scene_data):
        """씬 로드 완료 처리"""
        scene_name = scene_data.get('name', 'Untitled Scene')
        self.lbl_scene.setText(f"  {scene_name}  ")
        
        # 씬 계층 패널 업데이트
        if hasattr(self, 'scene_panel'):
            self.scene_panel.load_from_scene_data(scene_data)
        
        print(f"[Editor] Scene loaded: {scene_name}")

    def _on_scene_error(self, error_msg):
        """씬 로드 에러 처리"""
        from PySide6.QtWidgets import QMessageBox
        QMessageBox.critical(self, "Scene Load Error", error_msg)
        print(f"[Editor] Scene error: {error_msg}")

    def closeEvent(self, event):
        """종료 시 설정 저장"""
        self.config_manager.save_config()
        super().closeEvent(event)

    # ----------------------------------------------------------------
    # FPS 틱
    # ----------------------------------------------------------------

    def _on_fps_tick(self):
        # 여기서 self.viewport.tick()을 부르지 않는다. EngineViewport는 자기 16ms 타이머로 이미
        # 틱한다. 예전에는 여기서도 불러서 Scene 엔진이 프레임당 두 번(약 120Hz) 틱했다.
        self.status.tick_frame()

        # 이 타이머는 60fps 고정인데 액션의 fps는 다를 수 있다. 틱 수를 프레임 수로
        # 쓰면 30fps 액션이 두 배 빨리 재생되므로 **실제 경과 시간**으로 전진시킨다.
        now = time.perf_counter()
        dt = now - self._last_tick_time
        self._last_tick_time = now
        if self.is_playing:
            # dt가 비정상적으로 크면(탭 전환, 창 이동 등) 소리가 한꺼번에 쏟아지므로
            # 한 프레임 분량 근처로 자른다. 커서 쪽에도 한 바퀴 상한이 있지만
            # 여러 엔티티가 동시에 터지는 것은 여기서 막는 게 싸다.
            self._scene_playback.tick(min(dt, 0.1))

        # 엔티티 수 갱신
        if self._engine and HAS_ENGINE:
            try:
                world = self._engine.GetActiveWorld()
                if world:
                    registry = world.GetRegistry()
                    count = registry.GetEntityCount()
                    self.status.set_entity_count(count)
            except Exception:
                pass


# ============================================================
# Entry Point
# ============================================================

if __name__ == "__main__":
    app = QApplication(sys.argv)

    # 폰트 설정
    font = QFont("Segoe UI", 10)
    app.setFont(font)

    # 다크 테마 적용
    apply_theme(app)

    window = EditorMainWindow()
    apply_dark_title_bar(window)  # QSS로는 못 건드리는 OS 네이티브 타이틀바를 다크로
    window.show()

    sys.exit(app.exec())
