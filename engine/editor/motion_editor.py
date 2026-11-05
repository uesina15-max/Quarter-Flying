"""
editor/motion_editor.py

Motion Editor 메인 위젯 조립.
4+1 패널 (List, Preview, Inspector, Timeline, Graph)을 묶고 시그널을 라우팅한다.
"""

from __future__ import annotations

import os
from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QSplitter, QMessageBox, QFileDialog
)
from PySide6.QtCore import Qt

from core.action_data import ActionData, save_action, load_action

from panels.motion_list import MotionListPanel
from panels.animation_preview import AnimationPreviewPanel
from panels.section_inspector import SectionInspectorPanel
from panels.event_timeline import EventTimelinePanel
from panels.motion_graph import MotionGraphPanel


class MotionEditorWidget(QWidget):
    """
    Motion Editor의 최상위 위젯. main.py에서 QStackedWidget의 한 페이지로 들어간다.
    """
    def __init__(self, parent=None, shared_viewport=None):
        super().__init__(parent)
        self._current_action: Optional[ActionData] = None
        # Scene Editor와 공유하는 단일 EngineViewport 인스턴스 (착수 계약서 §C11) - 여기서
        # 소유하지 않고 AnimationPreviewPanel에 그대로 넘긴다. None이면(엔진 없음/더미 모드)
        # 기존 2D 프리뷰로 폴백한다.
        self._shared_viewport = shared_viewport
        self._build_ui()
        self._connect_signals()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 메인 수직 스플리터 (상단 3분할 / 하단 타임라인)
        self.main_v_splitter = QSplitter(Qt.Vertical)
        layout.addWidget(self.main_v_splitter)

        # 상단 수평 스플리터 (List | Preview | Right)
        self.top_h_splitter = QSplitter(Qt.Horizontal)
        self.main_v_splitter.addWidget(self.top_h_splitter)

        # 1. 왼쪽: Motion List
        self.motion_list = MotionListPanel()
        self.top_h_splitter.addWidget(self.motion_list)

        # 2. 중앙: Animation Preview (Motion Mixer 프리뷰 - Scene Editor와 공유 뷰포트)
        self.preview = AnimationPreviewPanel(shared_viewport=self._shared_viewport)
        self.top_h_splitter.addWidget(self.preview)

        # 3. 오른쪽 스플리터 (Inspector / Graph)
        self.right_v_splitter = QSplitter(Qt.Vertical)
        self.top_h_splitter.addWidget(self.right_v_splitter)

        self.inspector = SectionInspectorPanel()
        self.right_v_splitter.addWidget(self.inspector)

        self.graph = MotionGraphPanel()
        self.right_v_splitter.addWidget(self.graph)

        # 4. 하단: 타임라인
        self.timeline = EventTimelinePanel()
        self.main_v_splitter.addWidget(self.timeline)

        # 스플리터 비율 설정 — 뷰포트(중앙)와 모션 그래프(우측 하단)가 레이아웃의
        # 중심이 되도록 재조정한다("모션 에디터는 뷰포트와 모션 그래프 위주로").
        # 이전엔 Section Inspector(600) : Motion Graph(200) = 3:1로 그래프가 구석에
        # 눌려 있었다 - 이제 그래프가 Inspector보다 더 넓은 영역을 받는다.
        self.top_h_splitter.setSizes([170, 720, 310])
        self.right_v_splitter.setSizes([320, 480])
        self.main_v_splitter.setSizes([720, 180])
        # 창을 키우면 늘어난 만큼 뷰포트(중앙·위)만 커지게 한다. 예전에는 스트레치를 정하지 않아 Qt가 지금 비율대로
        # 나눠서, 넓은 화면일수록 액션 목록·오른쪽 패널·타임라인도 같이 커졌다.
        self.top_h_splitter.setStretchFactor(0, 0)
        self.top_h_splitter.setStretchFactor(1, 1)
        self.top_h_splitter.setStretchFactor(2, 0)
        self.main_v_splitter.setStretchFactor(0, 1)
        self.main_v_splitter.setStretchFactor(1, 0)

    def _connect_signals(self):
        # List -> Others (액션 선택 변경)
        self.motion_list.action_selected.connect(self._on_action_selected)
        self.motion_list.action_deselected.connect(self._on_action_deselected)
        
        # Frame 동기화 (Preview <-> Timeline <-> Graph)
        self.preview.frame_changed.connect(self.timeline.set_current_frame)
        self.preview.frame_changed.connect(self.graph.set_current_frame)
        
        self.timeline.frame_changed.connect(self.preview.set_current_frame)
        self.timeline.frame_changed.connect(self.graph.set_current_frame)
        
        self.graph.frame_changed.connect(self.preview.set_current_frame)
        self.graph.frame_changed.connect(self.timeline.set_current_frame)

        # Data 변경 동기화
        self.inspector.data_changed.connect(self._on_data_changed)
        self.timeline.data_changed.connect(self._on_data_changed)

    # ── 시그널 핸들러 ────────────────────────────────────────────────────────

    def _on_action_selected(self, action: ActionData):
        self._current_action = action
        self.preview.set_action(action)
        self.inspector.set_action(action)
        self.timeline.set_action(action)
        self.graph.set_action(action)

    def _on_action_deselected(self):
        self._current_action = None
        self.preview.set_action(None)
        self.inspector.set_action(None)
        self.timeline.set_action(None)
        self.graph.set_action(None)

    def _on_data_changed(self):
        # 뷰포트나 타임라인 다시 그리기
        # preview.canvas는 공유 EngineViewport가 붙어있는 동안(§C11) None이다 - 그 경우
        # 다시 그릴 2D 더미 캔버스가 없으므로 건너뛴다(엔진 뷰포트는 자체 타이머로 계속
        # TickFrame을 돌며 그린다).
        if self.preview.canvas is not None:
            self.preview.canvas.update()
        self.timeline.canvas.update()
        self.graph.canvas.update()
        
        if self._current_action:
            self.motion_list.notify_action_renamed(self._current_action)

    # ── Motion Mixer 프리뷰 (Phase 4A, main.py의 _set_mode가 모드 전환 시 호출) ──────

    def attach_viewport(self):
        self.preview.attach_viewport()

    def activate_motion_preview(self):
        self.preview.activate_motion_preview()

    def deactivate_motion_preview(self):
        self.preview.deactivate_motion_preview()

    # ── 외부에서 호출할 Actions (메뉴바 등에서 연결) ─────────────────────────

    def do_import_fbx(self):
        # 메뉴 항목은 꺼져 있다(main.py, A-1). 다른 곳에서 직접 부르더라도 지원하지 않는다는 사실만 알린다.
        QMessageBox.information(self, "Import FBX",
                                "FBX 가져오기는 아직 지원하지 않습니다.\n"
                                "스켈레톤·클립은 JSON(*.skeleton.json, *.clip.json)으로 assets/motion에 넣어 쓰세요.")

    # ── 액션 파일 (A-4) ─────────────────────────────────────────────────────
    # 최근 파일 목록은 에디터 설정(config.json)이다 - main.py가 recent_store(get/set을 가진 ConfigManager)를 넣어 준다.
    recent_store = None
    MAX_RECENT = 8

    @staticmethod
    def _actions_dir() -> str:
        from project import current_project
        return str(current_project().actions_dir)

    def recent_actions(self) -> list:
        if self.recent_store is None:
            return []
        items = self.recent_store.get("motion", "recentActions", default=[]) or []
        return [p for p in items if isinstance(p, str)]

    def _remember_recent(self, path: str):
        if self.recent_store is None:
            return
        path = os.path.abspath(path)
        items = [p for p in self.recent_actions() if os.path.normcase(p) != os.path.normcase(path)]
        self.recent_store.set("motion", "recentActions", value=([path] + items)[: self.MAX_RECENT])

    def do_new_action(self):
        self.motion_list._on_add()

    def do_save_action(self):
        """이미 파일이 있는 액션은 그 파일에 바로 저장한다(대화상자 없음). 예전에는 저장할 때마다 대화상자가 떴고 기본
        폴더도 없어서(작업 폴더 기준) 매번 위치를 다시 골라야 했다. 새 액션은 Save As로 넘어간다."""
        if not self._current_action:
            QMessageBox.warning(self, "저장 불가", "선택된 액션이 없습니다.")
            return False
        if not self._current_action.source_path:
            return self.do_save_action_as()
        return self._save_to(self._current_action.source_path)

    def do_save_action_as(self):
        if not self._current_action:
            QMessageBox.warning(self, "저장 불가", "선택된 액션이 없습니다.")
            return False
        return self._save_as(self._current_action)

    def _save_as(self, action: ActionData) -> bool:
        os.makedirs(self._actions_dir(), exist_ok=True)
        default = os.path.join(self._actions_dir(), f"{action.name}.action.json")
        filepath, _ = QFileDialog.getSaveFileName(self, f"액션 저장 - {action.name}", default,
                                                  "Action Files (*.action.json)")
        if not filepath:
            return False
        return self._save_to(filepath, action)

    def _save_to(self, filepath: str, action: Optional[ActionData] = None) -> bool:
        action = action or self._current_action
        try:
            save_action(action, filepath)
        except Exception as e:
            QMessageBox.critical(self, "저장 실패", f"{filepath}\n{e}")
            return False
        self._remember_recent(filepath)
        self.status_message(f"액션 저장: {os.path.basename(filepath)}")
        return True

    # ── 프로젝트 저장·세션 (File > Save Project, project/session.py) ─────────────
    # 예전에는 액션 목록이 메모리에만 있어서, 저장하지 않은 액션이나 Save 안 한 수정은 에디터를 닫으면 아무 경고 없이
    # 사라졌고, 다시 열면 목록이 비어 있었다(열어 둔 액션을 하나씩 다시 Load해야 했다).

    def unsaved_actions(self) -> list:
        """파일과 내용이 다른 액션(파일이 없는 새 액션 포함). 기준은 ActionData.is_modified() 하나다."""
        return [a for a in self.motion_list.get_actions() if a.is_modified()]

    def save_action_data(self, action: ActionData) -> bool:
        """선택과 무관하게 이 액션을 저장한다. 파일이 없는 새 액션이면 저장 위치를 묻는다."""
        if action.source_path:
            return self._save_to(action.source_path, action)
        return self._save_as(action)

    def open_action_paths(self) -> list:
        """목록에 있는 액션 중 파일이 있는 것들의 경로(목록 순서). 세션에 적는다."""
        return [a.source_path for a in self.motion_list.get_actions() if a.source_path]

    def selected_action_path(self) -> str:
        action = self.motion_list.current_action()
        return (action.source_path or "") if action is not None else ""

    def restore_actions(self, paths: list, selected: str = "") -> list:
        """목록을 이 파일들로 바꾼다(프로젝트를 열 때). 못 읽은 파일은 건너뛰고 이유를 돌려준다 - 대화상자를
        띄우지 않는다(세션 복원 중에 창이 여러 개 뜨면 안 된다. 호출부가 로그로 남긴다)."""
        actions, warnings = [], []
        for path in paths:
            try:
                actions.append(load_action(str(path)))
            except Exception as e:
                warnings.append(f"{path}: {type(e).__name__}: {e}")
        self.motion_list.set_actions(actions)
        wanted = os.path.normcase(os.path.abspath(selected)) if selected else ""
        chosen = next((a for a in actions if wanted and os.path.normcase(a.source_path or "") == wanted), None)
        if chosen is not None:
            self.motion_list.select_action(chosen)
        return warnings

    def do_load_action(self, filepath: str = ""):
        if not filepath:
            filepath, _ = QFileDialog.getOpenFileName(self, "액션 열기", self._actions_dir(), "Action Files (*.action.json)")
        if not filepath:
            return None
        try:
            action = load_action(filepath)
        except Exception as e:
            QMessageBox.critical(self, "열기 실패", f"{filepath}\n{e}")
            return None
        actions = self.motion_list.get_actions()
        actions.append(action)
        self.motion_list.set_actions(actions)
        self.motion_list.select_action(action)
        self._remember_recent(filepath)
        return action

    def status_message(self, text: str):
        """main.py가 상태바로 바꿔 끼운다. 기본은 콘솔."""
        print(f"[MotionEditor] {text}")
