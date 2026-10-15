"""
editor/panels/motion_graph.py

Motion Graph Panel — 섹션별 재생 속도 배율 표시기.

선택된 ActionData의 섹션마다 speed(재생 속도 배율)를 계단 모양으로 그린다. 데이터에 있는 값만 그린다.

예전에는 섹션 안에서 sin 곡선으로 "속도"를, 그 차이로 "가속도"를 지어내서 그렸다. 섹션에는 배율 하나뿐이고
본의 실제 속도 데이터는 어디에도 없는데, 그래프는 실제 측정값처럼 보였다(조용히 틀린 결과,
docs/USABILITY_IMPROVEMENT_PLAN.md A-2). 본 속도 분석이 필요해지면 클립 데이터에서 계산해 따로 그린다.
"""

from __future__ import annotations

from typing import Optional

from PySide6.QtWidgets import QWidget, QVBoxLayout, QLabel, QHBoxLayout, QSizePolicy
from PySide6.QtCore import Qt, Signal, QRectF, QPointF
from PySide6.QtGui import QPainter, QColor, QPen, QBrush, QPainterPath, QFont, QLinearGradient

from core.action_data import ActionData, SectionRole, SECTION_ROLE_COLORS
from style.theme import COLORS


class GraphCanvas(QWidget):
    """실제 그래프를 그리는 영역."""
    
    frame_clicked = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._current_frame = 0
        self.setMinimumHeight(120)
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def set_action(self, action: Optional[ActionData]):
        self._action = action
        self.update()

    def set_current_frame(self, frame: int):
        self._current_frame = frame
        self.update()

    def paintEvent(self, event):
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing)
        
        w = self.width()
        h = self.height()
        
        # 배경
        p.fillRect(0, 0, w, h, QColor("#0a1520"))
        
        # 그리드 라인
        p.setPen(QPen(QColor("#1e3a5f"), 1, Qt.DotLine))
        for i in range(1, 4):
            y = int(h * i / 4)
            p.drawLine(0, y, w, y)
            
        if not self._action or self._action.total_frames <= 0:
            p.setPen(QColor("#334455"))
            p.drawText(self.rect(), Qt.AlignCenter, "No Data")
            return

        total_frames = self._action.total_frames
        
        # 섹션 배경색 칠하기
        for section in self._action.sections:
            x1 = int((section.start_frame / total_frames) * w)
            x2 = int((section.end_frame / total_frames) * w)
            color = QColor(SECTION_ROLE_COLORS.get(section.role, "#333"))
            color.setAlpha(30)
            p.fillRect(x1, 0, x2 - x1, h, color)
        
        if not self._action.sections:
            p.setPen(QColor("#556677"))
            p.drawText(self.rect(), Qt.AlignCenter, "섹션 없음 — 표시할 속도 배율이 없습니다")
            self._draw_playhead(p, total_frames, w, h)
            return

        # 섹션 speed(배율)를 계단 모양으로. 섹션 밖은 1.0(기본 배율, core/action_data.py ActionSection.speed).
        max_speed = max(2.0, max(sec.speed for sec in self._action.sections) * 1.1)

        def speed_at(frame):
            for sec in self._action.sections:
                if sec.start_frame <= frame <= sec.end_frame:
                    return sec.speed
            return 1.0

        def y_of(speed):
            return h - (min(speed, max_speed) / max_speed) * h * 0.8   # 상단 여백 20%

        points_speed = []
        for frame in range(total_frames + 1):
            x = (frame / total_frames) * w
            y = y_of(speed_at(frame))
            if points_speed and abs(points_speed[-1].y() - y) > 1e-6:
                points_speed.append(QPointF(x, points_speed[-1].y()))   # 값이 바뀌는 자리에서 수직으로
            points_speed.append(QPointF(x, y))

        # 기준선 1.0x
        p.setPen(QPen(QColor("#2e4d6b"), 1, Qt.DashLine))
        p.drawLine(0, int(y_of(1.0)), w, int(y_of(1.0)))

        # 속도 곡선 그리기
        if len(points_speed) > 1:
            path_speed = QPainterPath()
            path_speed.moveTo(points_speed[0])
            for pt in points_speed[1:]:
                path_speed.lineTo(pt)
            
            # 그라데이션 채우기
            fill_path = QPainterPath(path_speed)
            fill_path.lineTo(w, h)
            fill_path.lineTo(0, h)
            fill_path.closeSubpath()
            
            grad = QLinearGradient(0, 0, 0, h)
            grad.setColorAt(0.0, QColor(0, 212, 255, 80))
            grad.setColorAt(1.0, QColor(0, 212, 255, 0))
            p.fillPath(fill_path, QBrush(grad))
            
            p.setPen(QPen(QColor(0, 212, 255, 255), 2))
            p.drawPath(path_speed)

        self._draw_playhead(p, total_frames, w, h)

        # 범례
        p.setPen(QColor(0, 212, 255, 255))
        p.drawText(10, 20, f"── 섹션 재생 속도 배율 (점선 = 1.0x, 위쪽 끝 = {max_speed:.1f}x)")

    def _draw_playhead(self, p, total_frames, w, h):
        px = int((self._current_frame / total_frames) * w)
        p.setPen(QPen(QColor("#ffffff"), 1))
        p.drawLine(px, 0, px, h)

    def mousePressEvent(self, event):
        if not self._action or self._action.total_frames <= 0:
            return
        w = self.width()
        x = max(0, min(w, event.pos().x()))
        frame = int((x / w) * self._action.total_frames)
        self.frame_clicked.emit(frame)
        self.set_current_frame(frame)

    def mouseMoveEvent(self, event):
        if event.buttons() & Qt.LeftButton:
            self.mousePressEvent(event)


class MotionGraphPanel(QWidget):
    """
    모션 그래프 패널 (속도/가속도)
    """
    frame_changed = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 헤더
        header = QWidget()
        header.setFixedHeight(24)
        header.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-bottom: 1px solid {COLORS['border']};")
        h_layout = QHBoxLayout(header)
        h_layout.setContentsMargins(6, 0, 6, 0)
        
        lbl = QLabel("MOTION ANALYSIS")
        lbl.setStyleSheet(f"color: {COLORS['accent']}; font-size: 10px; font-weight: bold; letter-spacing: 1px;")
        h_layout.addWidget(lbl)
        h_layout.addStretch()
        
        layout.addWidget(header)

        # 캔버스
        self.canvas = GraphCanvas()
        self.canvas.frame_clicked.connect(self.frame_changed.emit)
        layout.addWidget(self.canvas)

    def set_action(self, action: Optional[ActionData]):
        self.canvas.set_action(action)

    def set_current_frame(self, frame: int):
        self.canvas.set_current_frame(frame)
