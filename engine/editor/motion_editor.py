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
        QMessageBox.information(self, "Import FBX", "1단계에서는 더미 뷰어를 사용합니다.\nFBX 파싱은 다음 단계에서 연동됩니다.")

    def do_save_action(self):
        if not self._current_action:
            QMessageBox.warning(self, "저장 불가", "선택된 액션이 없습니다.")
            return
            
        filepath, _ = QFileDialog.getSaveFileName(
            self, "액션 저장", f"{self._current_action.name}.action.json", "Action Files (*.action.json)"
        )
        if filepath:
            try:
                save_action(self._current_action, filepath)
                QMessageBox.information(self, "저장 완료", f"'{filepath}' 에 저장되었습니다.")
            except Exception as e:
                QMessageBox.critical(self, "저장 실패", str(e))

    def do_load_action(self):
        filepath, _ = QFileDialog.getOpenFileName(
            self, "액션 열기", "", "Action Files (*.action.json)"
        )
        if filepath:
            try:
                action = load_action(filepath)
                # 현재 목록에 추가
                actions = self.motion_list.get_actions()
                actions.append(action)
                self.motion_list.set_actions(actions)
                self.motion_list.select_action(action)
            except Exception as e:
                QMessageBox.critical(self, "열기 실패", str(e))
