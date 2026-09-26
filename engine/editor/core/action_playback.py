"""
editor/core/action_playback.py

액션 재생 위치를 들고, 프레임이 전진할 때 "이번에 발화할 이벤트"를 돌려주는 커서.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.1.

이 모듈은 **Qt도, 오디오도, 엔진도 모른다.** 그래서 유닛테스트로 전부 닫힌다
(test_action_playback.py). 그렇게 만든 이유는 범위 정의서 §6.2가 (b)로 확정되면서
재생 루프가 둘이 되기 때문이다:

    Motion Editor  : AnimationPreviewPanel._on_tick()
    Scene Editor   : MainWindow._on_fps_tick()  (Play 중)

발화 "지점"이 둘이 되는 것은 어쩔 수 없지만 발화 "규칙"까지 둘이 되면, 한쪽만
고쳐지는 버그가 반드시 생긴다. 두 루프가 이 커서 하나를 공유한다.

핵심 설계 하나: **advance()만 이벤트를 돌려주고 seek()은 절대 돌려주지 않는다.**
타임라인을 드래그하면 프레임이 초당 수십 번 바뀌는데 그때마다 소리가 나면 쓸 수
없다(범위 정의서 §5.4). 이걸 호출부의 주의사항으로 두지 않고 **자료구조 수준에서**
강제하기 위해 두 메서드의 반환 타입을 아예 다르게 뒀다.
"""

from __future__ import annotations

from typing import List, Optional

from core.action_data import ActionData, ActionEvent


class ActionPlaybackCursor:
    """액션 하나의 재생 위치.

    프레임은 0 이상 total_frames 이하의 정수다(양 끝 포함) — AnimationPreviewPanel의
    기존 규약과 같다(`next_frame > total_frames`면 0으로 되감음).
    """

    def __init__(self, action: ActionData, loop: bool = True):
        self._action = action
        self._loop = loop
        self._frame = 0
        self._finished = False

    # ── 조회 ─────────────────────────────────────────────────────────────────

    @property
    def frame(self) -> int:
        return self._frame

    @property
    def finished(self) -> bool:
        """loop=False인 액션이 끝까지 갔는가. loop=True면 항상 False."""
        return self._finished

    @property
    def action(self) -> ActionData:
        return self._action

    # ── 상태 변경 ────────────────────────────────────────────────────────────

    def reset(self) -> None:
        self._frame = 0
        self._finished = False

    def seek(self, frame: int) -> None:
        """재생 위치를 옮긴다. **이벤트를 발화시키지 않는다.**

        반환값이 없는 것이 의도다 - 스크럽/이전·다음 버튼/타임라인 클릭은 전부
        이 경로를 쓰고, 그 경로에서는 소리가 나지 않아야 한다(범위 정의서 §5.4).
        """
        self._frame = self._clamp(frame)
        self._finished = False

    def advance(self, frames: int = 1) -> List[ActionEvent]:
        """프레임을 전진시키고, 그 사이에 지나간 이벤트를 순서대로 돌려준다.

        발화 조건은 `이전 프레임 < e.frame <= 새 프레임`이다. 프레임을 여러 개
        건너뛰어도(느린 프레임, 높은 액션 fps) 사이에 낀 이벤트를 놓치지 않는다 -
        `e.frame == 새 프레임`만 보면 조용히 누락된다.
        """
        if frames <= 0 or self._finished:
            return []

        total = self._total_frames()
        if total <= 0:
            return []

        # 한 바퀴는 total + 1 프레임이다(0..total, 양 끝 포함). 되감기 자체가 한 틱을
        # 소비하고 프레임 0도 실제로 표시된다 - AnimationPreviewPanel._on_tick()의
        # 기존 동작(`next > total이면 0`)이 그렇고, 커서가 그 프레임 값의 원천이 되므로
        # 반드시 같아야 한다.
        lap = total + 1
        if self._loop and frames > lap:
            # 한 번의 advance로 여러 바퀴를 도는 극단적인 경우(예: 탭 전환 후 큰 dt).
            # 한 바퀴로 잘라내면 모든 이벤트가 정확히 한 번씩만 발화한다 - 소리 수십
            # 개가 한꺼번에 터지는 것보다 한 번 나는 쪽이 낫다.
            frames = lap

        fired: List[ActionEvent] = []

        # "새로 도달한 프레임이 자기 이벤트를 발화시킨다." 시작 프레임은 이미 서 있던
        # 자리이므로 발화시키지 않는다. 이 규칙 하나로 §5.5(루프 경계 중복 발화)가
        # 자동으로 지켜진다 - 한 바퀴에 각 프레임을 정확히 한 번씩만 방문하기 때문이다.
        for _ in range(frames):
            nxt = self._frame + 1
            if nxt > total:
                if not self._loop:
                    self._finished = True
                    break
                nxt = 0
            self._frame = nxt
            fired.extend(self._events_at(nxt))

        # loop=False로 끝 프레임에 "정확히 딱 맞게" 도달한 경우. 위 break 분기만으로는
        # 빠뜨린다(다음 전진을 시도해야 break에 걸리기 때문).
        if not self._loop and self._frame >= total:
            self._finished = True

        return fired

    # ── 내부 ─────────────────────────────────────────────────────────────────

    def _total_frames(self) -> int:
        return max(0, int(self._action.total_frames))

    def _clamp(self, frame: int) -> int:
        return max(0, min(int(frame), self._total_frames()))

    def _events_at(self, frame: int) -> List[ActionEvent]:
        """그 프레임에 있는 이벤트들. 같은 프레임에 여러 개면 정의된 순서대로."""
        return [e for e in self._action.events if e.frame == frame]


def events_of_type(events: List[ActionEvent], type_value: str) -> List[ActionEvent]:
    """발화된 이벤트 중 특정 EventType만 고른다.

    커서 자체는 EventType을 모른다 - v1이 Sound만 소비한다는 결정(범위 정의서 §3)은
    호출부의 것이지 커서의 것이 아니다. 나중에 Hit/CameraShake를 발화시키게 되어도
    커서는 그대로 둔다.
    """
    return [e for e in events if e.type.value == type_value]
