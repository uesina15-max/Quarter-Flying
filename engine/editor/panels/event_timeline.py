"""
editor/panels/event_timeline.py

Event Timeline — 커스텀 QPainter 타임라인.

기능:
  - 섹션 블록 (SectionRole 기반 색상)
  - 이벤트 마커 ◆ (드래그로 이동)
  - 재생 헤드 (Playhead) 드래그
  - 마우스 휠 Zoom In/Out
  - 우클릭 → 이벤트 추가 메뉴
  - 섹션 경계 드래그
"""

from __future__ import annotations

import math
from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QMenu, QSizePolicy, QScrollBar,
    QInputDialog
)
from PySide6.QtCore import Qt, Signal, QRect, QPoint, QRectF, QPointF
from PySide6.QtGui import (
    QPainter, QColor, QPen, QBrush, QFont,
    QMouseEvent, QWheelEvent, QContextMenuEvent,
    QLinearGradient, QPainterPath
)

from style.theme import COLORS

from core.action_data import (
    ActionData, ActionEvent, ActionSection,
    EventType, SectionRole,
    SECTION_ROLE_COLORS, EVENT_TYPE_COLORS
)


# ─────────────────────────────────────────────────────────────────────────────
# 상수
# ─────────────────────────────────────────────────────────────────────────────

RULER_HEIGHT       = 28   # 눈금자 영역 높이
SECTION_BAND_H     = 24   # 섹션 블록 영역 높이
EVENT_ROW_H        = 20   # 이벤트 마커 행 높이
PLAYHEAD_WIDTH     = 2
MARKER_SIZE        = 8    # 마커 다이아몬드 크기
SECTION_DRAG_ZONE  = 6    # 섹션 경계 드래그 감지 픽셀 반경
MIN_PX_PER_FRAME   = 2
MAX_PX_PER_FRAME   = 40


# ─────────────────────────────────────────────────────────────────────────────
# 타임라인 캔버스
# ─────────────────────────────────────────────────────────────────────────────

class TimelineCanvas(QWidget):
    """
    QPainter로 직접 그리는 타임라인 캔버스.
    외부와의 통신은 Signal로만.
    """

    frame_clicked    = Signal(int)          # 프레임 클릭
    event_moved      = Signal(object, int)  # (ActionEvent, new_frame)
    event_added      = Signal(int, str)     # (frame, EventType.value)
    event_deleted    = Signal(object)       # ActionEvent
    section_resized  = Signal()             # 섹션 경계 변경됨
    data_changed     = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._current_frame = 0
        self._px_per_frame  = 8.0   # Zoom
        self._scroll_offset = 0     # 픽셀 단위 수평 스크롤

        # 드래그 상태
        self._drag_mode     = None   # "playhead" / "event" / "section_boundary"
        self._drag_event: Optional[ActionEvent] = None
        self._drag_section_idx: int = -1   # 경계 드래그 중인 섹션 인덱스

        self.setMinimumHeight(RULER_HEIGHT + SECTION_BAND_H + EVENT_ROW_H * 4 + 8)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Fixed)
        self.setMouseTracking(True)
        self.setCursor(Qt.ArrowCursor)

    # ── 공개 API ─────────────────────────────────────────────────────────────

    def set_action(self, action: Optional[ActionData]):
        self._action = action
        self._current_frame = 0
        self.update()

    def set_current_frame(self, frame: int):
        self._current_frame = frame
        self.update()

    def zoom(self, delta: float):
        self._px_per_frame = max(MIN_PX_PER_FRAME,
                                 min(MAX_PX_PER_FRAME,
                                     self._px_per_frame * (1 + delta)))
        self.update()

    def set_scroll(self, offset_px: int):
        self._scroll_offset = offset_px
        self.update()

    def total_width(self) -> int:
        if self._action is None:
            return self.width()
        return max(self.width(),
                   int(self._action.total_frames * self._px_per_frame) + 60)

    # ── 좌표 변환 ─────────────────────────────────────────────────────────────

    def _frame_to_x(self, frame: int) -> float:
        return frame * self._px_per_frame - self._scroll_offset + 40

    def _x_to_frame(self, x: float) -> int:
        raw = (x + self._scroll_offset - 40) / self._px_per_frame
        if self._action:
            return max(0, min(int(round(raw)), self._action.total_frames))
        return max(0, int(round(raw)))

    # ── 그리기 ───────────────────────────────────────────────────────────────

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)

        w = self.width()
        h = self.height()

        # 배경
        p.fillRect(0, 0, w, h, QColor("#0f1922"))

        if self._action is None:
            p.setPen(QColor("#334455"))
            p.drawText(QRect(0, 0, w, h), Qt.AlignCenter, "액션을 선택하세요")
            return

        self._draw_ruler(p)
        self._draw_sections(p)
        self._draw_events(p)
        self._draw_playhead(p)

        p.end()

    def _draw_ruler(self, p: QPainter):
        """프레임 눈금자 (맨 위)."""
        w = self.width()
        fps = self._action.fps if self._action else 30
        total = self._action.total_frames if self._action else 60

        p.fillRect(0, 0, w, RULER_HEIGHT, QColor("#0d1b2a"))
        p.setPen(QColor("#2a5080"))
        p.drawLine(0, RULER_HEIGHT, w, RULER_HEIGHT)

        # 눈금 간격 결정 (5, 10, 15, 30, 60 프레임)
        candidates = [1, 2, 5, 10, 15, 30, 60, 120]
        tick_interval = 5
        for c in candidates:
            if c * self._px_per_frame >= 40:
                tick_interval = c
                break

        font = QFont("Segoe UI", 8)
        p.setFont(font)

        frame = 0
        while frame <= total:
            x = self._frame_to_x(frame)
            if 0 <= x <= w:
                # 눈금 선
                p.setPen(QColor("#2a5080"))
                p.drawLine(int(x), RULER_HEIGHT - 6, int(x), RULER_HEIGHT)

                # 레이블 (적당히 필터)
                if tick_interval > 0 and frame % tick_interval == 0:
                    p.setPen(QColor("#8899aa"))
                    label = str(frame)
                    p.drawText(int(x) - 12, 4, 30, RULER_HEIGHT - 6,
                               Qt.AlignLeft | Qt.AlignVCenter, label)
            frame += max(1, tick_interval)

    def _draw_sections(self, p: QPainter):
        """섹션 컬러 블록."""
        y = RULER_HEIGHT
        h = SECTION_BAND_H
        total = self._action.total_frames

        # 배경
        p.fillRect(0, y, self.width(), h, QColor("#0a1520"))

        for section in self._action.sections:
            x1 = self._frame_to_x(section.start_frame)
            x2 = self._frame_to_x(section.end_frame)
            color = QColor(SECTION_ROLE_COLORS.get(section.role, "#333"))

            # 섹션 블록
            rect = QRectF(x1, y + 2, x2 - x1, h - 4)
            color_fill = QColor(color)
            color_fill.setAlpha(140)
            p.fillRect(rect.toRect(), color_fill)

            # 경계선
            p.setPen(QPen(color, 1.5))
            p.drawRect(rect.toRect())

            # 이름 레이블
            if rect.width() > 30:
                p.setPen(QColor("#e0e0e0"))
                font = QFont("Segoe UI", 8)
                font.setBold(True)
                p.setFont(font)
                p.drawText(rect.toRect().adjusted(4, 0, -2, 0),
                           Qt.AlignVCenter | Qt.AlignLeft,
                           f"{section.name}  {int(section.speed * 100)}%")

    def _draw_events(self, p: QPainter):
        """이벤트 마커 ◆"""
        y_base = RULER_HEIGHT + SECTION_BAND_H + 4

        # 이벤트 타입별 행 인덱스
        type_order = list(EventType)

        for event in self._action.events:
            row_idx = type_order.index(event.type) if event.type in type_order else 0
            x = self._frame_to_x(event.frame)
            y = y_base + row_idx * EVENT_ROW_H

            color = QColor(EVENT_TYPE_COLORS.get(event.type, "#ffffff"))

            # 다이아몬드 ◆
            half = MARKER_SIZE // 2
            path = QPainterPath()
            path.moveTo(x,        y)
            path.lineTo(x + half, y + half)
            path.lineTo(x,        y + half * 2)
            path.lineTo(x - half, y + half)
            path.closeSubpath()

            p.fillPath(path, QBrush(color))
            p.setPen(QPen(color.lighter(150), 1))
            p.drawPath(path)

            # 타입 레이블
            p.setPen(color)
            font = QFont("Segoe UI", 7)
            p.setFont(font)
            p.drawText(int(x) - 20, int(y) + MARKER_SIZE * 2 + 1, 50, 14,
                       Qt.AlignLeft, event.type.value)

    def _draw_playhead(self, p: QPainter):
        """재생 헤드 수직선."""
        x = int(self._frame_to_x(self._current_frame))
        if not (0 <= x <= self.width()):
            return

        h = self.height()
        pen = QPen(QColor("#00d4ff"), PLAYHEAD_WIDTH)
        p.setPen(pen)
        p.drawLine(x, RULER_HEIGHT, x, h)

        # 삼각형 헤드
        path = QPainterPath()
        path.moveTo(x - 5, RULER_HEIGHT)
        path.lineTo(x + 5, RULER_HEIGHT)
        path.lineTo(x,     RULER_HEIGHT + 10)
        path.closeSubpath()
        p.fillPath(path, QBrush(QColor("#00d4ff")))

    # ── 마우스 이벤트 ─────────────────────────────────────────────────────────

    def mousePressEvent(self, event: QMouseEvent):
        x, y = event.position().x(), event.position().y()

        if event.button() == Qt.LeftButton:
            # 재생 헤드 영역
            if y < RULER_HEIGHT:
                self._drag_mode = "playhead"
                frame = self._x_to_frame(x)
                self._current_frame = frame
                self.frame_clicked.emit(frame)
                self.update()
                return

            # 이벤트 마커 선택
            if self._action:
                hit = self._hit_test_event(x, y)
                if hit is not None:
                    self._drag_mode  = "event"
                    self._drag_event = hit
                    return

            # 섹션 경계 드래그
            if self._action and (RULER_HEIGHT <= y <= RULER_HEIGHT + SECTION_BAND_H):
                idx = self._hit_test_section_boundary(x)
                if idx >= 0:
                    self._drag_mode = "section_boundary"
                    self._drag_section_idx = idx
                    return

            # 빈 공간 → playhead 이동
            self._drag_mode = "playhead"
            frame = self._x_to_frame(x)
            self._current_frame = frame
            self.frame_clicked.emit(frame)
            self.update()

    def mouseMoveEvent(self, event: QMouseEvent):
        x = event.position().x()
        y = event.position().y()

        # 커서 힌트
        if self._action and (RULER_HEIGHT <= y <= RULER_HEIGHT + SECTION_BAND_H):
            if self._hit_test_section_boundary(x) >= 0:
                self.setCursor(Qt.SizeHorCursor)
            else:
                self.setCursor(Qt.ArrowCursor)
        else:
            self.setCursor(Qt.ArrowCursor)

        if not (event.buttons() & Qt.LeftButton):
            return

        if self._drag_mode == "playhead":
            frame = self._x_to_frame(x)
            self._current_frame = frame
            self.frame_clicked.emit(frame)
            self.update()

        elif self._drag_mode == "event" and self._drag_event:
            frame = self._x_to_frame(x)
            self._drag_event.frame = max(0, frame)
            self.event_moved.emit(self._drag_event, self._drag_event.frame)
            self.update()

        elif self._drag_mode == "section_boundary" and self._action:
            idx = self._drag_section_idx
            frame = max(0, self._x_to_frame(x))
            sections = self._action.sections
            if 0 <= idx < len(sections):
                sections[idx].end_frame = frame
                if idx + 1 < len(sections):
                    sections[idx + 1].start_frame = frame
            self.section_resized.emit()
            self.update()

    def mouseReleaseEvent(self, event: QMouseEvent):
        if self._drag_mode in ("section_boundary",):
            self.data_changed.emit()
        self._drag_mode      = None
        self._drag_event     = None
        self._drag_section_idx = -1

    def wheelEvent(self, event: QWheelEvent):
        delta = event.angleDelta().y()
        if delta > 0:
            self.zoom(0.15)
        else:
            self.zoom(-0.15)

    def contextMenuEvent(self, event: QContextMenuEvent):
        if self._action is None:
            return

        x = event.pos().x()
        frame = self._x_to_frame(x)

        # 이벤트 마커 우클릭 → 삭제
        hit = self._hit_test_event(x, event.pos().y())
        if hit:
            menu = QMenu(self)
            act_del = menu.addAction(f"'{hit.type.value}' 이벤트 삭제 (f:{hit.frame})")
            chosen = menu.exec(event.globalPos())
            if chosen == act_del:
                self._action.events.remove(hit)
                self.event_deleted.emit(hit)
                self.update()
            return

        # 빈 공간 우클릭 → 이벤트 추가
        menu = QMenu(self)
        menu.addSection(f"Frame {frame}에 이벤트 추가")
        for etype in EventType:
            color = EVENT_TYPE_COLORS.get(etype, "#fff")
            act = menu.addAction(f"◆ {etype.value}")
            act.setData(etype)
        chosen = menu.exec(event.globalPos())
        if chosen and chosen.data() is not None:
            etype = chosen.data()
            new_event = ActionEvent(frame=frame, type=etype)
            self._action.events.append(new_event)
            self._action.events.sort(key=lambda e: e.frame)
            self.event_added.emit(frame, etype.value)
            self.update()

    # ── 히트 테스트 ───────────────────────────────────────────────────────────

    def _hit_test_event(self, x: float, y: float) -> Optional[ActionEvent]:
        if self._action is None:
            return None
        y_base = RULER_HEIGHT + SECTION_BAND_H + 4
        type_order = list(EventType)
        for event in self._action.events:
            ex = self._frame_to_x(event.frame)
            row_idx = type_order.index(event.type) if event.type in type_order else 0
            ey = y_base + row_idx * EVENT_ROW_H + MARKER_SIZE
            if abs(x - ex) < MARKER_SIZE and abs(y - ey) < MARKER_SIZE:
                return event
        return None

    def _hit_test_section_boundary(self, x: float) -> int:
        """섹션 경계 픽셀 근처면 섹션 인덱스 반환, 없으면 -1."""
        if self._action is None:
            return -1
        for i, section in enumerate(self._action.sections):
            bx = self._frame_to_x(section.end_frame)
            if abs(x - bx) < SECTION_DRAG_ZONE:
                return i
        return -1


# ─────────────────────────────────────────────────────────────────────────────
# 타임라인 패널 (캔버스 + 컨트롤 바)
# ─────────────────────────────────────────────────────────────────────────────

class EventTimelinePanel(QWidget):
    """
    하단 패널: 타임라인 컨트롤 바 + TimelineCanvas.

    Signals:
        frame_changed(int)   — 재생 헤드 이동 시
        data_changed()       — 이벤트/섹션 변경 시
    """

    frame_changed = Signal(int)
    data_changed  = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 컨트롤 바
        ctrl = QWidget()
        ctrl.setFixedHeight(30)
        ctrl.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-bottom: 1px solid {COLORS['border']};")
        ctrl_layout = QHBoxLayout(ctrl)
        ctrl_layout.setContentsMargins(6, 2, 6, 2)
        ctrl_layout.setSpacing(6)

        lbl = QLabel("EVENT TIMELINE")
        lbl.setStyleSheet(f"color: {COLORS['accent']}; font-size: 10px; font-weight: bold; letter-spacing: 1px;")
        ctrl_layout.addWidget(lbl)

        ctrl_layout.addStretch()

        # 줌 버튼
        btn_zoom_out = QPushButton("－")
        btn_zoom_out.setFixedSize(22, 22)
        btn_zoom_out.setToolTip("Zoom Out")
        btn_zoom_out.clicked.connect(lambda: self.canvas.zoom(-0.2))

        btn_zoom_in = QPushButton("＋")
        btn_zoom_in.setFixedSize(22, 22)
        btn_zoom_in.setToolTip("Zoom In")
        btn_zoom_in.clicked.connect(lambda: self.canvas.zoom(0.2))

        self.lbl_frame = QLabel("Frame: 0")
        self.lbl_frame.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 11px; min-width: 70px;")

        ctrl_layout.addWidget(QLabel("Zoom"))
        ctrl_layout.addWidget(btn_zoom_out)
        ctrl_layout.addWidget(btn_zoom_in)
        ctrl_layout.addWidget(self.lbl_frame)

        layout.addWidget(ctrl)

        # 캔버스
        self.canvas = TimelineCanvas()
        self.canvas.frame_clicked.connect(self._on_frame_clicked)
        self.canvas.event_moved.connect(lambda e, f: self.data_changed.emit())
        self.canvas.event_added.connect(lambda f, t: self.data_changed.emit())
        self.canvas.event_deleted.connect(lambda e: self.data_changed.emit())
        self.canvas.section_resized.connect(self.data_changed.emit)
        self.canvas.data_changed.connect(self.data_changed.emit)
        layout.addWidget(self.canvas)

        # 수평 스크롤바
        self.scrollbar = QScrollBar(Qt.Horizontal)
        self.scrollbar.setFixedHeight(12)
        self.scrollbar.valueChanged.connect(self.canvas.set_scroll)
        layout.addWidget(self.scrollbar)

    # ── 공개 API ─────────────────────────────────────────────────────────────

    def set_action(self, action: Optional[ActionData]):
        self.canvas.set_action(action)
        self._update_scrollbar()

    def set_current_frame(self, frame: int):
        self.canvas.set_current_frame(frame)
        self.lbl_frame.setText(f"Frame: {frame}")

    # ── 내부 ─────────────────────────────────────────────────────────────────

    def _on_frame_clicked(self, frame: int):
        self.lbl_frame.setText(f"Frame: {frame}")
        self.frame_changed.emit(frame)

    def _update_scrollbar(self):
        total_w = self.canvas.total_width()
        visible = self.canvas.width()
        self.scrollbar.setRange(0, max(0, total_w - visible))
        self.scrollbar.setPageStep(visible)
