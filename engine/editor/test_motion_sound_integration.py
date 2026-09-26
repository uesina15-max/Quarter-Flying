"""
Sound Lite Phase 3 — Motion Editor 연결 검증.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 3.

Phase 3의 관문 네 가지 중 **가청("실제로 소리가 들리는가")을 뺀 세 가지**를 여기서
자동으로 확인한다. 가청은 프로그램으로 확인할 수 없으므로 사람이 에디터를 띄워
들어봐야 한다(구현 계획서 §6의 노트 참고).

  (1) 재생 중 해당 프레임에서 소리가 난다      -> 재생 요청 횟수/시점으로 확인(가청은 별도)
  (2) 타임라인을 드래그하면 소리가 나지 않는다  -> 여기서 확인
  (3) 루프해도 경계에서 두 번 나지 않는다        -> 여기서 확인
  (4) 클립이 없어도 재생이 멈추지 않는다         -> 여기서 확인

    python test_motion_sound_integration.py
"""

import math
import os
import struct
import sys
import tempfile
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide6.QtWidgets import QApplication

from core.action_data import ActionData, ActionEvent, EventType
from panels.animation_preview import AnimationPreviewPanel

_app = QApplication.instance() or QApplication([])
_tmpdir = tempfile.mkdtemp(prefix="soundlite_p3_")
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


def make_wav(name: str, seconds: float = 0.1) -> str:
    path = os.path.join(_tmpdir, name)
    rate = 44100
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b"".join(
            struct.pack("<h", int(0.2 * 32767 * math.sin(2 * math.pi * 440 * i / rate)))
            for i in range(int(rate * seconds))
        ))
    return path


class PlaySpy:
    """SoundPlayer.play를 가로채 호출을 기록한다. 실제 재생은 하지 않는다."""

    def __init__(self, panel):
        self.calls = []
        self._panel = panel
        self._orig = panel._sound.play
        panel._sound.play = self._record

    def _record(self, clip, volume=1.0, volume_var=0.0):
        self.calls.append((clip, volume, volume_var))
        return True

    def restore(self):
        self._panel._sound.play = self._orig


def make_panel(action, base_dir=None):
    panel = AnimationPreviewPanel()
    panel.set_action(action, base_dir)
    return panel


def sound_action(total_frames, event_frames, clip):
    return ActionData(
        name="SfxTest", fps=30, total_frames=total_frames, sections=[],
        events=[
            ActionEvent(frame=f, type=EventType.SOUND, params={"clip": clip, "volume": 0.01})
            for f in event_frames
        ],
        layers=[],
    )


# ── 테스트 ───────────────────────────────────────────────────────────────────

def test_playback_fires_sound_at_the_event_frame():
    """재생하면 이벤트가 있는 프레임에서 재생 요청이 나간다."""
    wav = make_wav("hit.wav")
    panel = make_panel(sound_action(10, [3], wav))
    spy = PlaySpy(panel)

    for _ in range(2):
        panel._on_tick()
    check(spy.calls == [], f"프레임 1,2에서는 재생 요청이 없어야 함: {spy.calls}")

    panel._on_tick()          # -> 프레임 3
    check(len(spy.calls) == 1, f"프레임 3에서 1번 재생 요청: {spy.calls}")
    check(spy.calls[0][0] == wav, f"클립 경로가 전달돼야 함: {spy.calls}")
    check(panel._current_frame == 3, f"프레임이 3이어야 함: {panel._current_frame}")

    spy.restore()


def test_scrubbing_does_not_make_sound():
    """(2) 타임라인 드래그/외부 프레임 동기화로는 소리가 나지 않는다(§5.4)."""
    wav = make_wav("scrub.wav")
    panel = make_panel(sound_action(10, [1, 2, 3, 4, 5], wav))
    spy = PlaySpy(panel)

    # 타임라인/그래프가 frame_changed로 부르는 경로가 이것이다.
    for f in [5, 2, 8, 0, 5, 3]:
        panel.set_current_frame(f)

    check(spy.calls == [], f"스크럽으로는 절대 소리가 나면 안 됨: {spy.calls}")
    check(panel._current_frame == 3, f"프레임은 정상 반영: {panel._current_frame}")

    spy.restore()


def test_prev_next_buttons_do_not_make_sound():
    """이전/다음 프레임 버튼도 재생 루프가 아니므로 소리를 내지 않는다."""
    wav = make_wav("step.wav")
    panel = make_panel(sound_action(10, [1, 2, 3], wav))
    spy = PlaySpy(panel)

    for _ in range(5):
        panel._on_next()
    for _ in range(3):
        panel._on_prev()

    check(spy.calls == [], f"프레임 이동 버튼으로는 소리가 나면 안 됨: {spy.calls}")

    spy.restore()


def test_loop_does_not_double_fire_at_boundary():
    """(3) 루프 경계에서 같은 이벤트가 두 번 나지 않는다(§5.5)."""
    wav = make_wav("loop.wav")
    total, laps = 5, 3
    panel = make_panel(sound_action(total, [0, total], wav))
    spy = PlaySpy(panel)

    for _ in range((total + 1) * laps):
        panel._on_tick()

    # 이벤트 2개 x 3바퀴 = 6번. 경계에서 겹쳐 나면 이 값이 커진다.
    check(len(spy.calls) == 2 * laps,
          f"이벤트 2개 x {laps}바퀴 = {2 * laps}번이어야 함, got {len(spy.calls)}")

    spy.restore()


def test_missing_clip_does_not_stop_playback():
    """(4) 클립이 없어도 재생이 멈추지 않는다(§5.6)."""
    good = make_wav("good.wav")
    missing = os.path.join(_tmpdir, "없는파일.wav")

    action = ActionData(
        name="Mixed", fps=30, total_frames=10, sections=[],
        events=[
            ActionEvent(frame=2, type=EventType.SOUND, params={"clip": missing}),
            ActionEvent(frame=4, type=EventType.SOUND, params={"clip": good, "volume": 0.01}),
            ActionEvent(frame=6, type=EventType.SOUND, params={}),          # clip 없음
            ActionEvent(frame=8, type=EventType.HIT,   params={"clip": good}),  # Sound 아님
        ],
        layers=[],
    )
    panel = make_panel(action)

    # 여기서는 실제 SoundPlayer를 그대로 쓴다 - 없는 클립이 예외를 던지지 않는지가 관심사다.
    for _ in range(10):
        panel._on_tick()

    check(panel._current_frame == 10,
          f"클립 실패에도 재생이 끝까지 진행돼야 함: {panel._current_frame}")


def test_relative_clip_path_is_resolved_against_base_dir():
    """§6.4 - params.clip은 .action.json 위치 기준 상대 경로다."""
    sub = os.path.join(_tmpdir, "sfx")
    os.makedirs(sub, exist_ok=True)
    wav = make_wav(os.path.join("sfx", "rel.wav"))

    action = sound_action(5, [1], os.path.join("sfx", "rel.wav"))
    panel = make_panel(action, base_dir=_tmpdir)

    check(panel._sound.is_loaded(os.path.normpath(wav)),
          "base_dir 기준으로 상대 경로가 풀려 로드돼야 함")


def test_changing_action_clears_previous_sounds():
    """액션을 바꾸면 이전 액션의 소리를 끌고 가지 않는다."""
    a_wav = make_wav("a.wav")
    b_wav = make_wav("b.wav")

    panel = make_panel(sound_action(5, [1], a_wav))
    check(panel._sound.is_loaded(a_wav), "첫 액션 클립이 로드됨")

    panel.set_action(sound_action(5, [1], b_wav))
    check(panel._sound.is_loaded(a_wav) is False, "이전 액션 클립은 정리돼야 함")
    check(panel._sound.is_loaded(b_wav), "새 액션 클립이 로드됨")
    check(panel._current_frame == 0, "액션 교체 시 프레임 0")


TESTS = [
    test_playback_fires_sound_at_the_event_frame,
    test_scrubbing_does_not_make_sound,
    test_prev_next_buttons_do_not_make_sound,
    test_loop_does_not_double_fire_at_boundary,
    test_missing_clip_does_not_stop_playback,
    test_relative_clip_path_is_resolved_against_base_dir,
    test_changing_action_clears_previous_sounds,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Sound Lite Phase 3 - Motion Editor 연결")
    print("=" * 60)

    for t in TESTS:
        before = len(_failures)
        print(f"\n[{t.__name__}]")
        try:
            t()
        except Exception as e:
            _failures.append(f"{t.__name__} 예외: {e}")
            print(f"  ERROR: {type(e).__name__}: {e}")
        if len(_failures) == before:
            print("  OK")

    print("\n" + "=" * 60)
    if _failures:
        print(f"FAILED: {len(_failures)}건")
        for f in _failures:
            print(f"  - {f}")
        sys.exit(1)
    print(f"PASSED: {len(TESTS)} tests")
    print("\n주의: '실제로 소리가 들리는가'는 여기서 확인할 수 없다.")
    print("      에디터를 띄워 사람이 직접 들어봐야 한다.")
