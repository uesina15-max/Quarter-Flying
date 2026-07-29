"""
editor/panels/animation_preview.py

Animation Preview Panel — 모션 프리뷰 (현재는 더미 스켈레톤).

역할:
  - 현재 ActionData의 진행도를 애니메이션으로 표시
  - 재생/정지 및 프레임 이동 컨트롤
  - 프레임 동기화 (타임라인/그래프와 연동)
"""

from __future__ import annotations

import math
from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QSizePolicy, QFrame
)
from PySide6.QtCore import Qt, Signal, QTimer, QRect, QPointF
from PySide6.QtGui import QPainter, QColor, QPen, QBrush

from core.action_data import ActionData


class PreviewCanvas(QWidget):
    """
    더미 스켈레톤을 그리는 캔버스.
    실제 엔진 뷰포트나 FBX 렌더러가 붙기 전까지 시각적 피드백 제공.
    """

    def __init__(self, parent=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._current_frame = 0
        self.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Expanding)

    def set_action(self, action: Optional[ActionData]):
        self._action = action
        self._current_frame = 0
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
        p.fillRect(0, 0, w, h, QColor("#111827"))

        # 그리드 바닥
        p.setPen(QPen(QColor("#1e3a5f"), 1, Qt.DashLine))
        ground_y = int(h * 0.8)
        p.drawLine(0, ground_y, w, ground_y)

        if not self._action or self._action.total_frames <= 0:
            p.setPen(QColor("#556677"))
            p.drawText(self.rect(), Qt.AlignCenter, "No Action Selected")
            return

        # 더미 스켈레톤 그리기 (진동/움직임 효과)
        # 프레임에 따라 약간씩 자세가 바뀌는 느낌을 줌.
        t = self._current_frame / self._action.total_frames
        
        # 기본 위치 (화면 중앙)
        cx = w / 2
        
        # 공격 섹션일 경우 좀 더 역동적으로
        is_attack = False
        speed_mult = 1.0
        for sec in self._action.sections:
            if sec.start_frame <= self._current_frame <= sec.end_frame:
                if sec.role.value == "Attack":
                    is_attack = True
                speed_mult = sec.speed
                break

        # 간단한 수학 함수로 포즈 계산
        time_angle = t * math.pi * 2 * speed_mult
        
        # 뼈대 위치 계산
        head_y = ground_y - 120 + math.sin(time_angle * 2) * 5
        neck_y = head_y + 20
        pelvis_y = neck_y + 50 + math.sin(time_angle * 2 + 1) * 5
        
        # 공격 모션 시 팔을 크게 휘두름
        if is_attack:
            l_hand_x = cx - 40 - math.cos(time_angle * 5) * 30
            l_hand_y = neck_y + 10 - math.sin(time_angle * 5) * 40
            r_hand_x = cx + 40 + math.cos(time_angle * 4) * 40
            r_hand_y = neck_y - 30 - math.sin(time_angle * 4) * 50
        else:
            l_hand_x = cx - 30 - math.cos(time_angle) * 10
            l_hand_y = neck_y + 30 + math.sin(time_angle) * 10
            r_hand_x = cx + 30 + math.cos(time_angle + 3.14) * 10
            r_hand_y = neck_y + 30 + math.sin(time_angle + 3.14) * 10

        # 다리
        l_foot_x = cx - 20 - math.sin(time_angle * 2) * 15
        l_foot_y = ground_y
        r_foot_x = cx + 20 + math.sin(time_angle * 2) * 15
        r_foot_y = ground_y

        # 그리기 ─────────────────
        p.setPen(QPen(QColor("#00d4ff"), 3, Qt.SolidLine, Qt.RoundCap, Qt.RoundJoin))

        # 척추
        p.drawLine(QPointF(cx, neck_y), QPointF(cx, pelvis_y))
        
        # 왼팔
        p.drawLine(QPointF(cx, neck_y), QPointF(l_hand_x, l_hand_y))
        
        # 오른팔 (무기 드는 팔)
        p.drawLine(QPointF(cx, neck_y), QPointF(r_hand_x, r_hand_y))
        
        # 왼다리
        p.drawLine(QPointF(cx, pelvis_y), QPointF(l_foot_x, l_foot_y))
        
        # 오른다리
        p.drawLine(QPointF(cx, pelvis_y), QPointF(r_foot_x, r_foot_y))

        # 머리
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(QColor("#e94560")))
        p.drawEllipse(QPointF(cx, head_y), 12, 12)

        # 무기 (공격 모드일 때만 검 강조)
        if is_attack:
            p.setPen(QPen(QColor("#ff4444"), 4))
            p.drawLine(QPointF(r_hand_x, r_hand_y), QPointF(r_hand_x + 30, r_hand_y - 40))

        # 프레임 오버레이 텍스트
        p.setPen(QColor("#ffffff"))
        p.drawText(10, 20, f"Frame: {self._current_frame} / {self._action.total_frames}")
        
        if is_attack:
            p.setPen(QColor("#ff4444"))
            p.drawText(10, 40, "ATTACK PHASE")


class AnimationPreviewPanel(QWidget):
    """
    중앙 패널: 애니메이션 프리뷰 뷰어 + 컨트롤
    """
    frame_changed = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._current_frame = 0
        self._is_playing = False
        
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._on_tick)
        self._timer.setInterval(1000 // 30) # 기본 30fps
        
        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        # 캔버스 (뷰어)
        self.canvas = PreviewCanvas()
        layout.addWidget(self.canvas)

        # 컨트롤 바
        ctrl = QWidget()
        ctrl.setFixedHeight(40)
        ctrl.setStyleSheet("background-color: #0d1b2a; border-top: 1px solid #1e3a5f;")
        c_layout = QHBoxLayout(ctrl)
        c_layout.setContentsMargins(10, 5, 10, 5)

        self.btn_prev = QPushButton("◀")
        self.btn_prev.setFixedSize(30, 24)
        self.btn_prev.clicked.connect(self._on_prev)

        self.btn_play = QPushButton("▶")
        self.btn_play.setFixedSize(40, 24)
        self.btn_play.setObjectName("btn_create")
        self.btn_play.clicked.connect(self._on_play_toggle)

        self.btn_next = QPushButton("▶")
        self.btn_next.setFixedSize(30, 24)
        self.btn_next.clicked.connect(self._on_next)
        
        self.lbl_frame = QLabel("0 / 0")
        self.lbl_frame.setStyleSheet("color: #8899aa; font-size: 12px; margin-left: 10px;")

        c_layout.addStretch()
        c_layout.addWidget(self.btn_prev)
        c_layout.addWidget(self.btn_play)
        c_layout.addWidget(self.btn_next)
        c_layout.addWidget(self.lbl_frame)
        c_layout.addStretch()

        layout.addWidget(ctrl)

    def set_action(self, action: Optional[ActionData]):
        self._action = action
        self.canvas.set_action(action)
        self.set_current_frame(0)
        if action:
            self._timer.setInterval(1000 // (action.fps if action.fps > 0 else 30))

    def set_current_frame(self, frame: int):
        self._current_frame = frame
        self.canvas.set_current_frame(frame)
        
        total = self._action.total_frames if self._action else 0
        self.lbl_frame.setText(f"{frame} / {total}")

    def _on_play_toggle(self):
        if not self._action:
            return
            
        self._is_playing = not self._is_playing
        if self._is_playing:
            self.btn_play.setText("⏸")
            self._timer.start()
        else:
            self.btn_play.setText("▶")
            self._timer.stop()

    def _on_tick(self):
        if not self._action:
            return
            
        next_frame = self._current_frame + 1
        if next_frame > self._action.total_frames:
            next_frame = 0
            
        self.set_current_frame(next_frame)
        self.frame_changed.emit(next_frame)

    def _on_prev(self):
        if not self._action: return
        f = max(0, self._current_frame - 1)
        self.set_current_frame(f)
        self.frame_changed.emit(f)

    def _on_next(self):
        if not self._action: return
        f = min(self._action.total_frames, self._current_frame + 1)
        self.set_current_frame(f)
        self.frame_changed.emit(f)
