"""
editor/panels/section_inspector.py

Section Inspector — 섹션 / Animation Layer 편집 패널.

역할:
  - 선택된 ActionData의 섹션 목록 표시 및 편집
  - 각 섹션: 이름(자유), Role(SectionRole), 시작/끝 프레임, 속도(Retiming)
  - Animation Layer 믹서: 클립 이름, 가중치, 속도
  - data_changed 시그널로 다른 패널에 변경 통보
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QScrollArea, QFrame, QPushButton, QLineEdit,
    QDoubleSpinBox, QSpinBox, QComboBox, QSizePolicy,
    QGroupBox, QSlider
)
from PySide6.QtCore import Qt, Signal

from core.action_data import (
    ActionData, ActionSection, AnimationLayer,
    SectionRole, SECTION_ROLE_COLORS
)


# ─────────────────────────────────────────────────────────────────────────────
# 섹션 행 위젯
# ─────────────────────────────────────────────────────────────────────────────

class SectionRowWidget(QWidget):
    """섹션 하나를 편집하는 행 위젯."""

    changed = Signal()
    delete_requested = Signal(object)  # self

    def __init__(self, section: ActionSection, parent=None):
        super().__init__(parent)
        self._section = section
        self._building = True
        self._build_ui()
        self._building = False

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 6, 8, 6)
        layout.setSpacing(4)

        # 색상 바
        role_color = SECTION_ROLE_COLORS.get(self._section.role, "#444")
        self.setStyleSheet(f"border-left: 3px solid {role_color};")

        # 행 1: 이름 + Role + 삭제
        row1 = QHBoxLayout()
        row1.setSpacing(4)

        self.name_edit = QLineEdit(self._section.name)
        self.name_edit.setPlaceholderText("이름")
        self.name_edit.setFixedWidth(90)
        self.name_edit.editingFinished.connect(self._on_changed)

        self.role_combo = QComboBox()
        for role in SectionRole:
            self.role_combo.addItem(role.value, role)
        self.role_combo.setCurrentIndex(
            list(SectionRole).index(self._section.role)
        )
        self.role_combo.currentIndexChanged.connect(self._on_role_changed)

        btn_del = QPushButton("✕")
        btn_del.setObjectName("btn_delete")
        btn_del.setFixedSize(22, 22)
        btn_del.setToolTip("섹션 삭제")
        btn_del.clicked.connect(lambda: self.delete_requested.emit(self))

        row1.addWidget(self.name_edit)
        row1.addWidget(self.role_combo, 1)
        row1.addWidget(btn_del)
        layout.addLayout(row1)

        # 행 2: 프레임 범위
        row2 = QHBoxLayout()
        row2.setSpacing(4)

        lbl_start = QLabel("시작")
        lbl_start.setFixedWidth(28)
        self.spin_start = QSpinBox()
        self.spin_start.setRange(0, 99999)
        self.spin_start.setValue(self._section.start_frame)
        self.spin_start.setButtonSymbols(QSpinBox.NoButtons)
        self.spin_start.valueChanged.connect(self._on_changed)

        lbl_end = QLabel("끝")
        lbl_end.setFixedWidth(20)
        self.spin_end = QSpinBox()
        self.spin_end.setRange(0, 99999)
        self.spin_end.setValue(self._section.end_frame)
        self.spin_end.setButtonSymbols(QSpinBox.NoButtons)
        self.spin_end.valueChanged.connect(self._on_changed)

        row2.addWidget(lbl_start)
        row2.addWidget(self.spin_start, 1)
        row2.addWidget(lbl_end)
        row2.addWidget(self.spin_end, 1)
        layout.addLayout(row2)

        # 행 3: 속도 (Retiming)
        row3 = QHBoxLayout()
        row3.setSpacing(4)

        lbl_speed = QLabel("속도")
        lbl_speed.setFixedWidth(28)
        self.spin_speed = QDoubleSpinBox()
        self.spin_speed.setRange(0.1, 5.0)
        self.spin_speed.setSingleStep(0.1)
        self.spin_speed.setDecimals(2)
        self.spin_speed.setValue(self._section.speed)
        self.spin_speed.setButtonSymbols(QDoubleSpinBox.NoButtons)
        self.spin_speed.setSuffix("×")
        self.spin_speed.valueChanged.connect(self._on_changed)

        self.speed_pct_label = QLabel(f"{int(self._section.speed * 100)}%")
        self.speed_pct_label.setFixedWidth(42)
        self.speed_pct_label.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self.speed_pct_label.setStyleSheet("color: #8899aa; font-size: 11px;")

        row3.addWidget(lbl_speed)
        row3.addWidget(self.spin_speed, 1)
        row3.addWidget(self.speed_pct_label)
        layout.addLayout(row3)

        # 구분선
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet("background-color: #1e3a5f; max-height: 1px;")
        layout.addWidget(line)

    def _on_role_changed(self):
        if self._building:
            return
        new_role = self.role_combo.currentData()
        self._section.role = new_role
        color = SECTION_ROLE_COLORS.get(new_role, "#444")
        self.setStyleSheet(f"border-left: 3px solid {color};")
        self.changed.emit()

    def _on_changed(self):
        if self._building:
            return
        self._section.name        = self.name_edit.text().strip() or self._section.name
        self._section.start_frame = self.spin_start.value()
        self._section.end_frame   = self.spin_end.value()
        self._section.speed       = self.spin_speed.value()
        self.speed_pct_label.setText(f"{int(self._section.speed * 100)}%")
        self.changed.emit()


# ─────────────────────────────────────────────────────────────────────────────
# 레이어 행 위젯
# ─────────────────────────────────────────────────────────────────────────────

class LayerRowWidget(QWidget):
    """AnimationLayer 하나를 편집하는 행 위젯."""

    changed = Signal()
    delete_requested = Signal(object)

    def __init__(self, layer: AnimationLayer, parent=None):
        super().__init__(parent)
        self._layer = layer
        self._building = True
        self._build_ui()
        self._building = False

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(3)

        # 행 1: 이름 + 클립 이름 + 삭제
        row1 = QHBoxLayout()
        row1.setSpacing(4)

        self.name_edit = QLineEdit(self._layer.name)
        self.name_edit.setFixedWidth(70)
        self.name_edit.setPlaceholderText("레이어")
        self.name_edit.editingFinished.connect(self._on_changed)

        self.clip_edit = QLineEdit(self._layer.clip_name)
        self.clip_edit.setPlaceholderText("클립 이름")
        self.clip_edit.editingFinished.connect(self._on_changed)

        btn_del = QPushButton("✕")
        btn_del.setObjectName("btn_delete")
        btn_del.setFixedSize(22, 22)
        btn_del.clicked.connect(lambda: self.delete_requested.emit(self))

        row1.addWidget(self.name_edit)
        row1.addWidget(self.clip_edit, 1)
        row1.addWidget(btn_del)
        layout.addLayout(row1)

        # 행 2: 가중치 + 속도
        row2 = QHBoxLayout()
        row2.setSpacing(4)

        lbl_w = QLabel("가중치")
        lbl_w.setFixedWidth(42)
        self.spin_weight = QDoubleSpinBox()
        self.spin_weight.setRange(0.0, 1.0)
        self.spin_weight.setSingleStep(0.05)
        self.spin_weight.setDecimals(2)
        self.spin_weight.setValue(self._layer.weight)
        self.spin_weight.setButtonSymbols(QDoubleSpinBox.NoButtons)
        self.spin_weight.valueChanged.connect(self._on_changed)

        lbl_s = QLabel("속도")
        lbl_s.setFixedWidth(28)
        self.spin_speed = QDoubleSpinBox()
        self.spin_speed.setRange(0.1, 5.0)
        self.spin_speed.setSingleStep(0.1)
        self.spin_speed.setDecimals(2)
        self.spin_speed.setValue(self._layer.speed)
        self.spin_speed.setButtonSymbols(QDoubleSpinBox.NoButtons)
        self.spin_speed.setSuffix("×")
        self.spin_speed.valueChanged.connect(self._on_changed)

        row2.addWidget(lbl_w)
        row2.addWidget(self.spin_weight, 1)
        row2.addWidget(lbl_s)
        row2.addWidget(self.spin_speed, 1)
        layout.addLayout(row2)

        # 구분선
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet("background-color: #1e3a5f; max-height: 1px;")
        layout.addWidget(line)

    def _on_changed(self):
        if self._building:
            return
        self._layer.name      = self.name_edit.text().strip() or self._layer.name
        self._layer.clip_name = self.clip_edit.text().strip()
        self._layer.weight    = self.spin_weight.value()
        self._layer.speed     = self.spin_speed.value()
        self.changed.emit()


# ─────────────────────────────────────────────────────────────────────────────
# 메인 패널
# ─────────────────────────────────────────────────────────────────────────────

class SectionInspectorPanel(QWidget):
    """
    오른쪽 패널: 섹션 목록 + Animation Layer 믹서.

    Signals:
        data_changed(ActionData) — 섹션 또는 레이어 변경 시
    """

    data_changed = Signal(object)  # ActionData

    def __init__(self, parent=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._section_widgets: list[SectionRowWidget] = []
        self._layer_widgets: list[LayerRowWidget] = []
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        header = QLabel("  SECTION INSPECTOR")
        header.setFixedHeight(28)
        header.setObjectName("panel_header")
        layout.addWidget(header)

        # 액션 이름 표시
        self.lbl_action = QLabel("선택된 액션 없음")
        self.lbl_action.setFixedHeight(28)
        self.lbl_action.setStyleSheet(
            "color: #8899aa; font-size: 11px; padding: 0 10px;"
        )
        layout.addWidget(self.lbl_action)

        # 스크롤 영역
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)

        self.scroll_content = QWidget()
        self.scroll_layout = QVBoxLayout(self.scroll_content)
        self.scroll_layout.setContentsMargins(0, 0, 0, 0)
        self.scroll_layout.setSpacing(0)

        # ── 섹션 블록 ──
        self.lbl_sections = self._make_section_label("SECTIONS")
        self.scroll_layout.addWidget(self.lbl_sections)

        self.sections_container = QWidget()
        self.sections_layout = QVBoxLayout(self.sections_container)
        self.sections_layout.setContentsMargins(0, 0, 0, 0)
        self.sections_layout.setSpacing(0)
        self.scroll_layout.addWidget(self.sections_container)

        btn_add_section = QPushButton("＋ Add Section")
        btn_add_section.setObjectName("btn_create")
        btn_add_section.clicked.connect(self._on_add_section)
        self.scroll_layout.addWidget(btn_add_section)

        # 구분선
        sep = QFrame()
        sep.setFrameShape(QFrame.HLine)
        sep.setStyleSheet("background-color: #1e3a5f; max-height: 1px; margin: 8px 0;")
        self.scroll_layout.addWidget(sep)

        # ── 레이어 블록 ──
        self.lbl_layers = self._make_section_label("ANIMATION LAYERS")
        self.scroll_layout.addWidget(self.lbl_layers)

        self.layers_container = QWidget()
        self.layers_layout = QVBoxLayout(self.layers_container)
        self.layers_layout.setContentsMargins(0, 0, 0, 0)
        self.layers_layout.setSpacing(0)
        self.scroll_layout.addWidget(self.layers_container)

        btn_add_layer = QPushButton("＋ Add Layer")
        btn_add_layer.setObjectName("btn_create")
        btn_add_layer.clicked.connect(self._on_add_layer)
        self.scroll_layout.addWidget(btn_add_layer)

        self.scroll_layout.addStretch()
        scroll.setWidget(self.scroll_content)
        layout.addWidget(scroll)

    @staticmethod
    def _make_section_label(text: str) -> QLabel:
        lbl = QLabel(f"  {text}")
        lbl.setFixedHeight(24)
        lbl.setStyleSheet(
            "background-color: #0d1b2a; color: #00d4ff; "
            "font-size: 10px; font-weight: bold; letter-spacing: 1px;"
        )
        return lbl

    # ── 공개 API ─────────────────────────────────────────────────────────────

    def set_action(self, action: Optional[ActionData]) -> None:
        self._action = action
        self._refresh()

    def apply_ai_sections(self, sections) -> None:
        """AI Analyze 결과로 섹션을 교체."""
        if self._action is None:
            return
        self._action.sections = sections
        self._refresh()
        self.data_changed.emit(self._action)

    # ── 내부 ─────────────────────────────────────────────────────────────────

    def _refresh(self):
        self._clear_widgets()

        if self._action is None:
            self.lbl_action.setText("선택된 액션 없음")
            return

        self.lbl_action.setText(
            f"  {self._action.name}  |  {self._action.total_frames}f @ {self._action.fps}fps"
        )

        for section in self._action.sections:
            w = SectionRowWidget(section)
            w.changed.connect(self._on_data_changed)
            w.delete_requested.connect(self._on_delete_section)
            self.sections_layout.addWidget(w)
            self._section_widgets.append(w)

        for layer in self._action.layers:
            w = LayerRowWidget(layer)
            w.changed.connect(self._on_data_changed)
            w.delete_requested.connect(self._on_delete_layer)
            self.layers_layout.addWidget(w)
            self._layer_widgets.append(w)

    def _clear_widgets(self):
        for w in self._section_widgets:
            w.deleteLater()
        self._section_widgets.clear()

        for w in self._layer_widgets:
            w.deleteLater()
        self._layer_widgets.clear()

    def _on_data_changed(self):
        if self._action:
            self.data_changed.emit(self._action)

    def _on_add_section(self):
        if self._action is None:
            return
        total = self._action.total_frames
        new_section = ActionSection(
            name        = f"Section{len(self._action.sections) + 1}",
            role        = SectionRole.ATTACK,
            start_frame = 0,
            end_frame   = total,
            speed       = 1.0,
        )
        self._action.sections.append(new_section)
        self._refresh()
        self.data_changed.emit(self._action)

    def _on_delete_section(self, row_widget: SectionRowWidget):
        if self._action is None:
            return
        idx = self._section_widgets.index(row_widget)
        if 0 <= idx < len(self._action.sections):
            self._action.sections.pop(idx)
            self._refresh()
            self.data_changed.emit(self._action)

    def _on_add_layer(self):
        if self._action is None:
            return
        new_layer = AnimationLayer(
            name      = f"Layer{len(self._action.layers) + 1}",
            clip_name = "",
        )
        self._action.layers.append(new_layer)
        self._refresh()
        self.data_changed.emit(self._action)

    def _on_delete_layer(self, row_widget: LayerRowWidget):
        if self._action is None:
            return
        idx = self._layer_widgets.index(row_widget)
        if 0 <= idx < len(self._action.layers):
            self._action.layers.pop(idx)
            self._refresh()
            self.data_changed.emit(self._action)
