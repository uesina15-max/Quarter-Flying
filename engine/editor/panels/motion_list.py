"""
editor/panels/motion_list.py

Motion List Panel — 액션 목록.

역할:
  - ActionData 목록을 QListWidget으로 표시
  - 추가 / 삭제 / 이름 변경
  - 선택 변경 시 action_selected 시그널 발신
"""

from __future__ import annotations

from typing import List, Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QListWidget, QListWidgetItem, QPushButton,
    QInputDialog, QMessageBox
)
from PySide6.QtCore import Qt, Signal
from PySide6.QtGui import QFont

from core.action_data import ActionData, make_default_action


class MotionListPanel(QWidget):
    """
    왼쪽 패널: 현재 프로젝트의 ActionData 목록.

    Signals:
        action_selected(ActionData)  — 새 항목 선택 시
        action_deselected()          — 선택 해제 시
        action_list_changed()        — 추가/삭제 등 리스트 변경 시
    """

    action_selected   = Signal(object)   # ActionData
    action_deselected = Signal()
    action_list_changed = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._actions: List[ActionData] = []
        self._build_ui()

    # ── UI 구성 ──────────────────────────────────────────────────────────────

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 헤더
        header = QLabel("  MOTION LIST")
        header.setFixedHeight(28)
        header.setObjectName("panel_header")
        layout.addWidget(header)

        # 목록
        self.list_widget = QListWidget()
        self.list_widget.setAlternatingRowColors(False)
        self.list_widget.setSpacing(1)
        self.list_widget.currentItemChanged.connect(self._on_selection_changed)
        self.list_widget.itemDoubleClicked.connect(self._on_rename)
        layout.addWidget(self.list_widget)

        # 버튼 행
        btn_row = QWidget()
        btn_row.setFixedHeight(38)
        btn_layout = QHBoxLayout(btn_row)
        btn_layout.setContentsMargins(6, 4, 6, 4)
        btn_layout.setSpacing(6)

        self.btn_add = QPushButton("＋ Add")
        self.btn_add.setObjectName("btn_create")
        self.btn_add.setToolTip("새 액션 추가")
        self.btn_add.clicked.connect(self._on_add)

        self.btn_remove = QPushButton("－ Remove")
        self.btn_remove.setObjectName("btn_delete")
        self.btn_remove.setToolTip("선택된 액션 삭제")
        self.btn_remove.clicked.connect(self._on_remove)
        self.btn_remove.setEnabled(False)

        btn_layout.addWidget(self.btn_add)
        btn_layout.addWidget(self.btn_remove)
        layout.addWidget(btn_row)

    # ── 공개 API ─────────────────────────────────────────────────────────────

    def set_actions(self, actions: List[ActionData]) -> None:
        """외부에서 액션 목록 전체를 교체."""
        self._actions = actions
        self._refresh_list()

    def get_actions(self) -> List[ActionData]:
        return self._actions

    def current_action(self) -> Optional[ActionData]:
        idx = self.list_widget.currentRow()
        if 0 <= idx < len(self._actions):
            return self._actions[idx]
        return None

    def select_action(self, action: ActionData) -> None:
        """특정 ActionData를 선택 상태로."""
        try:
            idx = self._actions.index(action)
            self.list_widget.setCurrentRow(idx)
        except ValueError:
            pass

    def notify_action_renamed(self, action: ActionData) -> None:
        """외부에서 이름 변경됐을 때 목록 갱신."""
        self._refresh_list()

    # ── 내부 ─────────────────────────────────────────────────────────────────

    def _refresh_list(self):
        prev_row = self.list_widget.currentRow()
        self.list_widget.blockSignals(True)
        self.list_widget.clear()

        for action in self._actions:
            item = QListWidgetItem(f"  {action.name}")
            item.setToolTip(
                f"FPS: {action.fps}  |  Frames: {action.total_frames}\n"
                f"Sections: {len(action.sections)}  Events: {len(action.events)}"
            )
            self.list_widget.addItem(item)

        self.list_widget.blockSignals(False)

        # 이전 선택 복원
        if self._actions:
            new_row = min(prev_row, len(self._actions) - 1)
            new_row = max(0, new_row)
            self.list_widget.setCurrentRow(new_row)
        else:
            self.action_deselected.emit()
            self.btn_remove.setEnabled(False)

    def _on_selection_changed(self, current: QListWidgetItem, _prev):
        if current is None:
            self.action_deselected.emit()
            self.btn_remove.setEnabled(False)
            return

        idx = self.list_widget.row(current)
        if 0 <= idx < len(self._actions):
            self.action_selected.emit(self._actions[idx])
            self.btn_remove.setEnabled(True)

    def _on_add(self):
        name, ok = QInputDialog.getText(
            self, "새 액션", "액션 이름:",
            text=f"Action{len(self._actions) + 1:02d}"
        )
        if not ok or not name.strip():
            return

        new_action = make_default_action(name.strip())
        self._actions.append(new_action)
        self._refresh_list()
        self.list_widget.setCurrentRow(len(self._actions) - 1)
        self.action_list_changed.emit()

    def _on_remove(self):
        idx = self.list_widget.currentRow()
        if not (0 <= idx < len(self._actions)):
            return

        action = self._actions[idx]
        result = QMessageBox.question(
            self, "삭제 확인",
            f"'{action.name}' 을(를) 삭제하시겠습니까?",
            QMessageBox.Yes | QMessageBox.No,
        )
        if result != QMessageBox.Yes:
            return

        self._actions.pop(idx)
        self._refresh_list()
        self.action_list_changed.emit()

    def _on_rename(self, item: QListWidgetItem):
        idx = self.list_widget.row(item)
        if not (0 <= idx < len(self._actions)):
            return

        action = self._actions[idx]
        name, ok = QInputDialog.getText(
            self, "이름 변경", "새 이름:", text=action.name
        )
        if ok and name.strip():
            action.name = name.strip()
            self._refresh_list()
            self.list_widget.setCurrentRow(idx)
            self.action_list_changed.emit()
