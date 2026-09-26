"""
Sound Lite Phase 1 — ActionPlaybackCursor 유닛테스트.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 1.

Qt도 오디오도 엔진도 필요 없다 - 이 Phase가 발화 규칙 전체를 닫는다. Phase 3에서
소리가 이상하면 원인이 "연결" 쪽임을 알고 시작할 수 있게 하는 것이 목적이다.

기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행).
    python test_action_playback.py
"""

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.action_data import ActionData, ActionEvent, EventType
from core.action_playback import ActionPlaybackCursor, events_of_type


# ── 헬퍼 ─────────────────────────────────────────────────────────────────────

def make_action(total_frames: int, event_frames, total_name: str = "Test") -> ActionData:
    """event_frames의 각 프레임에 Sound 이벤트 하나씩 있는 액션."""
    return ActionData(
        name=total_name,
        fps=30,
        total_frames=total_frames,
        sections=[],
        events=[
            ActionEvent(frame=f, type=EventType.SOUND, params={"clip": f"c{f}.wav"})
            for f in event_frames
        ],
        layers=[],
    )


def frames_of(events) -> list:
    return [e.frame for e in events]


_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


# ── 테스트 ───────────────────────────────────────────────────────────────────

def test_advance_fires_only_passed_frames():
    """advance(1)은 그 프레임의 이벤트만 돌려준다."""
    c = ActionPlaybackCursor(make_action(10, [3]))

    for _ in range(3):
        fired = c.advance(1)
    check(frames_of(fired) == [3], f"프레임 3에서 발화해야 함, got {frames_of(fired)}")
    check(c.frame == 3, f"frame이 3이어야 함, got {c.frame}")

    fired = c.advance(1)
    check(fired == [], f"프레임 4에는 이벤트가 없어야 함, got {frames_of(fired)}")


def test_advance_does_not_skip_events_between_frames():
    """여러 프레임을 한 번에 건너뛰어도 사이에 낀 이벤트를 놓치지 않는다.

    `e.frame == 새 프레임`만 보는 구현이면 여기서 2, 3이 조용히 누락된다 -
    느린 프레임이나 높은 액션 fps에서 실제로 일어나는 상황이다.
    """
    c = ActionPlaybackCursor(make_action(10, [1, 2, 3]))
    fired = c.advance(5)
    check(frames_of(fired) == [1, 2, 3], f"1,2,3 전부 발화해야 함, got {frames_of(fired)}")


def test_seek_never_fires():
    """seek()은 어떤 경우에도 이벤트를 돌려주지 않는다(범위 정의서 §5.4).

    스크럽 중 소리가 나지 않는다는 규칙을 자료구조 수준에서 강제하는지 확인한다 -
    seek은 애초에 반환값이 없어야 한다.
    """
    c = ActionPlaybackCursor(make_action(10, [1, 2, 3, 4, 5]))
    result = c.seek(5)
    check(result is None, "seek()은 아무것도 돌려주지 않아야 함")
    check(c.frame == 5, f"seek 후 frame이 5여야 함, got {c.frame}")

    # seek로 건너뛴 구간은 그 뒤 advance에서도 다시 발화하지 않아야 한다.
    fired = c.advance(1)
    check(frames_of(fired) == [], f"seek로 지나온 이벤트가 재발화하면 안 됨, got {frames_of(fired)}")


def test_loop_boundary_does_not_double_fire():
    """루프에서 각 이벤트가 **한 바퀴에 정확히 한 번씩만** 발화한다(범위 정의서 §5.5).

    한 바퀴는 total + 1 프레임이다(0..total). 되감기가 한 틱을 소비하고 프레임 0도
    실제로 방문하는데, 이는 AnimationPreviewPanel._on_tick()의 기존 동작
    (`next > total이면 0`)과 같다 - 커서가 그 프레임 값의 원천이 되므로 맞춰야 한다.

    경계 이벤트(프레임 0과 total)는 루프에서 특히 겹치기 쉬운 자리라 여기서 고정한다.
    """
    LAPS = 3
    c = ActionPlaybackCursor(make_action(5, [0, 5]), loop=True)

    counts = {0: 0, 5: 0}
    for _ in range((5 + 1) * LAPS):       # 한 바퀴 = total+1 틱, 정확히 LAPS바퀴
        for e in c.advance(1):
            counts[e.frame] += 1

    # 핵심: 바퀴당 정확히 1번. 되감기 경계에서 2번씩 나면 여기서 2*LAPS가 된다.
    check(counts[5] == LAPS, f"프레임 5 이벤트는 바퀴당 1번, got {counts[5]}")
    check(counts[0] == LAPS, f"프레임 0 이벤트도 바퀴당 1번, got {counts[0]}")


def test_advance_matches_editor_frame_sequence():
    """커서의 프레임 순서가 기존 _on_tick()과 정확히 같아야 한다.

    커서가 AnimationPreviewPanel의 _current_frame 원천이 되므로, 여기가 어긋나면
    재생 위치 표시가 기존과 달라진다.
    """
    total = 3
    c = ActionPlaybackCursor(make_action(total, []), loop=True)

    seen = []
    frame = 0
    for _ in range(9):
        c.advance(1)
        seen.append(c.frame)
        # 기존 _on_tick의 계산
        frame = frame + 1
        if frame > total:
            frame = 0
        check(c.frame == frame, f"프레임이 기존 동작과 달라짐: 커서 {c.frame} vs 기존 {frame}")

    check(seen == [1, 2, 3, 0, 1, 2, 3, 0, 1], f"프레임 순서: {seen}")


def test_no_loop_stops_at_end():
    """loop=False면 끝에서 멈추고 더 이상 발화하지 않는다."""
    c = ActionPlaybackCursor(make_action(5, [2, 5]), loop=False)

    fired = c.advance(10)
    check(frames_of(fired) == [2, 5], f"2,5 발화 후 정지해야 함, got {frames_of(fired)}")
    check(c.finished, "finished가 True여야 함")
    check(c.frame == 5, f"끝 프레임에 멈춰야 함, got {c.frame}")

    fired = c.advance(5)
    check(fired == [], f"끝난 뒤에는 발화하지 않아야 함, got {frames_of(fired)}")


def test_many_laps_in_one_advance_does_not_flood():
    """한 번의 advance로 여러 바퀴를 돌아도 같은 이벤트를 바퀴 수만큼 쏟지 않는다.

    탭 전환 후 큰 dt가 들어오는 상황. 소리 수십 개가 한꺼번에 터지는 것보다
    한 번 나는 쪽이 낫다.
    """
    c = ActionPlaybackCursor(make_action(4, [2]), loop=True)
    fired = c.advance(40)   # 10바퀴
    check(len(fired) <= 2, f"바퀴 수만큼 쏟아지면 안 됨, got {len(fired)}개")


def test_same_frame_multiple_events():
    """같은 프레임에 이벤트가 여러 개면 전부 돌려준다."""
    action = ActionData(
        name="Multi", fps=30, total_frames=10,
        sections=[],
        events=[
            ActionEvent(frame=3, type=EventType.SOUND, params={"clip": "a.wav"}),
            ActionEvent(frame=3, type=EventType.SOUND, params={"clip": "b.wav"}),
        ],
        layers=[],
    )
    c = ActionPlaybackCursor(action)
    fired = c.advance(3)
    check(len(fired) == 2, f"같은 프레임의 이벤트 2개 전부 발화해야 함, got {len(fired)}")


def test_empty_and_degenerate_actions():
    """이벤트가 없는 액션, total_frames=0에서 예외 없이 동작한다."""
    c = ActionPlaybackCursor(make_action(10, []))
    check(c.advance(20) == [], "이벤트 없는 액션은 빈 리스트")

    c0 = ActionPlaybackCursor(make_action(0, [0]))
    check(c0.advance(5) == [], "total_frames=0이면 빈 리스트")

    c1 = ActionPlaybackCursor(make_action(10, [3]))
    check(c1.advance(0) == [], "advance(0)은 빈 리스트")
    check(c1.advance(-1) == [], "advance(음수)는 빈 리스트")


def test_events_of_type_filters():
    """커서는 EventType을 모르고, 필터는 호출부가 한다."""
    action = ActionData(
        name="Mixed", fps=30, total_frames=10,
        sections=[],
        events=[
            ActionEvent(frame=1, type=EventType.SOUND, params={}),
            ActionEvent(frame=2, type=EventType.HIT, params={}),
            ActionEvent(frame=3, type=EventType.SOUND, params={}),
        ],
        layers=[],
    )
    c = ActionPlaybackCursor(action)
    fired = c.advance(5)
    check(len(fired) == 3, f"커서는 타입을 거르지 않아야 함, got {len(fired)}")

    sounds = events_of_type(fired, EventType.SOUND.value)
    check(frames_of(sounds) == [1, 3], f"Sound만 걸러야 함, got {frames_of(sounds)}")


def test_reset_returns_to_start():
    c = ActionPlaybackCursor(make_action(10, [2]), loop=False)
    c.advance(10)
    check(c.finished, "먼저 끝나 있어야 함")
    c.reset()
    check(c.frame == 0 and not c.finished, "reset 후 처음 상태여야 함")
    check(frames_of(c.advance(3)) == [2], "reset 후 다시 발화해야 함")


# ── 러너 ─────────────────────────────────────────────────────────────────────

TESTS = [
    test_advance_fires_only_passed_frames,
    test_advance_does_not_skip_events_between_frames,
    test_seek_never_fires,
    test_loop_boundary_does_not_double_fire,
    test_advance_matches_editor_frame_sequence,
    test_no_loop_stops_at_end,
    test_many_laps_in_one_advance_does_not_flood,
    test_same_frame_multiple_events,
    test_empty_and_degenerate_actions,
    test_events_of_type_filters,
    test_reset_returns_to_start,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Sound Lite Phase 1 - ActionPlaybackCursor")
    print("=" * 60)

    for t in TESTS:
        before = len(_failures)
        print(f"\n[{t.__name__}]")
        try:
            t()
        except Exception as e:
            _failures.append(f"{t.__name__} 예외: {e}")
            print(f"  ERROR: {e}")
        if len(_failures) == before:
            print("  OK")

    print("\n" + "=" * 60)
    if _failures:
        print(f"FAILED: {len(_failures)}건")
        for f in _failures:
            print(f"  - {f}")
        sys.exit(1)
    print(f"PASSED: {len(TESTS)} tests")
