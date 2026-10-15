"""
editor/panels/input_actions_panel.py

Input Actions 패널 — 프로젝트의 입력 액션 맵(assets/input/actions.json)을 표로 편집한다
(docs/USABILITY_IMPROVEMENT_PLAN.md G2).

에디터 GUI(PySide6)지만 다루는 것은 프로젝트 데이터(게임 조작)다. 에디터 단축키와는 별개다(Help > Keyboard Shortcuts).

형식은 엔진(engine/input/InputActions.h)만 안다. 이 패널은 파일을 직접 읽거나 쓰지 않는다:
  - 표시: Engine.GetInputActionsJson()(엔진이 만든 정규 JSON)을 그대로 보여 준다.
  - 편집: 각 액션의 "bindings" 칸은 파일의 bindings 배열과 같은 JSON 텍스트다. 고치면 Engine.SetInputActionJson으로
    보내고, 엔진이 거절하거나(문법·타입) 건너뛴 바인딩(모르는 키 이름 등)을 그대로 보여 준다.
  - 저장: Engine.SaveInputActions(프로젝트 경로).
그래서 패널과 C++ 로더의 형식 해석이 어긋날 수 없다. 편집은 실행 중인 엔진에 바로 반영된다(Play 중이면 다음 프레임부터).
"""

from __future__ import annotations

import json
from typing import Callable, Optional

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QHBoxLayout, QHeaderView, QInputDialog, QLabel, QPushButton, QTableWidget,
                               QTableWidgetItem, QVBoxLayout, QWidget)

from style.theme import COLORS

_COL_NAME, _COL_TYPE, _COL_BINDINGS = range(3)


class InputActionsPanel(QWidget):
    status_message = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._engine = None
        self._path_provider: Optional[Callable[[], str]] = None
        self._dirty = False
        self._filling = False
        self._build_ui()
        self._set_enabled(False)

    # ── UI ───────────────────────────────────────────────────────────────────

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(4, 4, 4, 4)
        layout.setSpacing(4)

        row = QHBoxLayout()
        self.btn_add = QPushButton("＋ 액션")
        self.btn_remove = QPushButton("삭제")
        self.btn_save = QPushButton("저장")
        self.btn_reload = QPushButton("파일에서 다시 읽기")
        self.btn_add.clicked.connect(self._on_add)
        self.btn_remove.clicked.connect(self._on_remove)
        self.btn_save.clicked.connect(self.save)
        self.btn_reload.clicked.connect(self._on_reload)
        for b in (self.btn_add, self.btn_remove, self.btn_save, self.btn_reload):
            row.addWidget(b)
        row.addStretch()
        layout.addLayout(row)

        self.table = QTableWidget(0, 3)
        self.table.setHorizontalHeaderLabels(["액션", "종류", "바인딩 (JSON 배열)"])
        self.table.horizontalHeader().setSectionResizeMode(_COL_BINDINGS, QHeaderView.Stretch)
        self.table.verticalHeader().setVisible(False)
        self.table.itemChanged.connect(self._on_item_changed)
        layout.addWidget(self.table)

        self.lbl_status = QLabel("")
        self.lbl_status.setWordWrap(True)
        self.lbl_status.setStyleSheet(f"color: {COLORS['text_dim']}; font-size: 10px;")
        layout.addWidget(self.lbl_status)
        self.lbl_status.setToolTip(
            'button: ["Key:Space", "Mouse:Right"]\n'
            'axis: [{"negative": "Key:A", "positive": "Key:D"}, {"source": "MouseDeltaX", "scale": 0.1}]')

    def _set_enabled(self, enabled: bool):
        for w in (self.btn_add, self.btn_remove, self.btn_save, self.btn_reload, self.table):
            w.setEnabled(enabled)
        if not enabled:
            self._status("엔진이 연결되지 않아 입력 액션을 편집할 수 없습니다(읽기·쓰기는 엔진이 한다)")

    def _status(self, text: str, error: bool = False):
        self.lbl_status.setText(text)
        self.lbl_status.setStyleSheet(
            f"color: {COLORS['accent_danger'] if error else COLORS['text_dim']}; font-size: 10px;")

    # ── 연결 ─────────────────────────────────────────────────────────────────

    def connect_engine(self, engine, path_provider: Callable[[], str]):
        """engine: quarterflying.Engine. path_provider: 현재 프로젝트의 actions.json 경로를 돌려준다."""
        self._engine = engine
        self._path_provider = path_provider
        self._set_enabled(engine is not None)
        self.reload_from_engine()

    @property
    def is_dirty(self) -> bool:
        return self._dirty

    def reload_from_engine(self):
        """엔진의 현재 맵으로 표를 다시 채운다(프로젝트를 열거나 파일에서 다시 읽은 뒤)."""
        if self._engine is None:
            return
        actions = json.loads(self._engine.GetInputActionsJson()).get("actions", {})
        self._filling = True
        try:
            self.table.setRowCount(0)
            for name in sorted(actions):
                self._append_row(name, actions[name].get("type", ""), actions[name].get("bindings", []))
        finally:
            self._filling = False
        self._dirty = False
        self._status(f"액션 {len(actions)}개 — 바인딩 칸을 고치면 바로 적용됩니다. 파일에 남기려면 '저장'")

    def _append_row(self, name: str, kind: str, bindings):
        row = self.table.rowCount()
        self.table.insertRow(row)
        for col, text, editable in ((_COL_NAME, name, False), (_COL_TYPE, kind, False),
                                    (_COL_BINDINGS, json.dumps(bindings, ensure_ascii=False), True)):
            item = QTableWidgetItem(text)
            if not editable:
                item.setFlags(item.flags() & ~Qt.ItemIsEditable)
            self.table.setItem(row, col, item)

    # ── 편집 ─────────────────────────────────────────────────────────────────

    def set_action(self, name: str, kind: str, bindings_text: str) -> list:
        """한 액션을 엔진에 보낸다. 엔진의 경고 목록을 돌려주고, 거절되면 RuntimeError(엔진 메시지)를 낸다."""
        # 정의 JSON을 문자열로만 이어 붙인다 - 바인딩 텍스트의 해석(문법 포함)은 엔진이 한다.
        def_json = '{"type": %s, "bindings": %s}' % (json.dumps(kind), bindings_text.strip() or "[]")
        warnings = list(self._engine.SetInputActionJson(name, def_json))
        self._dirty = True
        return warnings

    def _on_item_changed(self, item: QTableWidgetItem):
        if self._filling or self._engine is None or item.column() != _COL_BINDINGS:
            return
        row = item.row()
        name = self.table.item(row, _COL_NAME).text()
        kind = self.table.item(row, _COL_TYPE).text()
        try:
            warnings = self.set_action(name, kind, item.text())
        except RuntimeError as e:
            # 거절됐으면 엔진의 현재 값으로 되돌린 뒤 이유를 보인다. 순서가 반대면 reload_from_engine의 안내 문구가
            # 오류를 덮어써서, 칸만 말없이 되돌아가는 것처럼 보였다(G2 실제 실행에서 발견).
            dirty = self._dirty
            self.reload_from_engine()
            self._dirty = dirty
            self._status(f"'{name}' 적용 안 됨(이전 값 유지): {e}", error=True)
            return
        if warnings:
            self._status(f"'{name}' 적용(경고 {len(warnings)}개 — 해당 바인딩만 건너뜀): " + "; ".join(warnings), error=True)
        else:
            self._status(f"'{name}' 적용됨 (저장 안 됨)")
        # 엔진이 정규화한 값(건너뛴 바인딩 제외)으로 표를 맞춘다
        self._refresh_row_from_engine(row, name)

    def _refresh_row_from_engine(self, row: int, name: str):
        actions = json.loads(self._engine.GetInputActionsJson()).get("actions", {})
        if name in actions:
            self._filling = True
            try:
                self.table.item(row, _COL_BINDINGS).setText(json.dumps(actions[name]["bindings"], ensure_ascii=False))
            finally:
                self._filling = False

    def _on_add(self):
        name, ok = QInputDialog.getText(self, "새 액션", "액션 이름:")
        if not ok or not name.strip():
            return
        kind, ok = QInputDialog.getItem(self, "새 액션", "종류:", ["button", "axis"], 0, False)
        if not ok:
            return
        self.add_action(name.strip(), kind)

    def add_action(self, name: str, kind: str):
        try:
            self.set_action(name, kind, "[]")
        except RuntimeError as e:
            self._status(f"액션을 만들 수 없습니다: {e}", error=True)
            return
        dirty = self._dirty
        self.reload_from_engine()
        self._dirty = dirty
        self._status(f"'{name}' 추가됨 — 바인딩 칸에 키를 적으세요 (저장 안 됨)")

    def _on_remove(self):
        row = self.table.currentRow()
        if row < 0:
            return
        self.remove_action(self.table.item(row, _COL_NAME).text())

    def remove_action(self, name: str):
        if self._engine.RemoveInputAction(name):
            dirty = True
            self.reload_from_engine()
            self._dirty = dirty
            self._status(f"'{name}' 삭제됨 (저장 안 됨)")

    def save(self) -> bool:
        if self._engine is None or self._path_provider is None:
            return False
        path = self._path_provider()
        try:
            self._engine.SaveInputActions(path)
        except RuntimeError as e:
            self._status(f"저장 실패: {e}", error=True)
            return False
        self._dirty = False
        self._status(f"저장됨: {path}")
        self.status_message.emit("입력 액션 맵 저장됨")
        return True

    def _on_reload(self):
        # 프로젝트 로더(project_actions._load_input_actions)를 거쳐야 경고 표시 규칙이 같다 - 그쪽이 끝나면 이 패널을 다시 채운다.
        self.status_message.emit("입력 액션 맵을 파일에서 다시 읽습니다")
        self.reload_requested.emit()

    reload_requested = Signal()
