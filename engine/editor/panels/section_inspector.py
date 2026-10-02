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

import os
from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QScrollArea, QFrame, QPushButton, QLineEdit,
    QDoubleSpinBox, QSpinBox, QComboBox, QSizePolicy,
    QGroupBox, QSlider, QFileDialog
)
from PySide6.QtCore import Qt, Signal

from core.action_data import (
    ActionData, ActionSection, ActionEvent, AnimationLayer,
    EventType, SectionRole, SECTION_ROLE_COLORS
)
from style.theme import COLORS


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
        self.speed_pct_label.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")

        row3.addWidget(lbl_speed)
        row3.addWidget(self.spin_speed, 1)
        row3.addWidget(self.speed_pct_label)
        layout.addLayout(row3)

        # 구분선
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet(f"background-color: {COLORS['border']}; max-height: 1px;")
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
        line.setStyleSheet(f"background-color: {COLORS['border']}; max-height: 1px;")
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

class SoundEventRowWidget(QWidget):
    """Sound 이벤트 하나를 편집하는 행 위젯 (Sound Lite Phase 4).

    docs/SOUND_LITE_PLAN.md §2의 3개 값(Clip / Volume / Volume ±)만 노출한다.
    Pitch가 없는 이유는 §2의 개정 참고 - QSoundEffect에 음정 속성이 없다.

    값은 ActionEvent.params(자유형 dict)에 그대로 들어간다 - 자료구조도 파일 포맷도
    바꾸지 않는다(§4.1).
    """

    changed = Signal()
    delete_requested = Signal(object)

    def __init__(self, event: ActionEvent, total_frames: int, parent=None):
        super().__init__(parent)
        self._event = event
        self._total_frames = total_frames
        self._building = True
        self._build_ui()
        self._building = False

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(3)

        # 행 1: 프레임 + 클립 경로 + 찾아보기 + 삭제
        row1 = QHBoxLayout()
        row1.setSpacing(4)

        lbl_f = QLabel("프레임")
        lbl_f.setFixedWidth(42)
        self.spin_frame = QSpinBox()
        self.spin_frame.setRange(0, max(0, self._total_frames))
        self.spin_frame.setValue(int(self._event.frame))
        self.spin_frame.setFixedWidth(56)
        self.spin_frame.setButtonSymbols(QSpinBox.NoButtons)
        self.spin_frame.valueChanged.connect(self._on_changed)

        self.clip_edit = QLineEdit(self._params().get("clip", ""))
        self.clip_edit.setPlaceholderText("클립 파일 (.wav)")
        self.clip_edit.editingFinished.connect(self._on_changed)

        btn_browse = QPushButton("…")
        btn_browse.setFixedSize(24, 22)
        btn_browse.setToolTip("WAV 파일 선택")
        btn_browse.clicked.connect(self._on_browse)

        btn_del = QPushButton("✕")
        btn_del.setObjectName("btn_delete")
        btn_del.setFixedSize(22, 22)
        btn_del.clicked.connect(lambda: self.delete_requested.emit(self))

        row1.addWidget(lbl_f)
        row1.addWidget(self.spin_frame)
        row1.addWidget(self.clip_edit, 1)
        row1.addWidget(btn_browse)
        row1.addWidget(btn_del)
        layout.addLayout(row1)

        # 행 2: 볼륨 + 볼륨 변동폭
        row2 = QHBoxLayout()
        row2.setSpacing(4)

        lbl_v = QLabel("볼륨")
        lbl_v.setFixedWidth(42)
        self.spin_volume = QDoubleSpinBox()
        self.spin_volume.setRange(0.0, 1.0)
        self.spin_volume.setSingleStep(0.05)
        self.spin_volume.setDecimals(2)
        self.spin_volume.setValue(float(self._params().get("volume", 1.0)))
        self.spin_volume.setButtonSymbols(QDoubleSpinBox.NoButtons)
        self.spin_volume.valueChanged.connect(self._on_changed)

        lbl_var = QLabel("±")
        lbl_var.setFixedWidth(12)
        self.spin_var = QDoubleSpinBox()
        self.spin_var.setRange(0.0, 1.0)
        self.spin_var.setSingleStep(0.05)
        self.spin_var.setDecimals(2)
        self.spin_var.setValue(float(self._params().get("volumeVar", 0.0)))
        self.spin_var.setButtonSymbols(QDoubleSpinBox.NoButtons)
        self.spin_var.setToolTip("반복 재생이 기계적으로 들리지 않게 하는 음량 변동폭")
        self.spin_var.valueChanged.connect(self._on_changed)

        row2.addWidget(lbl_v)
        row2.addWidget(self.spin_volume, 1)
        row2.addWidget(lbl_var)
        row2.addWidget(self.spin_var, 1)
        layout.addLayout(row2)

    def _params(self) -> dict:
        if self._event.params is None:
            self._event.params = {}
        return self._event.params

    def _on_browse(self):
        # §6.3 - v1은 WAV만 지원한다. 필터로 그 제약을 드러낸다.
        path, _ = QFileDialog.getOpenFileName(self, "사운드 클립 선택", "", "Sound Files (*.wav)")
        if not path:
            return
        # 고른 시점에는 이 액션이 어느 폴더에 저장될지 모른다. 절대 경로로 들고 있다가
        # 저장 시점에 상대 경로로 계산한다(§6.4, save_action의 relativize_clip_paths).
        self.clip_edit.setText(os.path.abspath(path))
        self._on_changed()

    def _on_changed(self):
        if self._building:
            return
        self._event.frame = int(self.spin_frame.value())
        p = self._params()
        p["clip"] = self.clip_edit.text().strip()
        p["volume"] = float(self.spin_volume.value())
        p["volumeVar"] = float(self.spin_var.value())
        self.changed.emit()


class CameraEventRowWidget(QWidget):
    """Camera 이벤트 하나를 편집하는 행 위젯 (카메라 계획 C4).

    params = {"camera": 카메라 엔티티 이름}. Scene Play에서 그 카메라가 활성 카메라가 된다
    (ScenePlaybackController._switch_cameras). 전환 연출(블렌드 시간)은 카메라 쪽 값이다.
    """

    changed = Signal()
    delete_requested = Signal(object)

    def __init__(self, event: ActionEvent, total_frames: int, parent=None):
        super().__init__(parent)
        self._event = event
        self._building = True
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 4, 8, 4)
        layout.setSpacing(4)

        lbl_f = QLabel("프레임")
        lbl_f.setFixedWidth(42)
        self.spin_frame = QSpinBox()
        self.spin_frame.setRange(0, max(0, total_frames))
        self.spin_frame.setValue(int(event.frame))
        self.spin_frame.setFixedWidth(56)
        self.spin_frame.setButtonSymbols(QSpinBox.NoButtons)
        self.spin_frame.valueChanged.connect(self._on_changed)

        self.camera_edit = QLineEdit((event.params or {}).get("camera", ""))
        self.camera_edit.setPlaceholderText("카메라 엔티티 이름 (예: Cam B)")
        self.camera_edit.editingFinished.connect(self._on_changed)

        btn_del = QPushButton("\u2715")
        btn_del.setObjectName("btn_delete")
        btn_del.setFixedSize(22, 22)
        btn_del.clicked.connect(lambda: self.delete_requested.emit(self))

        layout.addWidget(lbl_f)
        layout.addWidget(self.spin_frame)
        layout.addWidget(self.camera_edit, 1)
        layout.addWidget(btn_del)
        self._building = False

    def _on_changed(self):
        if self._building:
            return
        self._event.frame = int(self.spin_frame.value())
        if self._event.params is None:
            self._event.params = {}
        self._event.params["camera"] = self.camera_edit.text().strip()
        self.changed.emit()


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
        self._sound_widgets: list[SoundEventRowWidget] = []
        self._camera_widgets: list[CameraEventRowWidget] = []
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

        # 구분선
        sep2 = QFrame()
        sep2.setFrameShape(QFrame.HLine)
        sep2.setStyleSheet("background-color: #1e3a5f; max-height: 1px; margin: 8px 0;")
        self.scroll_layout.addWidget(sep2)

        # ── 사운드 이벤트 블록 (Sound Lite Phase 4) ──
        # 이벤트 타임라인에는 이미 Sound 이벤트를 찍는 자리가 있지만(EventType.SOUND)
        # 파라미터를 편집할 곳이 없었다. 새 패널을 만들지 않고 섹션/레이어와 같은
        # 행 위젯 목록 패턴을 그대로 쓴다.
        self.lbl_sounds = self._make_section_label("SOUND EVENTS")
        self.scroll_layout.addWidget(self.lbl_sounds)

        self.sounds_container = QWidget()
        self.sounds_layout = QVBoxLayout(self.sounds_container)
        self.sounds_layout.setContentsMargins(0, 0, 0, 0)
        self.sounds_layout.setSpacing(0)
        self.scroll_layout.addWidget(self.sounds_container)

        btn_add_sound = QPushButton("＋ Add Sound Event")
        btn_add_sound.setObjectName("btn_create")
        btn_add_sound.clicked.connect(self._on_add_sound)
        self.scroll_layout.addWidget(btn_add_sound)

        # ── 카메라 이벤트 블록 (카메라 계획 C4) ──
        self.lbl_cameras = self._make_section_label("CAMERA EVENTS")
        self.scroll_layout.addWidget(self.lbl_cameras)
        self.cameras_container = QWidget()
        self.cameras_layout = QVBoxLayout(self.cameras_container)
        self.cameras_layout.setContentsMargins(0, 0, 0, 0)
        self.cameras_layout.setSpacing(0)
        self.scroll_layout.addWidget(self.cameras_container)
        btn_add_camera = QPushButton("\uff0b Add Camera Event")
        btn_add_camera.setObjectName("btn_create")
        btn_add_camera.clicked.connect(self._on_add_camera_event)
        self.scroll_layout.addWidget(btn_add_camera)

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

        # Sound 이벤트만 골라 행으로 만든다. 다른 EventType(Hit/Effect/CameraShake)의
        # 파라미터 편집은 이번 범위가 아니다(범위 정의서 §3).
        for event in self._sound_events():
            w = SoundEventRowWidget(event, self._action.total_frames)
            w.changed.connect(self._on_data_changed)
            w.delete_requested.connect(self._on_delete_sound)
            self.sounds_layout.addWidget(w)
            self._sound_widgets.append(w)

        for event in self._camera_events():
            w = CameraEventRowWidget(event, self._action.total_frames)
            w.changed.connect(self._on_data_changed)
            w.delete_requested.connect(self._on_delete_camera_event)
            self.cameras_layout.addWidget(w)
            self._camera_widgets.append(w)

    def _sound_events(self) -> list:
        if self._action is None:
            return []
        return [e for e in self._action.events if e.type == EventType.SOUND]

    def _camera_events(self) -> list:
        if self._action is None:
            return []
        return [e for e in self._action.events if e.type == EventType.CAMERA]

    def _clear_widgets(self):
        for w in self._section_widgets:
            w.deleteLater()
        self._section_widgets.clear()

        for w in self._layer_widgets:
            w.deleteLater()
        self._layer_widgets.clear()

        for w in self._sound_widgets:
            w.deleteLater()
        self._sound_widgets.clear()

        for w in self._camera_widgets:
            w.deleteLater()
        self._camera_widgets.clear()

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

    def _on_add_sound(self):
        if self._action is None:
            return
        self._action.events.append(
            ActionEvent(frame=0, type=EventType.SOUND,
                        params={"clip": "", "volume": 1.0, "volumeVar": 0.0})
        )
        self._refresh()
        self.data_changed.emit(self._action)

    def _on_delete_sound(self, row_widget: SoundEventRowWidget):
        if self._action is None:
            return
        idx = self._sound_widgets.index(row_widget)
        sounds = self._sound_events()
        if 0 <= idx < len(sounds):
            # events 리스트에는 다른 타입도 섞여 있으므로 인덱스가 아니라 객체로 지운다.
            self._action.events.remove(sounds[idx])
            self._refresh()
            self.data_changed.emit(self._action)

    def _on_add_camera_event(self):
        if self._action is None:
            return
        self._action.events.append(ActionEvent(frame=0, type=EventType.CAMERA, params={"camera": ""}))
        self._refresh()
        self.data_changed.emit(self._action)

    def _on_delete_camera_event(self, row_widget: CameraEventRowWidget):
        if self._action is None:
            return
        idx = self._camera_widgets.index(row_widget)
        cams = self._camera_events()
        if 0 <= idx < len(cams):
            self._action.events.remove(cams[idx])   # 다른 타입과 섞여 있으므로 객체로 지운다
            self._refresh()
            self.data_changed.emit(self._action)
