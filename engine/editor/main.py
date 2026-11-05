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
from PySide6.QtGui import QAction, QFont, QIcon, QKeySequence, QShortcut

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
from project import current_project
from project_actions import ProjectActionsMixin

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

class EditorMainWindow(ProjectActionsMixin, QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("Quarter Flying Editor")
        self.resize(1600, 900)
        self.setMinimumSize(1000, 600)

        self._engine = None      # 엔진 모듈의 Engine 인스턴스 (연동 후)
        self._world = None       # 엔진 모듈의 World 인스턴스  (연동 후)
        # Play 세션 상태. is_playing은 Play~Stop 사이 전체(일시정지 포함)이고, is_paused는 그 안의 일시정지다.
        # 예전에는 Pause가 is_playing을 False로 만들어서, 일시정지 중에는 "Play 중이 아님"으로 보였다.
        # 그래서 일시정지한 채 Save Scene을 누르면 Play로 바뀐 월드가 그대로 씬 파일에 저장됐다
        # (project_actions._require_edit_mode가 is_playing만 본다). 에러도 경고도 없었다.
        self.is_playing = False
        self.is_paused = False
        
        # 설정 관리자
        self.config_manager = ConfigManager()
        # 마지막으로 연 프로젝트(없으면 샘플 프로젝트 = engine/). 뷰포트가 엔진을 만들 때
        # 에셋 기준 폴더를 이 프로젝트에서 받으므로 _build_central_widget보다 먼저 정한다.
        self._restore_last_project()

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
        # 도킹·분할 배치(E-6). 기본 배치를 먼저 기억해 두고(View > Reset Layout), 저장된 배치가 있으면 복원한다.
        self._apply_default_dock_height()
        self._default_dock_state = self.saveState()
        self._restore_layout()
        # 프로젝트 세션(project/session.py): 액션 목록·에디터 카메라는 지금, 씬과 모드는 엔진이 연결된 뒤
        # (_connect_engine_later). 엔진이 없으면 씬을 열 수 없으니 모드만 바로 복원한다.
        self._apply_session_ui(self._load_project_session())
        if not self._viewport_is_engine():
            self._apply_session_mode()
        
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
        # 예전에는 New/Open/Save Scene 항목만 만들고 아무 동작에도 연결하지 않았다(눌러도 아무 일 없음).
        # 씬 파일 저장/불러오기가 엔진에 생기면서(engine/scene/SceneIO.h) project_actions.py로 연결한다.
        self._add_project_menu_actions(file_menu)
        act_exit  = QAction("Exit",         self, shortcut="Alt+F4")
        act_exit.triggered.connect(self.close)
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
        view_menu.addSeparator()   # 아래쪽 패널 표시/숨김은 도크를 만든 뒤 붙인다(_build_prefab_browser_dock)
        self._view_menu = view_menu
        from panels.production_tools import open_references, open_material, open_profiler
        tools_menu = mb.addMenu("Tools")
        tools_menu.addAction("Asset References…", lambda: open_references(self))
        tools_menu.addAction("Material Editor…", lambda: open_material(self))
        tools_menu.addAction("Frame Profiler…", lambda: open_profiler(self))

        # Motion (New)
        motion_menu = mb.addMenu("Motion")
        # 예전에는 이 항목이 켜져 있었고, 누르면 "다음 단계에서 연동됩니다" 안내만 떴다(FBX 파서 없음) - 되는 기능처럼
        # 보이는 메뉴였다. 지원하지 않는 동안은 꺼 두고 이유를 툴팁에 적는다(docs/USABILITY_IMPROVEMENT_PLAN.md A-1).
        act_import = QAction("Import FBX… (미지원)", self)
        act_import.setEnabled(False)
        act_import.setToolTip("FBX 가져오기는 아직 지원하지 않습니다. 스켈레톤·클립은 JSON(*.skeleton.json, "
                              "*.clip.json)으로 assets/motion에 넣어 쓰세요. 외부 형식은 glTF 임포터로 추가할 예정입니다.")
        act_new_action = QAction("New Action", self)
        act_save_action = QAction("Save Action", self)
        act_save_action_as = QAction("Save Action As…", self)
        act_load_action = QAction("Load Action…", self)
        act_new_action.triggered.connect(lambda: self.motion_editor.do_new_action() if hasattr(self, 'motion_editor') else None)
        act_save_action_as.triggered.connect(lambda: self.motion_editor.do_save_action_as() if hasattr(self, 'motion_editor') else None)
        # 최근 액션(A-4). 열 때마다 목록을 다시 만든다(config.json의 motion.recentActions).
        self._recent_actions_menu = QMenu("Recent Actions", self)
        self._recent_actions_menu.aboutToShow.connect(self._fill_recent_actions_menu)
        
        act_import.triggered.connect(lambda: self.motion_editor.do_import_fbx() if hasattr(self, 'motion_editor') else None)
        act_save_action.triggered.connect(lambda: self.motion_editor.do_save_action() if hasattr(self, 'motion_editor') else None)
        act_load_action.triggered.connect(lambda: self.motion_editor.do_load_action() if hasattr(self, 'motion_editor') else None)
        
        motion_menu.setToolTipsVisible(True)   # 꺼진 항목의 이유(툴팁)가 메뉴에서 보이게
        motion_menu.addActions([act_new_action, act_save_action, act_save_action_as, act_load_action])
        motion_menu.addMenu(self._recent_actions_menu)
        motion_menu.addSeparator()
        motion_menu.addAction(act_import)

        # Engine
        engine_menu = mb.addMenu("Engine")
        # 에디터 단축키는 Play 중에도 수식키 조합만 쓴다. Space·Esc 같은 맨 키는 뷰포트에 포커스가 있으면
        # 게임 입력으로 엔진에 간다(viewport.py) - 에디터가 가져가면 게임이 그 키를 쓸 수 없다
        # (docs/USABILITY_IMPROVEMENT_PLAN.md E-1).
        act_play   = QAction("▶  Play",   self, shortcut="Ctrl+P")
        act_stop   = QAction("⏹  Stop",   self, shortcut="Ctrl+Shift+P")
        act_play.triggered.connect(self._on_play)
        act_stop.triggered.connect(self._on_stop)
        # 예전에는 Pause 메뉴 항목이 아무 슬롯에도 연결돼 있지 않았다(눌러도 아무 일 없음).
        self._act_pause_menu = self._make_pause_action(shortcut="Ctrl+Alt+P")
        engine_menu.addActions([act_play, self._act_pause_menu, act_stop])

        # Help
        help_menu = mb.addMenu("Help")
        act_about = QAction("About Quarter Flying Editor", self)
        act_about.triggered.connect(self._on_about)   # 예전에는 연결된 슬롯이 없었다
        act_shortcuts = QAction("Keyboard Shortcuts", self)
        act_shortcuts.triggered.connect(self._on_keyboard_shortcuts)
        help_menu.addActions([act_shortcuts, act_about])

    def _fill_recent_actions_menu(self):
        menu = self._recent_actions_menu
        menu.clear()
        recent = self.motion_editor.recent_actions() if hasattr(self, 'motion_editor') else []
        if not recent:
            menu.addAction("(없음)").setEnabled(False)
            return
        for path in recent:
            act = menu.addAction(path)
            act.setEnabled(os.path.isfile(path))
            act.triggered.connect(lambda _=False, p=path: self._open_recent_action(p))

    def _open_recent_action(self, path: str):
        self._set_mode(2)
        self.motion_editor.do_load_action(path)

    def _make_pause_action(self, shortcut=None):
        act = QAction("⏸  Pause", self)
        if shortcut:
            act.setShortcut(shortcut)
        act.setCheckable(True)
        act.setEnabled(False)          # Play 중에만 켠다(_refresh_play_controls)
        act.setToolTip("일시정지 / 계속 (Ctrl+Alt+P) - Play 중에만")
        act.triggered.connect(self._on_pause_toggle)
        return act

    # 메뉴의 QAction에 걸리지 않는 에디터 입력(위젯 단축키·마우스). 메뉴 단축키는 아래 함수가 메뉴에서 직접 읽는다.
    _EXTRA_EDITOR_INPUTS = [
        ("Delete", "선택 엔티티 삭제 (Scene Hierarchy·뷰포트, Edit 모드)"),
        ("Ctrl+D", "선택 엔티티 복제 (Scene Hierarchy·뷰포트, Edit 모드)"),
        ("Ctrl/Shift+클릭", "Scene Hierarchy 다중 선택"),
        ("F", "에디터 카메라 리셋 (뷰포트)"),
        ("우클릭 드래그 / Alt+좌클릭 드래그", "에디터 카메라 회전"),
        ("휠클릭 드래그 / Shift+우클릭 드래그", "에디터 카메라 이동"),
        ("휠", "에디터 카메라 줌"),
    ]

    def editor_shortcut_rows(self):
        """(키, 동작) 목록. 메뉴의 QAction에서 읽으므로 단축키를 바꾸면 이 표도 저절로 바뀐다(E-5).
        게임 조작(프로젝트의 입력 액션 맵)은 넣지 않는다 - 에디터 설정과 게임 데이터는 다른 것이다(§1.1)."""
        rows = []
        for menu_action in self.menuBar().actions():
            menu = menu_action.menu()
            if menu is None:
                continue
            for act in menu.actions():
                key = act.shortcut().toString()
                if key:
                    rows.append((key, f"{menu_action.text()} > {act.text().strip()}"))
        return rows + list(self._EXTRA_EDITOR_INPUTS)

    def _on_keyboard_shortcuts(self):
        from PySide6.QtWidgets import QMessageBox
        rows = "".join(f"<tr><td><b>{k}</b></td><td>&nbsp;&nbsp;{v}</td></tr>" for k, v in self.editor_shortcut_rows())
        QMessageBox.information(
            self, "Keyboard Shortcuts",
            f"<table>{rows}</table><br>Play 중에는 수식키 없는 키가 뷰포트에 포커스가 있으면 게임으로 갑니다.<br>"
            "게임 조작 키는 프로젝트의 입력 액션 맵(assets/input/actions.json)에서 정합니다.")

    def _on_about(self):
        from PySide6.QtWidgets import QMessageBox
        engine_version = None
        if HAS_ENGINE:
            try:
                from engine_binding import binding as qf
                engine_version = getattr(qf, "__version__", None)
            except Exception:
                engine_version = None
        python_version = ".".join(str(v) for v in sys.version_info[:3])
        QMessageBox.about(
            self, "About Quarter Flying Editor",
            "<b>Quarter Flying Editor</b><br><br>"
            f"엔진 버전: {engine_version or '엔진 미연결'}<br>"
            f"Python: {python_version}<br>"
            f"프로젝트: {current_project().root}")

    # ----------------------------------------------------------------
    # 툴바
    # ----------------------------------------------------------------

    def _build_toolbar(self):
        tb = QToolBar("Main Toolbar")
        tb.setObjectName("main_toolbar")   # saveState/restoreState는 objectName으로 툴바·도크를 찾는다(E-6)
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

        # 예전에는 툴바 Pause도 아무 슬롯에 연결돼 있지 않았다. 단축키는 메뉴 쪽 항목에만 둔다
        # (같은 단축키를 두 QAction에 걸면 Qt가 "Ambiguous shortcut"으로 둘 다 무시한다).
        self._act_pause_tb = self._make_pause_action()
        act_stop  = QAction("⏹  Stop",  self)
        act_stop.triggered.connect(self._on_stop)

        tb.addActions([self.act_play, self._act_pause_tb, act_stop])
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

        # 뷰포트 기즈모 도구(0 이동, 1 회전, 2 크기). 고르는 곳은 Inspector Transform의 줄 이름과 뷰포트의 W/E/R이다
        # (사용자 결정 2026-10-30: 값을 보는 곳에서 도구를 고른다 - 툴바 버튼은 두지 않는다).
        self._gizmo_mode = 0

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
        self.lbl_play_hint = QLabel("")
        play_layout.addWidget(self.lbl_play_hint)
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
            self.motion_editor.recent_store = self.config_manager     # 최근 액션 목록(A-4)은 에디터 설정
            self.motion_editor.status_message = lambda text: self.status.log(text) if hasattr(self, 'status') else print(text)
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
        self._update_bottom_docks_for_mode(index)

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
        dock.setObjectName("prefab_browser_dock")   # saveState/restoreState가 이 이름으로 도크를 찾는다(E-6)
        dock.setWidget(self.prefab_browser)
        # Closable: View 메뉴의 표시/숨김(toggleViewAction)은 close()로 숨기는데, Qt는 닫기 기능이 없는 도크의 close를
        # 무시한다 - 이 플래그 없이 메뉴만 달면 눌러도 아무 일도 안 일어난다. 닫은 패널은 View 메뉴로 다시 연다.
        _features = QDockWidget.DockWidgetMovable | QDockWidget.DockWidgetFloatable | QDockWidget.DockWidgetClosable
        dock.setFeatures(_features)
        self.addDockWidget(Qt.BottomDockWidgetArea, dock)

        # 입력 액션 맵 편집(G2). 프리팹 브라우저와 같은 아래쪽 영역에 탭으로 둔다.
        from panels.input_actions_panel import InputActionsPanel
        self.input_actions_panel = InputActionsPanel()
        self.input_actions_panel.status_message.connect(lambda m: self.status.log(m) if hasattr(self, 'status') else None)
        self.input_actions_panel.reload_requested.connect(self._load_input_actions)
        input_dock = QDockWidget("Input Actions", self)
        input_dock.setObjectName("input_actions_dock")
        input_dock.setWidget(self.input_actions_panel)
        input_dock.setFeatures(_features)
        self.addDockWidget(Qt.BottomDockWidgetArea, input_dock)
        self.tabifyDockWidget(dock, input_dock)
        dock.raise_()

        # 뷰포트를 넓히려고 아래쪽 패널을 숨길 수 있게 한다(도크의 × 또는 View 메뉴, 다시 켜는 것은 View 메뉴).
        self._bottom_docks = (dock, input_dock)
        self._docks_hidden_for_motion = None   # Motion Editor가 숨긴 동안 {도크: 숨기기 전 표시 여부}
        # 탭으로 묶인 도크는 뒤쪽 탭도 isVisible()이 True라 앞에 있던 탭을 알 수 없다 - 활성화 시그널로 기억한다.
        self._raised_bottom_dock = dock
        self.tabifiedDockWidgetActivated.connect(
            lambda d: setattr(self, '_raised_bottom_dock', d) if d in self._bottom_docks else None)
        for d in self._bottom_docks:
            self._view_menu.addAction(d.toggleViewAction())

    # 아래쪽 도크의 기본 높이. 예전에는 높이를 정하지 않아 Qt가 패널 sizeHint로 잡았고(약 310px), Motion Editor에서는
    # 뷰포트가 180px 남짓으로 눌렸다.
    _BOTTOM_DOCK_HEIGHT = 180
    # 저장된 도킹 배치(config.json layout.dockState)의 형식. 기본 배치를 바꾸면 올린다 - 옛 배치를 복원하면 큰 도크가
    # 그대로 돌아온다. 2: 아래쪽 도크 기본 높이 축소(2026-10-30).
    _LAYOUT_VERSION = 2

    def _apply_default_dock_height(self):
        self.resizeDocks([self._bottom_docks[0]], [self._BOTTOM_DOCK_HEIGHT], Qt.Vertical)

    def _update_bottom_docks_for_mode(self, index: int):
        """Motion Editor(2)에서는 아래쪽 도크(프리팹·입력 액션)를 숨긴다 - 씬 편집용이라 거기서는 쓰지 않고, 뷰포트
        높이만 차지했다. 다른 모드로 돌아가면 숨기기 전 표시 여부로 되돌린다(Motion 중 View 메뉴로 켠 것은 그때만 유효)."""
        if not hasattr(self, '_bottom_docks'):
            return
        if index == 2 and self._docks_hidden_for_motion is None:
            # isVisible()이 아니라 isHidden(): 시작할 때 세션 복원이 창을 띄우기 전에 Motion Editor로 바꾸는데, 그때는
            # 모든 위젯의 isVisible()이 False라 "원래 숨겨져 있었다"로 기억해 Scene Editor로 돌아가도 도크가 사라졌다.
            self._docks_hidden_for_motion = {d: not d.isHidden() for d in self._bottom_docks}
            for d in self._bottom_docks:
                d.hide()
        elif index != 2 and self._docks_hidden_for_motion is not None:
            restore = self._docks_hidden_for_motion
            self._docks_hidden_for_motion = None
            for d, visible in restore.items():
                d.setVisible(visible)
            raised = self._raised_bottom_dock
            if restore.get(raised):
                raised.raise_()   # 다시 보이면 마지막에 보인 탭이 앞으로 온다 - 숨기기 전 앞에 있던 탭으로 되돌린다

    # ----------------------------------------------------------------
    # 패널 시그널 연결
    # ----------------------------------------------------------------

    def _connect_panels(self):
        # 뷰포트에서도 Delete / Ctrl+D (E-4). 위젯에만 걸고 Play 중에는 끈다(_refresh_play_controls) - Play 중의 맨 키는
        # 게임 입력이다. 처리는 계층 패널의 같은 함수(Undo되는 명령)로 보낸다.
        self._viewport_edit_shortcuts = []
        if self._viewport_is_engine():
            from panels.scene_hierarchy import widget_shortcut
            self._viewport_edit_shortcuts = [
                widget_shortcut(QKeySequence.Delete, self.viewport, self.scene_panel.delete_selected_entity),
                widget_shortcut(QKeySequence("Ctrl+D"), self.viewport, self.scene_panel.duplicate_selected_entity),
                # 기즈모 도구. Play 중에는 꺼진다(같은 목록) - 그때 W는 게임 입력(키보드 이동)이다.
                widget_shortcut(QKeySequence("W"), self.viewport, lambda: self._set_gizmo_mode(0)),
                widget_shortcut(QKeySequence("E"), self.viewport, lambda: self._set_gizmo_mode(1)),
                widget_shortcut(QKeySequence("R"), self.viewport, lambda: self._set_gizmo_mode(2)),
            ]
            self.viewport.focus_changed.connect(lambda has: self._update_play_hint(has))

        # 씬 계층 뷰 → 인스펙터
        self.scene_panel.entity_selected.connect(self.inspector.on_entity_selected)
        self.scene_panel.entity_deselected.connect(self.inspector.on_entity_deselected)

        # 씬 계층 뷰 → 상태 바. 예전에는 이름 자리에 "Entity_<id>"를 넣어서 큐브를 골라도 "Entity_11"로 보였다.
        self.scene_panel.entity_selected.connect(
            lambda eid: self.status.set_selection(eid, self._entity_display_name(eid))
        )
        self.scene_panel.entity_deselected.connect(
            lambda: self.status.set_selection(-1)
        )
        # 씬 계층 뷰 선택(여러 개 포함) → 뷰포트 선택 강조
        self.scene_panel.tree.itemSelectionChanged.connect(self._sync_engine_selection)

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
        self.inspector.authoring_status.connect(self.status.log)
        self.inspector.follow_camera_play_requested.connect(self._on_play)
        self.inspector.gizmo_tool_selected.connect(self._set_gizmo_mode)

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

        # 프로젝트 폴더(프리팹 상대 경로 기준)와 입력 액션 맵. 프로젝트를 바꿀 때도 같은 함수들을 부른다
        # (project_actions._switch_project). 데모 시드가 프리팹을 인스턴스화하기 전에 해야 상대 경로로 기록된다.
        self._set_editor_project_root()
        self.input_actions_panel.connect_engine(engine, lambda: str(current_project().input_actions_path))
        self._load_input_actions()

        # 3단계 실패는 seed_demo_scene()이 단계별로 이미 로그를 남기므로 여기서는 잡지 않는다
        # (잡을 예외는 인자 누락 같은 호출 순서 버그뿐이고, 그건 드러나야 한다).
        if not getattr(self, '_demo_scene_seeded', False):
            self._demo_scene_seeded = True
            project = current_project()
            # 마지막에 열었던 씬(세션)이 있으면 그것, 없으면 프로젝트 시작 씬.
            start = self._session_scene_path() or project.start_scene_path
            if start and start.is_file():
                # 샘플 프로젝트도 보통 프로젝트와 같은 경로: 시작 씬 파일(샘플은 assets/scenes/Demo.scene.json).
                self._load_scene_file(start)
            elif project.is_sample:
                # 대체 경로: 샘플 시작 씬 파일이 없을 때만 프로젝트 개념 이전의 코드 시드 + scene.json으로 만든다.
                self._log_engine(f"[DemoSeed] 샘플 시작 씬이 없어({start}) 코드 시드로 데모 월드를 만듭니다")
                seed_demo_scene(editor_api, registry, self._log_engine)
                self._instantiate_loaded_scene(editor_api, registry)
            else:
                self._new_scene_in_world()
            # 시작 월드(데모 시드·시작 씬)를 만든 명령들은 "저장 안 된 변경"이 아니다 - 그 뒤를 기준으로 삼는다.
            self._mark_scene_clean()
            self._apply_session_mode()

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
        self._connect_viewport_manipulation(engine, registry, editor_api)
        return editor_api

    # ── 뷰포트 조작(docs/VIEWPORT_MANIPULATION_PLAN.md) ─────────────────────────
    def _connect_viewport_manipulation(self, engine, registry, editor_api):
        if not (HAS_VIEWPORT and isinstance(self.viewport, EngineViewport)):
            return
        from core.viewport_manipulation import ViewportManipulator
        self.viewport.manipulator = ViewportManipulator(
            engine, registry, editor_api, ge_python,
            on_select=self.scene_panel.select_entity,
            on_moved=self._after_viewport_move,
            on_error=self.status.log)
        # Edit 모드의 Scene Editor 화면에서만. Play 중 좌클릭은 게임 입력이고, Motion Editor에는 씬 오브젝트가 없다.
        self.viewport.manipulation_enabled = lambda: not self.is_playing and self.stack.currentIndex() == 0
        self.viewport.manipulator.set_mode(self._gizmo_mode)   # 엔진 연결 전에 고른 도구

    def _set_gizmo_mode(self, mode: int):
        """기즈모 도구: 0 이동, 1 회전, 2 크기. Inspector Transform 줄 이름과 W/E/R이 같은 곳으로 온다."""
        self._gizmo_mode = mode
        self.inspector.set_gizmo_tool(mode)
        manipulator = getattr(self.viewport, 'manipulator', None) if HAS_VIEWPORT else None
        if manipulator is not None:
            try:
                manipulator.set_mode(mode)
            except Exception as e:
                print(f"[Viewport] 기즈모 도구 전환 실패: {e}")

    def _after_viewport_move(self, entity_id: int):
        self._update_undo_redo_actions()
        if self.inspector._current_entity_id == entity_id:
            self.inspector.on_entity_selected(entity_id)   # Inspector의 위치 칸을 드래그 결과로 다시 그린다

    def _sync_engine_selection(self):
        """트리에서 고른 엔티티들을 뷰포트에 강조한다(엔진은 Edit 상태일 때만 그린다)."""
        if self._engine is None:
            return
        from panels.scene_hierarchy import EntityItem
        entities = [to_entity(item.entity_id) for item in self.scene_panel.tree.selectedItems()
                    if isinstance(item, EntityItem) and item.entity_id >= 0]
        try:
            self._engine.SetSelectedEntities(entities)
        except Exception as e:
            print(f"[Viewport] 선택 강조 실패: {e}")

    def _entity_display_name(self, entity_id: int) -> str:
        if self._world is not None and entity_id >= 0:
            try:
                name = self._world.GetRegistry().GetEntityName(to_entity(entity_id))
                if name:
                    return name
            except Exception:
                pass
        return f"Entity_{entity_id}"

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
        # Motion Editor가 도크를 숨긴 채라면 기억을 지운다 - restoreState가 도크를 다시 보이게 하므로, 아래에서 지금
        # 모드 기준으로 다시 숨긴다(안 그러면 Motion Editor 화면에 도크가 돌아온다).
        self._docks_hidden_for_motion = None
        self.restoreState(self._default_dock_state)
        self._apply_default_dock_height()
        self._update_bottom_docks_for_mode(self.stack.currentIndex())

    # 도킹·분할 배치 저장/복원(E-6). 에디터 설정이라 config.json에 둔다(프로젝트 데이터가 아니다, R4).
    def _save_layout(self):
        import base64
        # Motion Editor에서 닫으면 도크가 숨겨진 채로 저장돼, 다음 실행의 Scene Editor에서도 도크가 사라져 있었을
        # 것이다. 숨기기 전 표시 여부로 되돌린 상태를 저장한다(창이 닫히는 중이라 화면에는 보이지 않는다).
        hidden = self._docks_hidden_for_motion
        if hidden:
            for d, visible in hidden.items():
                d.setVisible(visible)
            if hidden.get(self._raised_bottom_dock):
                self._raised_bottom_dock.raise_()
        state = bytes(self.saveState())
        if hidden:
            for d in hidden:
                d.hide()
        self.config_manager.set("layout", "dockState", value=base64.b64encode(state).decode("ascii"))
        self.config_manager.set("layout", "version", value=self._LAYOUT_VERSION)
        self.config_manager.set("layout", "splitter", value=list(self.main_splitter.sizes()))

    def _restore_layout(self):
        import base64
        from PySide6.QtCore import QByteArray
        state = self.config_manager.get("layout", "dockState", default=None)
        sizes = self.config_manager.get("layout", "splitter", default=None)
        if state and self.config_manager.get("layout", "version", default=1) != self._LAYOUT_VERSION:
            print("[Layout] 저장된 도킹 배치가 이전 형식이라 새 기본 배치를 씁니다(아래쪽 패널 높이 변경)")
            state = None
        try:
            if state:
                if not self.restoreState(QByteArray(base64.b64decode(state))):
                    print("[Layout] 저장된 도킹 배치를 복원하지 못해 기본 배치를 씁니다(형식이 다름)")
            if isinstance(sizes, list) and len(sizes) == 3 and all(isinstance(v, int) and v >= 0 for v in sizes):
                self.main_splitter.setSizes(sizes)
        except Exception as e:
            print(f"[Layout] 배치 복원 실패, 기본 배치를 씁니다: {e}")

    # ----------------------------------------------------------------
    # 엔진 제어 (Phase 2 플레이 모드 통합)
    # ----------------------------------------------------------------

    def _refresh_play_controls(self):
        """Play/Pause 버튼 표시를 is_playing·is_paused에 맞춘다. 상태를 바꾸는 곳은 이것만 부른다."""
        self.act_play.setChecked(self.is_playing and not self.is_paused)
        for act in (getattr(self, '_act_pause_menu', None), getattr(self, '_act_pause_tb', None)):
            if act is not None:
                act.setEnabled(self.is_playing)
                act.setChecked(self.is_paused)
        self.btn_mode_play.setObjectName(
            "btn_mode_active" if (self.is_playing and not self.is_paused) else "btn_mode_inactive")
        self.btn_mode_play.style().unpolish(self.btn_mode_play)
        self.btn_mode_play.style().polish(self.btn_mode_play)
        # E-3 모드 표시, E-4 편집 단축키는 Edit 모드에서만(Play 중 Delete 등은 게임 키다)
        mode = "Pause" if self.is_paused else ("Play" if self.is_playing else "Edit")
        self.status.set_mode(mode)
        # 증상(2026-10-16 점검, docs/EDITOR_BUGFIX_PLAN_2026-10-22.md F2): Play 중에도 Inspector 입력칸과
        # "＋ Component"가 켜져 있어 값을 바꿀 수 있었고, 바꾼 값은 Stop 때 안내 없이 사라졌다.
        # 원인: Inspector.set_play_mode(True)를 부르는 곳이 없었다(Stop의 set_play_mode(False)만 있었다) -
        # 읽기 전용 기능은 구현돼 있었지만 한 번도 켜진 적이 없다. 일시정지도 Play 세션이므로 읽기 전용이다.
        self.inspector.set_play_mode(self.is_playing)
        self.scene_panel.set_edit_shortcuts_enabled(not self.is_playing)
        for sc in getattr(self, '_viewport_edit_shortcuts', ()):
            sc.setEnabled(not self.is_playing)
        self._update_play_hint()
        self._update_undo_redo_actions()

    def _update_play_hint(self, viewport_has_focus=None):
        """Play 화면 위 안내 줄(E-3). 키보드 입력이 게임으로 가는지(뷰포트 포커스)를 알려 준다.
        뷰포트는 네이티브 창(HWND)이라 그 위에 Qt 위젯을 겹쳐 그릴 수 없어서, 오버레이 대신 위쪽 줄에 둔다."""
        hint = getattr(self, 'lbl_play_hint', None)
        if hint is None:
            return
        if viewport_has_focus is None:
            viewport_has_focus = self._viewport_is_engine() and self.viewport.hasFocus()
        if self.is_paused:
            text, color = "⏸ 일시정지 — Ctrl+Alt+P로 계속, Ctrl+Shift+P로 정지", COLORS['accent_orange']
        elif viewport_has_focus:
            text, color = "▶ 게임 입력 중 — 키보드·마우스가 게임으로 갑니다 (Ctrl+Alt+P 일시정지, Ctrl+Shift+P 정지)", COLORS['accent_green']
        else:
            text, color = "▶ 화면을 클릭하면 게임 입력을 받습니다", COLORS['text_secondary']
        hint.setText(text)
        hint.setStyleSheet(f"color: {color}; background-color: {COLORS['bg_header']}; font-size: 11px; padding: 3px 8px;")

    def _on_play(self):
        """플레이 모드 시작. Play 중에 누르면 일시정지, 일시정지 중에 누르면 계속."""
        if self.is_playing:
            if self.is_paused:
                self._resume()
            else:
                self._pause()
            return

        if not self.is_playing:
            # Toolbar and camera preview also reach this entry point. Without this
            # guard an unapplied speed could be displayed while Play used the old value.
            pending = self.inspector.pending_authoring_message()
            if pending:
                self.status.log(pending)
                return
            self.is_playing = True
            self.is_paused = False
            # Stop하면 월드가 Play 시작 시점으로 되돌아가므로, 저장 안 된 변경 표시도 그때 상태로 돌린다.
            self._remember_dirty_before_play()

            # 플레이 모드로 전환 (버튼 활성 상태 갱신 포함 — Motion Editor 등 다른
            # 모드에서 넘어올 때 이전 버튼 강조가 남아있지 않도록 _set_mode로 일원화)
            self._set_mode(1)

            # 엔진 플레이 시작
            if self._engine and HAS_ENGINE:
                try:
                    # 월드 플레이 시작
                    if self._world:
                        # ScriptComponent의 모듈은 프로젝트 scripts/에서 import된다.
                        self._add_project_scripts_to_path()
                        self._world.Play()
                        # Sound Lite Phase 6 - 액션 재생 시작(§2.4). 액션 경로가 상대
                        # 경로면 현재 프로젝트의 assets 폴더 기준으로 푼다(Player도 같은 규칙,
                        # runtime/python/runtime_bootstrap.py).
                        assets_dir = str(current_project().assets_dir)
                        started = self._scene_playback.start(self._world.GetRegistry(), assets_dir)
                        if started:
                            self.status.log(f"액션 재생 시작: {started}개")
                    self.status.log("Play mode started")
                except Exception as e:
                    print(f"[Editor] Play start error: {e}")
                    self.status.log(f"Play error: {e}")
                    self._on_stop()
                    return
            else:
                self.status.log("Play mode (no engine)")
            
            self._refresh_play_controls()
            # 게임 입력(키보드)이 바로 뷰포트로 가게 한다. 포커스가 툴바에 남아 있으면 첫 키 입력을 놓친다.
            if self._viewport_is_engine():
                self.viewport.setFocus()
            print("[Editor] Play mode started")

    def _on_pause_toggle(self):
        """Pause 메뉴/툴바/Ctrl+Alt+P. Play 중이 아니면 아무것도 하지 않는다."""
        if not self.is_playing:
            self._refresh_play_controls()   # checkable 액션이 눌린 상태로 남지 않게
            return
        if self.is_paused:
            self._resume()
        else:
            self._pause()

    def _resume(self):
        self.is_paused = False
        if self._engine and HAS_ENGINE and self._world:
            try:
                self._world.Play()   # 일시정지 상태에서 Play()는 스냅샷을 다시 찍지 않고 이어서 돈다(World::Play)
                self.status.log("Resumed")
            except Exception as e:
                print(f"[Editor] Resume error: {e}")
        self._refresh_play_controls()
        if self._viewport_is_engine():
            self.viewport.setFocus()

    def _pause(self):
        """일시정지 처리. Play 세션은 유지된다(is_playing은 그대로 True)."""
        self.is_paused = True
        self._refresh_play_controls()

        # Sound Lite Phase 6 - 일시정지하면 울리던 소리도 멈춘다. 재생 커서는 그대로 두고,
        # 일시정지 동안에는 _on_fps_tick이 커서를 전진시키지 않으므로 계속하면 이어서 재생된다.
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
        was_playing = self.is_playing
        self.is_playing = False
        self.is_paused = False
        self._refresh_play_controls()

        # Sound Lite Phase 6 - 소리가 Scene 모드로 새어나가지 않게 한다.
        self._scene_playback.stop()

        # 씬 에디터 모드로 복귀 (버튼 활성 상태 갱신 포함)
        self._set_mode(0)

        # 엔진 정지
        if self._engine and HAS_ENGINE:
            try:
                if self._world:
                    registry = self._world.GetRegistry()
                    selected_id = self.inspector._current_entity_id
                    selected_uuid = registry.GetEntityUUID(to_entity(selected_id)) if selected_id >= 0 else 0
                    self._world.Stop()
                    # Stop restores entities with new runtime IDs. Keeping the old
                    # selected ID used to empty the Inspector after a camera test.
                    restored = registry.GetEntityByUUID(selected_uuid)
                    self.inspector._current_entity_id = restored.id if restored.IsValid() else -1
                    self.scene_panel._selected_entity_id = self.inspector._current_entity_id
                    self.scene_panel._last_structure = None
                    self.scene_panel.refresh_from_engine()
                    self._sync_engine_selection()   # 강조도 새 id로(Stop이 엔티티를 새 id로 다시 만든다)
                self.status.log("Stopped")
            except Exception as e:
                print(f"[Editor] Stop error: {e}")
        
        self.inspector.set_play_mode(False)
        
        # 롤백된 데이터를 UI에 반영하기 위해 위젯 리프레시
        if self.inspector._current_entity_id != -1:
            self.inspector.on_entity_selected(self.inspector._current_entity_id)
            
        self._update_undo_redo_actions()
        self.status.log("Play 모드 종료 — 이전 편집을 Undo/Redo할 수 있습니다")
        if was_playing:
            self._restore_dirty_after_stop()

    # ----------------------------------------------------------------
    # 데모 씬 통합 메서드
    # ----------------------------------------------------------------

    def _load_default_scene(self):
        """scene.json 파싱 (샘플 프로젝트의 대체 경로 전용 — 시작 씬 파일이 있으면 엔진이 그 파일을 불러온다)"""
        project = current_project()
        if not project.is_sample:
            return
        if project.start_scene_path and project.start_scene_path.is_file():
            return   # Demo.scene.json이 데모 월드 전체를 담고 있다(scene.json 내용 포함, X-1b)
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
        
        # 엔진의 에셋 기준 폴더는 config.json의 paths가 아니라 현재 프로젝트에서 정한다
        # (viewport.py가 엔진을 만들 때, project_actions._switch_project가 프로젝트를 바꿀 때).
        # 예전에는 여기서 quarterflying.PathResolver.Init을 불렀는데, 그 바인딩은 존재하지 않아 매번
        # "module 'quarterflying' has no attribute 'PathResolver'"로 실패했고, C++의 PathResolver도
        # 엔진 어디에서도 쓰이지 않는다(셰이더는 C++ 코드에 문자열로 들어 있다).

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
        """종료 시: 저장 안 된 변경 확인 → 프로젝트 세션 → 에디터 설정.

        예전에는 묻지 않고 바로 닫혀서, 저장 안 한 씬·액션·입력 맵 수정이 경고 없이 사라졌다.
        Play 중이면 먼저 멈춘다 - 변경 여부와 세션은 Play 전 월드 기준이다(Play 중 월드는 저장 대상이 아니다).
        """
        if self.is_playing:
            self._on_stop()
        try:
            proceed = self._confirm_unsaved("Exit", include_project_data=True)
        except Exception as e:
            # 확인 자체가 실패하면 변경이 있는지 모른다. 예전처럼 그냥 닫으면(Qt는 closeEvent의 예외를 삼키고
            # 창을 닫는다) 작업이 경고 없이 사라진다. 처음 연결할 때 InputActionsPanel.is_dirty(프로퍼티)를
            # 메서드로 불러 "'bool' object is not callable"이 났고, 창이 묻지 않고 닫혔다.
            import traceback
            from PySide6.QtWidgets import QMessageBox
            traceback.print_exc()
            proceed = QMessageBox.question(
                self, "Exit", f"저장 안 된 변경을 확인하지 못했습니다:\n{type(e).__name__}: {e}\n\n"
                "저장하지 않고 종료할까요?", QMessageBox.Yes | QMessageBox.No, QMessageBox.No) == QMessageBox.Yes
        if not proceed:
            event.ignore()
            return
        self._write_session_quietly()
        self._save_layout()
        self.config_manager.save_config()
        super().closeEvent(event)

    # ----------------------------------------------------------------
    # FPS 틱
    # ----------------------------------------------------------------

    def _on_fps_tick(self):
        # 여기서 self.viewport.tick()을 부르지 않는다. EngineViewport는 자기 16ms 타이머로 이미
        # 틱한다. 예전에는 여기서도 불러서 Scene 엔진이 프레임당 두 번(약 120Hz) 틱했다.
        self.status.tick_frame()
        self._poll_scene_dirty()   # 창 제목의 '*' (project_actions.py)

        # 이 타이머는 60fps 고정인데 액션의 fps는 다를 수 있다. 틱 수를 프레임 수로
        # 쓰면 30fps 액션이 두 배 빨리 재생되므로 **실제 경과 시간**으로 전진시킨다.
        now = time.perf_counter()
        dt = now - self._last_tick_time
        self._last_tick_time = now
        if self.is_playing and not self.is_paused:
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
