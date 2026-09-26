"""
editor/panels/motion_graph.py

Motion Graph Panel — Speed & Acceleration Curve 표시기.

역할:
  - 선택된 ActionData의 섹션별 속도(speed)를 기반으로 커브를 그림.
  - 실제 FBX가 연동되면 본(Bone)의 실제 속도 데이터를 시각화할 위치.
  - 현재는 각 섹션의 배율(speed)을 기반으로 보간된 그래프를 표시.
"""

from __future__ import annotations

import math
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
        
        # 커브 데이터 생성 (더미 곡선)
        # 1. 속도 곡선 (노란색)
        # 2. 가속도 곡선 (주황색)
        
        points_speed = []
        points_accel = []
        
        max_speed = 3.0 # 최대 3배속으로 가정하여 Y축 스케일링
        
        for frame in range(total_frames + 1):
            x = (frame / total_frames) * w
            
            # 현재 프레임이 속한 섹션 찾기
            current_speed = 1.0
            for sec in self._action.sections:
                if sec.start_frame <= frame <= sec.end_frame:
                    # 섹션 중간부분에서 최고 속도가 되도록 곡선 깎기
                    sec_len = max(1, sec.end_frame - sec.start_frame)
                    t = (frame - sec.start_frame) / sec_len
                    # sin 함수로 부드러운 곡선 만들기
                    curve = math.sin(t * math.pi) 
                    # 기본 0.5에서 시작해 최고점에서 sec.speed에 도달하도록
                    current_speed = 0.5 + (sec.speed - 0.5) * curve
                    break
                    
            # 화면 Y 좌표로 변환 (아래쪽이 0)
            normalized_speed = min(1.0, current_speed / max_speed)
            y_speed = h - (normalized_speed * h * 0.8) # 상단 여백 20%
            points_speed.append(QPointF(x, y_speed))
            
            # 가속도 (속도의 변화량, 대충 미분)
            if frame > 0:
                prev_y = points_speed[-2].y()
                # 변화량이 클수록 가속도가 큼. 중간 지점(h/2)을 0으로 잡음
                accel = (prev_y - y_speed) * 2 
                y_accel = (h / 2) - accel
                y_accel = max(10, min(h - 10, y_accel))
                points_accel.append(QPointF(x, y_accel))
            else:
                points_accel.append(QPointF(x, h/2))

        # 가속도 곡선 그리기
        if len(points_accel) > 1:
            path_accel = QPainterPath()
            path_accel.moveTo(points_accel[0])
            for pt in points_accel[1:]:
                path_accel.lineTo(pt)
            p.setPen(QPen(QColor(255, 120, 0, 150), 1.5))
            p.drawPath(path_accel)

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

        # 현재 재생 헤드
        px = int((self._current_frame / total_frames) * w)
        p.setPen(QPen(QColor("#ffffff"), 1))
        p.drawLine(px, 0, px, h)
        
        # 범례 표시
        p.setPen(QColor(0, 212, 255, 255))
        p.drawText(10, 20, "── Speed")
        p.setPen(QColor(255, 120, 0, 255))
        p.drawText(10, 35, "── Accel")
        
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
