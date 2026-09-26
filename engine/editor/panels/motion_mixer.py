"""
editor/panels/motion_mixer.py

Motion Mixer Panel — Phase 5 믹서 UI (착수 계약서 §C, ROADMAP.md P1.5 참고).

역할:
  - engine/assets/motion/ 의 *.skeleton.json / *.clip.json 을 스캔해 드롭다운에 채운다.
  - 스켈레톤을 로드하면, 이후 클립 드롭다운은 MotionPreviewState.CheckClipCompatible()로
    "이 스켈레톤과 실제로 맞물릴 수 있는 클립"만 남긴다(Phase 3의 §C10 검증 로직 재사용 -
    별도의 호환성 판정을 Python에서 다시 구현하지 않는다).
  - 베이스 클립 1개 + 레이어 N개(가중치/마스크/프레임) 스택을 편집한다.
  - 디스크의 임의 위치에서 커스텀 *.skeleton.json / *.clip.json 을 "가져오기"할 수 있다
    (유저가 만든 바이페드/키프레임 데이터 임포트 경로).

의도적 제약("UI blending 로직 재구현 금지" - 착수 계약서 §C): 이 파일은 weight/frame/mask
값을 MotionPreviewState(C++)에 그대로 전달만 한다. 실제 포즈 블렌딩(LayerMixer::MixLayers)
연산은 전부 C++ 쪽에서 일어나고, 이 패널은 그 결과를 알지도 못한다 - 매 프레임
ComputeBoneWorldLines()를 읽어 그리는 건 Renderer의 몫이다.
"""

from __future__ import annotations

import json
import os
from typing import Callable, Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel, QPushButton,
    QComboBox, QSlider, QFrame, QScrollArea, QFileDialog, QSizePolicy
)
from PySide6.QtCore import Qt, Signal

from style.theme import COLORS

from engine_binding import binding as ge_python, HAS_ENGINE

# engine/editor/panels/motion_mixer.py 기준 engine/assets/motion/
_MOTION_ASSETS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
    "assets", "motion",
)

# (내부 enum 이름, 드롭다운 표시 이름) - MaskPreset::Custom은 per-bone 맵 편집 UI가 아직
# 없어서(§C7 "posteri에" 슬롯, 1차 미사용) 여기 목록에서 뺀다.
_MASK_PRESET_ITEMS = [
    ("Full", "Full Body"),
    ("UpperBody", "Upper Body"),
    ("LowerBody", "Lower Body"),
]


def _read_text(path: str) -> Optional[str]:
    try:
        with open(path, "r", encoding="utf-8") as f:
            return f.read()
    except OSError:
        return None


def _extract_total_frames(clip_text: str) -> int:
    """Frame 슬라이더 범위를 정하기 위한 메타데이터 읽기 - 블렌딩과 무관한 순수 조회."""
    try:
        data = json.loads(clip_text)
        return max(1, int(data.get("totalFrames", 1)))
    except (ValueError, TypeError):
        return 1


class LayerRowWidget(QWidget):
    """레이어 하나(=MotionPreviewState의 layerId 하나)를 편집하는 행."""

    remove_requested = Signal(int)  # layerId

    def __init__(self, layer_id: int, clip_label: str, total_frames: int,
                 get_preview_state: Callable[[], object], parent=None):
        super().__init__(parent)
        self.layer_id = layer_id
        self._get_preview_state = get_preview_state
        self._total_frames = max(1, total_frames)
        self._build_ui(clip_label)

    def _build_ui(self, clip_label: str):
        outer = QVBoxLayout(self)
        outer.setContentsMargins(6, 3, 6, 3)
        outer.setSpacing(1)

        row1 = QHBoxLayout()
        lbl = QLabel(clip_label)
        lbl.setStyleSheet(f"color: {COLORS['text_primary']}; font-size: 11px; font-weight: bold;")
        btn_del = QPushButton("✕")
        btn_del.setFixedSize(18, 18)
        btn_del.setToolTip("레이어 제거")
        btn_del.clicked.connect(lambda: self.remove_requested.emit(self.layer_id))
        row1.addWidget(lbl, 1)
        row1.addWidget(btn_del)
        outer.addLayout(row1)

        # Weight/Frame 슬라이더는 한 글자 라벨(W/F)로 줄여서 슬라이더 자체가 더 넓게
        # 보이도록 한다 - 값은 옆의 숫자 라벨이 이미 보여주므로 "Weight"/"Frame"
        # 전체 텍스트가 없어도 헷갈리지 않는다(UI 단순화, 툴팁으로 원래 이름 유지).
        row2 = QHBoxLayout()
        lbl_w = QLabel("W")
        lbl_w.setFixedWidth(14)
        lbl_w.setToolTip("Weight")
        self.slider_weight = QSlider(Qt.Horizontal)
        self.slider_weight.setRange(0, 100)
        self.slider_weight.setValue(100)
        self.slider_weight.valueChanged.connect(self._on_weight_changed)
        self.lbl_weight_val = QLabel("1.00")
        self.lbl_weight_val.setFixedWidth(30)
        row2.addWidget(lbl_w)
        row2.addWidget(self.slider_weight, 1)
        row2.addWidget(self.lbl_weight_val)
        outer.addLayout(row2)

        row3 = QHBoxLayout()
        lbl_f = QLabel("F")
        lbl_f.setFixedWidth(14)
        lbl_f.setToolTip("Frame")
        self.slider_frame = QSlider(Qt.Horizontal)
        self.slider_frame.setRange(0, self._total_frames - 1)
        self.slider_frame.setValue(0)
        self.slider_frame.valueChanged.connect(self._on_frame_changed)
        self.lbl_frame_val = QLabel("0")
        self.lbl_frame_val.setFixedWidth(30)
        row3.addWidget(lbl_f)
        row3.addWidget(self.slider_frame, 1)
        row3.addWidget(self.lbl_frame_val)
        outer.addLayout(row3)

        row4 = QHBoxLayout()
        lbl_m = QLabel("M")
        lbl_m.setFixedWidth(14)
        lbl_m.setToolTip("Mask")
        self.combo_mask = QComboBox()
        for name, label in _MASK_PRESET_ITEMS:
            self.combo_mask.addItem(label, name)
        self.combo_mask.currentIndexChanged.connect(self._on_mask_changed)
        row4.addWidget(lbl_m)
        row4.addWidget(self.combo_mask, 1)
        outer.addLayout(row4)

        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet(f"background-color: {COLORS['border']}; max-height: 1px;")
        outer.addWidget(line)

        # 초기 상태(weight=1.0, frame=0, mask=Full)를 C++ 쪽에도 명시적으로 맞춰둔다 -
        # AddLayer 직후의 기본값과 UI 위젯 기본값이 실제로 같은지는 값을 한 번 보내서
        # 보장하는 편이 "둘 다 기본값이라 안 맞아도 우연히 일치"보다 안전하다.
        self._on_weight_changed(self.slider_weight.value())
        self._on_frame_changed(self.slider_frame.value())
        self._on_mask_changed(self.combo_mask.currentIndex())

    def _on_weight_changed(self, value: int):
        w = value / 100.0
        self.lbl_weight_val.setText(f"{w:.2f}")
        preview = self._get_preview_state()
        if preview is not None:
            preview.SetLayerWeight(self.layer_id, w)

    def _on_frame_changed(self, value: int):
        self.lbl_frame_val.setText(str(value))
        preview = self._get_preview_state()
        if preview is not None:
            preview.SetLayerFrame(self.layer_id, float(value))

    def _on_mask_changed(self, index: int):
        preview = self._get_preview_state()
        if preview is None or not HAS_ENGINE or index < 0:
            return
        name = self.combo_mask.itemData(index)
        mask_enum = getattr(ge_python.MaskPreset, name)
        preview.SetLayerMask(self.layer_id, mask_enum)


class MotionMixerPanel(QWidget):
    """
    Phase 5 믹서 UI 본체: 스켈레톤/베이스 클립 선택 + 레이어 스택 편집.

    get_preview_state_fn: () -> MotionPreviewState | None (엔진이 아직 초기화되지
    않았으면 None을 돌려주는 콜백 - AnimationPreviewPanel의 _get_preview_state를 그대로
    받는다).
    """

    def __init__(self, get_preview_state_fn: Callable[[], object], parent=None):
        super().__init__(parent)
        self._get_preview_state = get_preview_state_fn
        self._layer_rows: list[LayerRowWidget] = []
        self._build_ui()
        self.refresh_asset_lists()

    # ── UI 구성 ──────────────────────────────────────────────────────────────

    @staticmethod
    def _make_import_button(tooltip: str) -> QPushButton:
        """"가져오기…" 텍스트 버튼 대신 쓰는 컴팩트 버튼(UI 단순화). 폴더 이모지(📂)는
        이 폰트/크기에서 얇은 세로 막대로 깨져 보여서 어느 폰트에서도 안전한 말줄임표로
        대체한다 - 툴팁에 실제 동작을 적어둔다.

        theme.py의 기본 QPushButton QSS는 padding: 5px 12px라 26px 너비 안에서
        글자가 안 보일 정도로 잘려버린다(실측 확인) - 이 버튼만 패딩을 좁게 덮어쓴다."""
        btn = QPushButton("…")
        btn.setFixedWidth(26)
        btn.setToolTip(tooltip)
        btn.setStyleSheet("padding: 2px 0px;")
        return btn

    def _build_ui(self):
        self.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-top: 1px solid {COLORS['border']};")
        root = QVBoxLayout(self)
        root.setContentsMargins(8, 5, 8, 3)
        root.setSpacing(3)

        # 스켈레톤 행 — 콤보에서 항목을 고르는 순간(activated, 프로그램적 setCurrentIndex는
        # 무시) 바로 반영한다. 예전엔 "로드" 버튼을 따로 눌러야 했는데, 콤보 선택과
        # 별개의 동작일 이유가 없어서 없앴다(모션 에디터 UI 단순화).
        row_skel = QHBoxLayout()
        lbl_skel = QLabel("Skeleton")
        lbl_skel.setFixedWidth(52)
        lbl_skel.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        self.combo_skeleton = QComboBox()
        self.combo_skeleton.activated.connect(self._on_load_skeleton)
        btn_import_skel = self._make_import_button("스켈레톤 파일 가져오기")
        btn_import_skel.clicked.connect(self._on_import_skeleton)
        row_skel.addWidget(lbl_skel)
        row_skel.addWidget(self.combo_skeleton, 1)
        row_skel.addWidget(btn_import_skel)
        root.addLayout(row_skel)

        # 베이스 클립 행 — 위와 같은 이유로 "설정" 버튼 제거, 콤보 선택 시 바로 적용.
        row_base = QHBoxLayout()
        lbl_base = QLabel("Base")
        lbl_base.setFixedWidth(52)
        lbl_base.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        self.combo_base = QComboBox()
        self.combo_base.activated.connect(self._on_set_base_clip)
        btn_import_base = self._make_import_button("베이스 클립 파일 가져오기")
        btn_import_base.clicked.connect(self._on_import_base_clip)
        row_base.addWidget(lbl_base)
        row_base.addWidget(self.combo_base, 1)
        row_base.addWidget(btn_import_base)
        root.addLayout(row_base)

        # 레이어 추가 행 — "추가"는 남긴다: 콤보에서 클립을 훑어보는 것과 실제로 레이어
        # 스택에 얹는 것은(스택에 쌓이는 부작용이 있어) 같은 동작으로 합칠 수 없다.
        row_add = QHBoxLayout()
        lbl_add = QLabel("+ Layer")
        lbl_add.setFixedWidth(52)
        lbl_add.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px;")
        self.combo_add_layer = QComboBox()
        btn_add = QPushButton("추가")
        btn_add.setObjectName("btn_create")
        btn_add.setFixedWidth(44)
        btn_add.clicked.connect(self._on_add_layer)
        btn_import_layer = self._make_import_button("레이어용 클립 파일 가져오기")
        btn_import_layer.clicked.connect(self._on_import_layer_clip)
        row_add.addWidget(lbl_add)
        row_add.addWidget(self.combo_add_layer, 1)
        row_add.addWidget(btn_add)
        row_add.addWidget(btn_import_layer)
        root.addLayout(row_add)

        # 상태 라인
        self.lbl_status = QLabel("")
        self.lbl_status.setStyleSheet(f"color: {COLORS['text_dim']}; font-size: 10px;")
        root.addWidget(self.lbl_status)

        # 레이어 스택 (스크롤)
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setFrameShape(QFrame.NoFrame)
        self.layers_container = QWidget()
        self.layers_layout = QVBoxLayout(self.layers_container)
        self.layers_layout.setContentsMargins(0, 0, 0, 0)
        self.layers_layout.setSpacing(0)
        self.layers_layout.addStretch()
        scroll.setWidget(self.layers_container)
        root.addWidget(scroll, 1)

    # ── 자산 스캔 ─────────────────────────────────────────────────────────────

    def refresh_asset_lists(self):
        """assets/motion/ 을 다시 스캔한다. 스켈레톤이 로드돼 있으면 클립 목록을
        CheckClipCompatible()로 필터링한다."""
        preview = self._get_preview_state()

        prev_skel = self.combo_skeleton.currentData()
        self.combo_skeleton.clear()
        for name in self._scan_dir(".skeleton.json"):
            self.combo_skeleton.addItem(name, os.path.join(_MOTION_ASSETS_DIR, name))
        if prev_skel:
            idx = self.combo_skeleton.findData(prev_skel)
            if idx >= 0:
                self.combo_skeleton.setCurrentIndex(idx)

        self.combo_base.clear()
        self.combo_add_layer.clear()
        for name in self._scan_dir(".clip.json"):
            path = os.path.join(_MOTION_ASSETS_DIR, name)
            if preview is not None and preview.HasSkeleton():
                text = _read_text(path)
                if text is None or not preview.CheckClipCompatible(text):
                    continue  # 호환 안 되는 클립은 dropdown에서 제외
            self.combo_base.addItem(name, path)
            self.combo_add_layer.addItem(name, path)

    @staticmethod
    def _scan_dir(suffix: str) -> list[str]:
        if not os.path.isdir(_MOTION_ASSETS_DIR):
            return []
        return sorted(f for f in os.listdir(_MOTION_ASSETS_DIR) if f.endswith(suffix))

    # ── 스켈레톤 ─────────────────────────────────────────────────────────────

    def _on_load_skeleton(self):
        path = self.combo_skeleton.currentData()
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        self._load_skeleton_text(text, os.path.basename(path))

    def _on_import_skeleton(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "스켈레톤 가져오기", _MOTION_ASSETS_DIR, "Skeleton Files (*.skeleton.json)"
        )
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        name = os.path.basename(path)
        if self.combo_skeleton.findData(path) < 0:
            self.combo_skeleton.addItem(name, path)
        self.combo_skeleton.setCurrentIndex(self.combo_skeleton.findData(path))
        self._load_skeleton_text(text, name)

    def _load_skeleton_text(self, text: str, label: str):
        preview = self._get_preview_state()
        if preview is None:
            self._status("엔진이 아직 초기화되지 않았습니다", error=True)
            return
        if not preview.LoadSkeleton(text):
            self._status(f"스켈레톤 로드 실패: {preview.GetLastError()}", error=True)
            return
        # LoadSkeleton()은 C++ 쪽에서 이전 베이스 클립/레이어를 전부 비운다 - UI도 맞춘다.
        self._clear_layer_rows()
        self.combo_base.setCurrentIndex(-1)
        self.refresh_asset_lists()
        self._status(f"스켈레톤 로드 완료: {label}")

    # ── 베이스 클립 ──────────────────────────────────────────────────────────

    def _on_set_base_clip(self):
        path = self.combo_base.currentData()
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        self._set_base_clip_text(text, os.path.basename(path))

    def _on_import_base_clip(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "베이스 클립 가져오기", _MOTION_ASSETS_DIR, "Clip Files (*.clip.json)"
        )
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        name = os.path.basename(path)
        if self.combo_base.findData(path) < 0:
            self.combo_base.addItem(name, path)
        self.combo_base.setCurrentIndex(self.combo_base.findData(path))
        self._set_base_clip_text(text, name)

    def _set_base_clip_text(self, text: str, label: str):
        preview = self._get_preview_state()
        if preview is None:
            self._status("엔진이 아직 초기화되지 않았습니다", error=True)
            return
        if not preview.LoadBaseClip(text):
            self._status(f"베이스 클립 로드 실패: {preview.GetLastError()}", error=True)
            return
        preview.SetBaseFrame(0.0)
        self._status(f"베이스 클립 설정 완료: {label}")

    # ── 레이어 스택 ──────────────────────────────────────────────────────────

    def _on_add_layer(self):
        path = self.combo_add_layer.currentData()
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        self._add_layer_text(text, os.path.basename(path))

    def _on_import_layer_clip(self):
        path, _ = QFileDialog.getOpenFileName(
            self, "레이어 클립 가져오기", _MOTION_ASSETS_DIR, "Clip Files (*.clip.json)"
        )
        if not path:
            return
        text = _read_text(path)
        if text is None:
            self._status(f"파일을 읽을 수 없습니다: {path}", error=True)
            return
        name = os.path.basename(path)
        if self.combo_add_layer.findData(path) < 0:
            self.combo_add_layer.addItem(name, path)
        self._add_layer_text(text, name)

    def _add_layer_text(self, text: str, label: str):
        preview = self._get_preview_state()
        if preview is None:
            self._status("엔진이 아직 초기화되지 않았습니다", error=True)
            return
        layer_id = preview.AddLayer(text)
        if layer_id < 0:
            self._status(f"레이어 추가 실패: {preview.GetLastError()}", error=True)
            return
        row = LayerRowWidget(layer_id, label, _extract_total_frames(text), self._get_preview_state)
        row.remove_requested.connect(self._on_remove_layer)
        # addStretch()로 끝나는 레이아웃이라 마지막(stretch) 앞에 끼워 넣는다.
        self.layers_layout.insertWidget(self.layers_layout.count() - 1, row)
        self._layer_rows.append(row)
        self._status(f"레이어 추가: {label}")

    def _on_remove_layer(self, layer_id: int):
        preview = self._get_preview_state()
        if preview is not None:
            preview.RemoveLayer(layer_id)
        row = next((r for r in self._layer_rows if r.layer_id == layer_id), None)
        if row is not None:
            self._layer_rows.remove(row)
            row.deleteLater()

    def _clear_layer_rows(self):
        for row in self._layer_rows:
            row.deleteLater()
        self._layer_rows.clear()

    # ── 데모용 초기 상태 (Motion Editor 진입 시 아무 것도 안 실려있으면 자동 호출) ──

    def load_default_sample(self):
        """번들된 샘플 스켈레톤 + Idle 베이스 + Attack 레이어(weight 0.5)를 미리 채워
        Mixer UI가 뭘 하는 물건인지 바로 보여준다. 이후 사용자가 자유롭게 레이어를
        추가/조절/제거할 수 있다."""
        skel_path = os.path.join(_MOTION_ASSETS_DIR, "sample.skeleton.json")
        idle_path = os.path.join(_MOTION_ASSETS_DIR, "idle.clip.json")
        attack_path = os.path.join(_MOTION_ASSETS_DIR, "attack.clip.json")

        skel_text = _read_text(skel_path)
        idle_text = _read_text(idle_path)
        attack_text = _read_text(attack_path)
        if skel_text is None or idle_text is None or attack_text is None:
            self._status("샘플 자산 파일을 찾을 수 없습니다", error=True)
            return

        self._load_skeleton_text(skel_text, "sample.skeleton.json")
        idx = self.combo_skeleton.findData(skel_path)
        if idx >= 0:
            self.combo_skeleton.setCurrentIndex(idx)

        self._set_base_clip_text(idle_text, "idle.clip.json")
        idx = self.combo_base.findData(idle_path)
        if idx >= 0:
            self.combo_base.setCurrentIndex(idx)

        self._add_layer_text(attack_text, "attack.clip.json")
        if self._layer_rows:
            last = self._layer_rows[-1]
            last.slider_weight.setValue(50)   # 데모 시작값: base/attack 절반씩 블렌드
            last.slider_frame.setValue(30)

    # ── 기타 ─────────────────────────────────────────────────────────────────

    def _status(self, text: str, error: bool = False):
        self.lbl_status.setText(text)
        self.lbl_status.setStyleSheet(
            f"color: {COLORS['accent_danger'] if error else COLORS['text_dim']}; font-size: 10px;"
        )
