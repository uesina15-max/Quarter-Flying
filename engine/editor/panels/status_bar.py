"""
Status Bar Panel
하단 상태 바 — FPS, 엔티티 수, 로그 메시지 표시
"""

from PySide6.QtWidgets import QWidget, QHBoxLayout, QLabel, QFrame
from PySide6.QtCore import Qt, QTimer
import time

from style.theme import COLORS


class StatusBar(QWidget):
    """
    에디터 하단 상태 바

    표시 항목:
    - FPS (엔진 프레임 기반 또는 자체 계산)
    - 총 엔티티 수
    - 선택된 엔티티 정보
    - 마지막 로그 메시지
    - 엔진 연결 상태
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setFixedHeight(24)
        self.setObjectName("status_bar_main")
        self.setStyleSheet(
            f"background-color: {COLORS['bg_header']}; "
            f"border-top: 1px solid {COLORS['border']};"
        )

        self._frame_count = 0
        self._last_fps_time = time.time()
        self._fps = 0.0
        self._entity_count = 0
        self._selected_id = -1

        self._build_ui()
        self._start_fps_timer()

    # ----------------------------------------------------------------
    # UI 구성
    # ----------------------------------------------------------------

    def _build_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(8, 0, 8, 0)
        layout.setSpacing(0)

        # 엔진 상태 표시기
        self.lbl_engine_status = self._make_label("● OFFLINE", COLORS['accent_danger'])
        layout.addWidget(self.lbl_engine_status)

        layout.addWidget(self._make_separator())

        # FPS
        self.lbl_fps = self._make_label("FPS: --")
        layout.addWidget(self.lbl_fps)

        layout.addWidget(self._make_separator())

        # 엔티티 수
        self.lbl_entities = self._make_label("Entities: 0")
        layout.addWidget(self.lbl_entities)

        layout.addWidget(self._make_separator())

        # 선택 정보
        self.lbl_selection = self._make_label("No Selection", color=COLORS['text_secondary'])
        layout.addWidget(self.lbl_selection)

        # 중앙 여백
        layout.addStretch()

        # 로그 메시지 (우측)
        self.lbl_log = self._make_label("", color=COLORS['text_dim'])
        layout.addWidget(self.lbl_log)

        layout.addWidget(self._make_separator())

        # 버전 정보
        ver = self._make_label("Quarter Flying  v0.1.0", color=COLORS['text_dim'])
        layout.addWidget(ver)

    def _make_label(self, text: str, color: str = None) -> QLabel:
        if color is None:
            color = COLORS['text_secondary']
        lbl = QLabel(text)
        lbl.setStyleSheet(
            f"color: {color}; font-size: 10px; "
            f"background-color: transparent; padding: 0px 6px;"
        )
        return lbl

    def _make_separator(self) -> QFrame:
        sep = QFrame()
        sep.setFrameShape(QFrame.VLine)
        sep.setStyleSheet(f"color: {COLORS['border']};")
        sep.setFixedWidth(1)
        return sep

    # ----------------------------------------------------------------
    # FPS 계산
    # ----------------------------------------------------------------

    def _start_fps_timer(self):
        self._fps_timer = QTimer(self)
        self._fps_timer.timeout.connect(self._update_fps_display)
        self._fps_timer.start(1000)  # 1초마다 갱신

    def tick_frame(self):
        """EngineViewport.tick() 마다 호출하여 FPS 계산"""
        self._frame_count += 1

    def _update_fps_display(self):
        self._fps = self._frame_count
        self._frame_count = 0
        if self._fps >= 55:
            color = COLORS['accent_green']
        elif self._fps >= 30:
            color = COLORS['accent_orange']
        else:
            color = COLORS['accent_danger']
        self.lbl_fps.setText(f"FPS: {int(self._fps)}")
        self.lbl_fps.setStyleSheet(
            f"color: {color}; font-size: 10px; "
            f"background-color: transparent; padding: 0px 6px;"
        )

    # ----------------------------------------------------------------
    # Public API
    # ----------------------------------------------------------------

    def set_engine_online(self, online: bool):
        if online:
            self.lbl_engine_status.setText("● ONLINE")
            self.lbl_engine_status.setStyleSheet(
                f"color: {COLORS['accent_green']}; font-size: 10px; font-weight: bold; padding: 0px 6px;"
            )
        else:
            self.lbl_engine_status.setText("● OFFLINE")
            self.lbl_engine_status.setStyleSheet(
                f"color: {COLORS['accent_danger']}; font-size: 10px; font-weight: bold; padding: 0px 6px;"
            )

    def set_entity_count(self, count: int):
        self._entity_count = count
        self.lbl_entities.setText(f"Entities: {count}")

    def set_selection(self, entity_id: int, name: str = ""):
        self._selected_id = entity_id
        if entity_id < 0:
            self.lbl_selection.setText("No Selection")
            self.lbl_selection.setStyleSheet(
                f"color: {COLORS['text_dim']}; font-size: 10px; padding: 0px 6px;"
            )
        else:
            display = name if name else f"Entity_{entity_id}"
            self.lbl_selection.setText(f"Selected: {display}  [{entity_id}]")
            self.lbl_selection.setStyleSheet(
                f"color: {COLORS['accent']}; font-size: 10px; padding: 0px 6px;"
            )

    def log(self, message: str):
        """짧은 로그 메시지를 상태 바에 표시 (3초 후 자동 소거)"""
        self.lbl_log.setText(f"▸ {message}")
        self.lbl_log.setStyleSheet(
            f"color: {COLORS['accent_orange']}; font-size: 10px; padding: 0px 6px;"
        )
        QTimer.singleShot(3000, self._clear_log)

    def _clear_log(self):
        self.lbl_log.setText("")
