"""
GE Engine Editor — Main Window
3분할 레이아웃: 씬 계층 뷰 | 3D 뷰포트 | 인스펙터

Stage 1: 더미 데이터로 동작 (ge_python 없어도 시작 가능)
Stage 2: ge_python 바인딩 연결 시 실제 ECS 연동
"""

import sys
import os

# 에디터 디렉토리를 경로에 추가
_EDITOR_DIR = os.path.dirname(os.path.abspath(__file__))
if _EDITOR_DIR not in sys.path:
    sys.path.insert(0, _EDITOR_DIR)

from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QSplitter,
    QVBoxLayout, QHBoxLayout, QLabel, QToolBar,
    QSizePolicy, QMenuBar, QMenu, QStatusBar, QStackedWidget, QPushButton
)
from PySide6.QtCore import Qt, QTimer, QSize
from PySide6.QtGui import QAction, QFont, QIcon, QKeySequence

# 패널 임포트
from panels.scene_hierarchy import SceneHierarchyPanel
from panels.inspector import InspectorPanel
from panels.status_bar import StatusBar
from style.theme import apply_theme

# 데모 씬 통합 임포트
from demo_scene_integration import DemoSceneIntegration

# 설정 관리자 임포트
from config_manager import ConfigManager

# 모션 에디터 임포트
try:
    from motion_editor import MotionEditorWidget
    HAS_MOTION_EDITOR = True
except Exception as e:
    print(f"[Editor] Motion Editor 로드 실패: {e}")
    HAS_MOTION_EDITOR = False

# 뷰포트 임포트 (ge_python 없으면 더미 뷰포트 사용)
try:
    from viewport import EngineViewport
    HAS_VIEWPORT = True
except Exception as e:
    print(f"[Editor] 뷰포트 로드 실패 (더미 모드): {e}")
    HAS_VIEWPORT = False

try:
    import ge_python
    HAS_ENGINE = True
except ImportError:
    HAS_ENGINE = False


# ============================================================
# 더미 뷰포트 (ge_python 없이도 UI 테스트 가능)
# ============================================================

class DummyViewport(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(400, 300)
        self.setStyleSheet("background-color: #111827;")

        layout = QVBoxLayout(self)
        layout.setAlignment(Qt.AlignCenter)

        icon = QLabel("🎮")
        icon.setAlignment(Qt.AlignCenter)
        icon.setStyleSheet("font-size: 48px; background: transparent;")
        layout.addWidget(icon)

        msg = QLabel("3D Viewport\n(ge_python 바인딩 필요)")
        msg.setAlignment(Qt.AlignCenter)
        msg.setStyleSheet(
            "color: #334455; font-size: 14px; "
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

        self._engine = None      # ge_python.Engine (연동 후)
        self._world = None       # ge_python.World  (연동 후)
        self.is_playing = False  # 플레이 모드 상태
        
        # 설정 관리자
        self.config_manager = ConfigManager()

        self._build_menu()
        self._build_toolbar()
        self._build_central_widget()
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

        # 씬 이름
        self.lbl_scene = QLabel("  Untitled Scene  ")
        self.lbl_scene.setStyleSheet("color: #8899aa; font-size: 11px; padding: 0 8px;")
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
            self.viewport = EngineViewport()
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
        
        # 플레이 뷰포트
        if HAS_VIEWPORT and HAS_ENGINE:
            self.play_viewport = EngineViewport()
        else:
            self.play_viewport = DummyViewport()
        self.play_viewport.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)
        play_layout.addWidget(self.play_viewport)
        
        self.stack.addWidget(self.play_mode_widget)

        # --- Page 2: Motion Editor ---
        if HAS_MOTION_EDITOR:
            self.motion_editor = MotionEditorWidget()
            self.stack.addWidget(self.motion_editor)
        else:
            dummy_motion = QWidget()
            dl = QVBoxLayout(dummy_motion)
            dl.addWidget(QLabel("Motion Editor 로드 실패"))
            self.stack.addWidget(dummy_motion)

    def _set_mode(self, index: int):
        self.stack.setCurrentIndex(index)
        
        # 버튼 상태 초기화
        self.btn_mode_scene.setObjectName("btn_mode_inactive")
        self.btn_mode_motion.setObjectName("btn_mode_inactive")
        
        if index == 0:  # Scene Editor
            self.btn_mode_scene.setObjectName("btn_mode_active")
        elif index == 1:  # Play Mode
            self.btn_mode_scene.setObjectName("btn_mode_active")  # Play는 Scene 모드의 확장
        elif index == 2:  # Motion Editor
            self.btn_mode_motion.setObjectName("btn_mode_active")
            
        # 스타일 강제 갱신
        self.btn_mode_scene.style().unpolish(self.btn_mode_scene)
        self.btn_mode_scene.style().polish(self.btn_mode_scene)
        self.btn_mode_motion.style().unpolish(self.btn_mode_motion)
        self.btn_mode_motion.style().polish(self.btn_mode_motion)

    # ----------------------------------------------------------------
    # 상태 바
    # ----------------------------------------------------------------

    def _build_status_bar(self):
        self.status = StatusBar()
        # QMainWindow 기본 상태바 교체
        self.setStatusBar(None)
        self.centralWidget().layout().addWidget(self.status)

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

    # ----------------------------------------------------------------
    # 2단계: 엔진 ECS 연동
    # ----------------------------------------------------------------

    def _try_connect_engine(self):
        """
        ge_python 바인딩이 있으면 엔진 ECS를 패널에 연결.
        없으면 더미 모드로 계속 동작.
        """
        if not HAS_ENGINE:
            self.status.set_engine_online(False)
            self.status.log("더미 모드로 실행 중 (ge_python 없음)")
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
        """뷰포트 초기화 후 ECS 연동"""
        try:
            if not (HAS_VIEWPORT and isinstance(self.viewport, EngineViewport)):
                return
            if not self.viewport.initialized:
                return

            engine = self.viewport.engine
            world = engine.GetActiveWorld()
            if world is None:
                world = engine.CreateWorld()
            registry = world.GetRegistry()

            # 패널에 registry 연결
            self.scene_panel.connect_registry(registry)
            self.inspector.connect_registry(registry)

            self._engine = engine
            self._world = world
            self.status.set_engine_online(True)
            self.status.log("엔진 ECS 연결 완료")
            print("[Editor] 엔진 ECS 연결 완료")

            import ge_python
            ge_python.EditorAPI.get_instance().set_registry(registry)
            self.inspector.connect_editor(ge_python.EditorAPI.get_instance())
            self._update_undo_redo_actions()

        except Exception as e:
            print(f"[Editor] ECS 연결 실패: {e}")
            self.status.log(f"ECS 연결 실패: {e}")

    def _on_undo(self):
        if not HAS_ENGINE:
            return
        try:
            import ge_python
            ge_python.CommandManager.get_instance().undo()
            self._after_edit_command()
        except ge_python.EngineError as e:
            self.status.log(f"Undo failed: {e}")

    def _on_redo(self):
        if not HAS_ENGINE:
            return
        try:
            import ge_python
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
            import ge_python
            cm = ge_python.CommandManager.get_instance()
            self._act_undo.setEnabled(cm.can_undo())
            self._act_redo.setEnabled(cm.can_redo())
        except Exception:
            self._act_undo.setEnabled(False)
            self._act_redo.setEnabled(False)

    # ----------------------------------------------------------------
    # 레이아웃 리셋
    # ----------------------------------------------------------------

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
            self.btn_mode_play.setObjectName("btn_mode_active")
            
            # 플레이 모드로 전환
            self.stack.setCurrentWidget(self.play_mode_widget)
            
            # 엔진 플레이 시작
            if self._engine and HAS_ENGINE:
                try:
                    # 월드 플레이 시작
                    if self._world:
                        self._world.Play()
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
        self.btn_mode_play.setObjectName("btn_mode_inactive")
        
        # 씬 에디터 모드로 복귀
        self.stack.setCurrentWidget(self.scene_editor_widget)
        
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
        scene_path = "assets/scene.json"
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
        try:
            import ge_python
            ge_python.PathResolver.Init(asset_root, shader_root)
            print(f"[Editor] PathResolver initialized: {asset_root}, {shader_root}")
        except ImportError:
            print("[Editor] ge_python not available for PathResolver")
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
        self.viewport.tick()
        self.status.tick_frame()

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
    window.show()

    sys.exit(app.exec())
