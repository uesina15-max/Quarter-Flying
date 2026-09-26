"""
editor/panels/prefab_browser.py

Prefab Browser Panel — Phase 3 (docs/PREFAB_IMPLEMENTATION_PLAN.md §3).

역할:
  - engine/assets/prefabs/ 의 *.prefab.json을 스캔해 목록에 채운다
    (motion_mixer.py가 assets/motion/*.skeleton.json을 os.listdir + 확장자 필터로
    스캔하는 것과 같은 관례 - engine/asset/ 모듈은 쓰지 않는다,
    docs/PREFAB_IMPLEMENTATION_PLAN.md §1 참고).
  - 선택한 프리팹을 "Instantiate"하면 EditorAPI::InstantiatePrefab(Phase 2)을
    position 인자 없이 호출해 프리팹 자체 위치 그대로 새 엔티티를 스폰한다(§2.8).

의도적 범위 제한: MASTER_PLAN.md §2의 P2 "에셋 브라우저"(범용, 아직 없음)와는 다른
프리팹 전용 최소 구현이다 - 범용 에셋 브라우저가 생기면 그쪽으로 흡수하면 된다.
"""

from __future__ import annotations

import json
import os

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QListWidget, QListWidgetItem,
    QPushButton
)
from PySide6.QtCore import Qt, Signal

from style.theme import COLORS

# engine/editor/panels/prefab_browser.py 기준 engine/assets/prefabs/
_PREFAB_ASSETS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "assets", "prefabs",
)


def _read_prefab_display_name(path: str) -> str:
    """목록 표시용 이름 - 파일 내부의 "name" 필드가 있으면 그걸, 없으면 파일명."""
    try:
        with open(path, "r", encoding="utf-8") as f:
            data = json.load(f)
        name = data.get("name")
        if name:
            return str(name)
    except (OSError, ValueError):
        pass
    return os.path.basename(path)


class PrefabBrowserPanel(QWidget):
    """
    프리팹 목록 + 인스턴스화 패널.

    Signals:
        status_message(str): 상태 바로 전달할 메시지 텍스트(성공/실패 모두 포함)
    """

    status_message = Signal(str)

    def __init__(self, parent=None):
        super().__init__(parent)
        self.editor_api = None
        self._build_ui()
        self._connect_signals()
        self.refresh()

    # ----------------------------------------------------------------
    # UI 구성
    # ----------------------------------------------------------------

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QLabel("  PREFABS")
        header.setObjectName("panel_header")
        header.setFixedHeight(28)
        layout.addWidget(header)

        toolbar = QWidget()
        toolbar.setFixedHeight(32)
        toolbar.setStyleSheet(
            f"background-color: {COLORS['bg_header']}; border-bottom: 1px solid {COLORS['border']};"
        )
        h = QHBoxLayout(toolbar)
        h.setContentsMargins(6, 2, 6, 2)
        h.setSpacing(4)

        self.btn_refresh = QPushButton("↻ Refresh")
        self.btn_refresh.setFixedHeight(22)
        self.btn_refresh.setToolTip("assets/prefabs/ 다시 스캔")

        self.btn_instantiate = QPushButton("Instantiate")
        self.btn_instantiate.setFixedHeight(22)
        self.btn_instantiate.setEnabled(False)
        self.btn_instantiate.setToolTip("선택한 프리팹을 새 엔티티로 스폰")

        h.addWidget(self.btn_refresh)
        h.addStretch()
        h.addWidget(self.btn_instantiate)
        layout.addWidget(toolbar)

        self.list_widget = QListWidget()
        self.list_widget.setAlternatingRowColors(True)
        layout.addWidget(self.list_widget)

    def _connect_signals(self):
        self.btn_refresh.clicked.connect(self.refresh)
        self.btn_instantiate.clicked.connect(self._on_instantiate_clicked)
        self.list_widget.itemSelectionChanged.connect(self._on_selection_changed)
        self.list_widget.itemDoubleClicked.connect(lambda _item: self._on_instantiate_clicked())

    # ----------------------------------------------------------------
    # 엔진 연동
    # ----------------------------------------------------------------

    def set_editor_api(self, editor_api):
        self.editor_api = editor_api
        self._on_selection_changed()

    def refresh(self):
        """assets/prefabs/*.prefab.json 재스캔."""
        self.list_widget.clear()
        if not os.path.isdir(_PREFAB_ASSETS_DIR):
            return
        for filename in sorted(f for f in os.listdir(_PREFAB_ASSETS_DIR) if f.endswith(".prefab.json")):
            path = os.path.join(_PREFAB_ASSETS_DIR, filename)
            item = QListWidgetItem(f"📦  {_read_prefab_display_name(path)}")
            item.setData(Qt.UserRole, path)
            item.setToolTip(path)
            self.list_widget.addItem(item)
        self._on_selection_changed()

    # ----------------------------------------------------------------
    # 이벤트 핸들러
    # ----------------------------------------------------------------

    def _on_selection_changed(self):
        has_selection = self.list_widget.currentItem() is not None
        self.btn_instantiate.setEnabled(self.editor_api is not None and has_selection)

    def _on_instantiate_clicked(self):
        item = self.list_widget.currentItem()
        if not item or self.editor_api is None:
            return

        path = item.data(Qt.UserRole)
        label = item.text().strip()
        try:
            # position 인자를 안 줘서 프리팹 자체 캡처 위치 그대로 스폰한다(§2.8).
            entity = self.editor_api.instantiate_prefab(path)
            self.status_message.emit(f"프리팹 인스턴스화 완료: {label} (entity id={entity.id})")
        except Exception as e:
            print(f"[PrefabBrowser] 프리팹 인스턴스화 실패: {e}")
            self.status_message.emit(f"프리팹 인스턴스화 실패: {e}")
