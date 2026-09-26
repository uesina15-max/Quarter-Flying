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
import os
from typing import Optional

from PySide6.QtWidgets import (
    QWidget, QVBoxLayout, QHBoxLayout, QLabel,
    QPushButton, QSizePolicy, QFrame
)
from PySide6.QtCore import Qt, Signal, QTimer, QRect, QPointF
from PySide6.QtGui import QPainter, QColor, QPen, QBrush

from core.action_data import ActionData, EventType
from core.action_playback import ActionPlaybackCursor, events_of_type
from core.sound_player import SoundPlayer, collect_clip_paths
from panels.motion_mixer import MotionMixerPanel
from style.theme import COLORS

from engine_binding import binding as ge_python, HAS_ENGINE


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

    def __init__(self, parent=None, shared_viewport=None):
        super().__init__(parent)
        self._action: Optional[ActionData] = None
        self._current_frame = 0
        self._is_playing = False

        # Motion Mixer 프리뷰(Phase 4A, 착수 계약서 §C11) - Scene Editor와 같은
        # EngineViewport 인스턴스를 그대로 받아쓴다. 이 패널은 이 뷰포트를 소유하지 않고
        # main.py가 모드 전환 시 붙였다 뗐다 한다(attach_viewport 참고).
        self._shared_viewport = shared_viewport

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._on_tick)
        self._timer.setInterval(1000 // 30) # 기본 30fps

        # Sound Lite Phase 3 (docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3).
        # 커서가 _current_frame의 원천이 되고, 재생 중에만 이벤트를 발화시킨다.
        # 스크럽/이전·다음 버튼은 커서의 seek()을 쓰는데 seek()은 반환값이 아예 없어서
        # 소리를 낼 수가 없다(범위 정의서 §5.4를 자료구조로 강제).
        self._cursor: Optional[ActionPlaybackCursor] = None
        self._sound = SoundPlayer()

        self._build_ui()

    def _build_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(0, 0, 0, 0)
        layout.setSpacing(0)

        if self._shared_viewport is not None:
            # 공유 뷰포트가 들어갈 빈 슬롯 - 실제 위젯은 attach_viewport()에서 붙는다
            # (엔진 뷰포트가 있는 경우에만, 없으면 else 분기의 2D 더미로 폴백).
            self.viewport_slot = QWidget()
            self._viewport_slot_layout = QVBoxLayout(self.viewport_slot)
            self._viewport_slot_layout.setContentsMargins(0, 0, 0, 0)
            self.canvas = None
            layout.addWidget(self.viewport_slot, 1)

            # Phase 5 믹서 UI - 실제 블렌딩(LayerMixer)은 전부 C++에 위임하고, 여기서는
            # weight/frame/mask 값 전달만 한다("UI blending 로직 재구현 금지").
            self.mixer_panel = MotionMixerPanel(self._get_preview_state)
            # 버튼 제거(로드/설정 통합, motion_mixer.py) + 여백 축소로 줄어든 만큼
            # 뷰포트가 더 넓은 영역을 쓰도록 높이를 낮춘다(모션 에디터 레이아웃을
            # 뷰포트/모션그래프 위주로 재조정).
            self.mixer_panel.setFixedHeight(205)
            layout.addWidget(self.mixer_panel)
        else:
            # 캔버스 (뷰어) - 엔진이 없을 때의 2D 더미 폴백
            self.canvas = PreviewCanvas()
            layout.addWidget(self.canvas)

        # 컨트롤 바
        ctrl = QWidget()
        ctrl.setFixedHeight(40)
        ctrl.setStyleSheet(f"background-color: {COLORS['bg_header']}; border-top: 1px solid {COLORS['border']};")
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
        self.lbl_frame.setStyleSheet(f"color: {COLORS['text_secondary']}; font-size: 12px; margin-left: 10px;")

        c_layout.addStretch()
        c_layout.addWidget(self.btn_prev)
        c_layout.addWidget(self.btn_play)
        c_layout.addWidget(self.btn_next)
        c_layout.addWidget(self.lbl_frame)
        c_layout.addStretch()

        layout.addWidget(ctrl)

    def set_action(self, action: Optional[ActionData], base_dir: Optional[str] = None):
        """액션을 붙인다.

        base_dir는 그 액션의 `.action.json`이 있는 폴더다 - params.clip이 그 기준
        상대 경로이기 때문이다(범위 정의서 §6.4). 넘기지 않으면 **액션이 들고 있는
        source_path에서 스스로 알아낸다** - 그래서 기존 호출부(`set_action(action)`)를
        고치지 않아도 상대 경로가 제대로 풀린다. 저장된 적 없는 액션은 source_path가
        None이고, 그 경우 clip은 절대 경로라 기준 폴더가 필요 없다.
        """
        if base_dir is None and action is not None and getattr(action, "source_path", None):
            base_dir = os.path.dirname(action.source_path)
        self._action = action
        if self.canvas is not None:
            self.canvas.set_action(action)

        # 액션이 바뀌면 이전 액션의 소리를 끌고 가지 않는다.
        self._sound.clear()
        self._cursor = ActionPlaybackCursor(action, loop=True) if action else None

        self.set_current_frame(0)
        if action:
            self._timer.setInterval(1000 // (action.fps if action.fps > 0 else 30))
            # §5.3 - 여기서 한 번만 로드한다. 재생 중에는 파일을 건드리지 않는다.
            clips = collect_clip_paths(action, base_dir)
            if clips:
                self._sound.preload(clips)

    def set_current_frame(self, frame: int):
        self._current_frame = frame
        if self.canvas is not None:
            self.canvas.set_current_frame(frame)

        # 스크럽/외부 동기화 경로. seek()은 이벤트를 돌려주지 않으므로 소리가 나지 않는다.
        if self._cursor is not None and self._cursor.frame != frame:
            self._cursor.seek(frame)

        total = self._action.total_frames if self._action else 0
        self.lbl_frame.setText(f"{frame} / {total}")

    # ── Motion Mixer 프리뷰 (Phase 4A, 착수 계약서 §C11) ────────────────────────
    # Motion Editor는 뷰포트를 소유하지 않는다 - 여기서는 공유 뷰포트를 붙였다 떼는 것과
    # Preview State(스켈레톤/클립/weight)를 채우는 것만 하고, OpenGL이나 블렌딩 연산에는
    # 손대지 않는다("Editor <-> OpenGL Context 직접 결합 금지", "UI blending 로직
    # 재구현 금지 - Python은 weight 전달까지만, Mixer는 C++").

    def _get_preview_state(self):
        """엔진이 초기화되어 있으면 MotionPreviewState를, 아니면 None을 돌려준다."""
        if self._shared_viewport is None or not getattr(self._shared_viewport, "initialized", False):
            return None
        if not HAS_ENGINE:
            return None
        return self._shared_viewport.engine.GetMotionPreviewState()

    def attach_viewport(self):
        """공유 뷰포트를 이 패널의 슬롯에 붙인다 (Motion Editor 활성화 시 main.py가 호출).
        Qt는 addWidget 시 위젯을 자동으로 재부모(reparent)하므로, 이전에 어디 붙어있었든
        (Scene Editor의 스플리터 등) 여기로 옮겨온다."""
        if self._shared_viewport is None:
            return
        self._viewport_slot_layout.addWidget(self._shared_viewport)

    def activate_motion_preview(self):
        """Motion 모드로 전환하고, 아직 아무것도 안 실려있으면 샘플을 자동 로드한다."""
        if not HAS_ENGINE or self._shared_viewport is None or not getattr(self._shared_viewport, "initialized", False):
            return
        self._shared_viewport.engine.SetRenderMode(ge_python.RenderMode.Motion)
        preview = self._get_preview_state()
        if preview is not None and not preview.HasSkeleton():
            self.mixer_panel.load_default_sample()

    def deactivate_motion_preview(self):
        """Scene 모드로 되돌린다 (Motion Editor를 벗어날 때 main.py가 호출)."""
        if not HAS_ENGINE or self._shared_viewport is None or not getattr(self._shared_viewport, "initialized", False):
            return
        self._shared_viewport.engine.SetRenderMode(ge_python.RenderMode.Scene)

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
            # 일시정지한 순간 울리던 소리가 계속 나면 "멈췄는데 소리가 난다"가 된다.
            self._sound.stop_all()

    def _on_tick(self):
        if not self._action:
            return

        if self._cursor is None:
            # 커서가 없을 때의 폴백 - 기존 계산 그대로.
            next_frame = self._current_frame + 1
            if next_frame > self._action.total_frames:
                next_frame = 0
            self.set_current_frame(next_frame)
            self.frame_changed.emit(next_frame)
            return

        # 재생 루프에서만 이벤트가 발화한다(범위 정의서 §4.2).
        fired = self._cursor.advance(1)
        next_frame = self._cursor.frame

        self.set_current_frame(next_frame)
        self.frame_changed.emit(next_frame)

        self._play_sounds(fired)

    def _play_sounds(self, fired_events):
        """발화된 이벤트 중 Sound만 재생한다.

        커서는 EventType을 모른다 - v1이 Sound만 소비한다는 결정(범위 정의서 §3)은
        호출부인 여기의 것이다. 재생 실패는 로그만 남기고 넘어간다(§5.6) - 사운드는
        곁다리이지 본체가 아니므로 액션 재생을 멈추면 안 된다.
        """
        if not fired_events:
            return
        for e in events_of_type(fired_events, EventType.SOUND.value):
            params = e.params or {}
            clip = params.get("clip")
            if not clip:
                continue
            self._sound.play(
                clip,
                volume=float(params.get("volume", 1.0)),
                volume_var=float(params.get("volumeVar", 0.0)),
            )

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
