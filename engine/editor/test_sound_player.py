"""
Sound Lite Phase 2 — SoundPlayer 검증.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 2.

이 Phase는 액션과 무관하게 단독으로 검증된다 - Phase 1(발화 규칙)과 Phase 2(소리 내기)를
각각 따로 닫아두면, Phase 3에서 소리가 이상할 때 원인이 "연결" 쪽임을 알고 시작할 수 있다.

테스트용 WAV는 stdlib `wave`로 즉석 생성한다(외부 에셋에 의존하지 않는다).
구현 계획서 §6의 남은 실측 항목도 여기서 확인한다:
  1. QSoundEffect가 실제로 어떤 포맷을 받는가 (WAV만인가)
  3. 동시 재생 시 인스턴스를 어떻게 다뤄야 하는가

    python test_sound_player.py
"""

import math
import os
import struct
import sys
import tempfile
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide6.QtWidgets import QApplication
from PySide6.QtMultimedia import QSoundEffect

from core.sound_player import SoundPlayer, collect_clip_paths
from core.action_data import ActionData, ActionEvent, EventType

_app = QApplication.instance() or QApplication([])
_tmpdir = tempfile.mkdtemp(prefix="soundlite_")
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


def make_wav(name: str, seconds: float = 0.2, freq: float = 440.0) -> str:
    """짧은 사인파 WAV(16bit mono 44.1kHz)를 만든다."""
    path = os.path.join(_tmpdir, name)
    rate = 44100
    frames = int(rate * seconds)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        data = b"".join(
            struct.pack("<h", int(0.25 * 32767 * math.sin(2 * math.pi * freq * i / rate)))
            for i in range(frames)
        )
        w.writeframes(data)
    return path


def make_fake_ogg(name: str) -> str:
    """WAV가 아닌 파일. 확장자만 오디오인 쓰레기 데이터."""
    path = os.path.join(_tmpdir, name)
    with open(path, "wb") as f:
        f.write(b"OggS" + b"\x00" * 256)
    return path


# ── 테스트 ───────────────────────────────────────────────────────────────────

def test_preload_and_play_wav():
    wav = make_wav("beep.wav")
    p = SoundPlayer()
    results = p.preload([wav])

    check(results.get(wav) is True, f"WAV 프리로드가 성공해야 함: {results}")
    check(p.is_loaded(wav), "is_loaded가 True여야 함")
    check(p.play(wav, volume=0.05) is True, "재생이 True를 돌려줘야 함")


def test_missing_file_does_not_raise():
    """§5.6 - 없는 파일이 예외가 아니라 False가 되어야 한다."""
    p = SoundPlayer()
    missing = os.path.join(_tmpdir, "nope.wav")
    results = p.preload([missing])

    check(results.get(missing) is False, "없는 파일은 프리로드 실패로 보고돼야 함")
    check(p.play(missing) is False, "없는 파일 재생은 False")
    check(p.play("전혀_프리로드_안한_클립.wav") is False, "미프리로드 클립도 False")


def test_non_wav_format_is_rejected_not_crashed():
    """§6-1 실측: QSoundEffect가 WAV 아닌 것을 어떻게 다루는가."""
    fake = make_fake_ogg("fake.ogg")
    p = SoundPlayer()
    results = p.preload([fake])
    status = p._effects[fake].status()

    print(f"    (실측) WAV 아닌 파일의 status = {status}")
    check(results.get(fake) is False,
          f"WAV가 아니면 프리로드 실패로 보고돼야 함: {results}")
    check(p.play(fake) is False, "WAV가 아니면 재생은 False")


def test_voice_cap_refuses_new_without_cutting():
    """§5.2 - 상한 초과 시 신규 재생을 거부하고, 이미 나는 소리를 끊지 않는다."""
    clips = [make_wav(f"v{i}.wav", seconds=1.0, freq=220 + i * 40) for i in range(4)]
    p = SoundPlayer(max_voices=2)
    p.preload(clips)

    ok = [p.play(c, volume=0.01) for c in clips]
    print(f"    (실측) max_voices=2에서 4개 재생 시도 결과 = {ok}")

    check(ok[0] is True and ok[1] is True, f"상한까지는 재생돼야 함: {ok}")
    check(p.playing_count() <= 2, f"동시 재생이 상한을 넘으면 안 됨: {p.playing_count()}")
    # 상한을 넘긴 요청은 거부되어야 한다 - 이미 나는 소리를 끊고 자리를 만들지 않는다.
    check(False in ok[2:], f"상한 초과 요청은 거부돼야 함: {ok}")


def test_same_clip_retrigger_is_allowed_by_cap():
    """v1의 알려진 한계: 같은 클립 재트리거는 새 보이스가 아니라 재시작이다.

    따라서 상한 검사에서 제외되어야 한다 - 안 그러면 상한이 1일 때 같은 소리를
    두 번째부터 아예 못 낸다.
    """
    wav = make_wav("retrig.wav", seconds=1.0)
    p = SoundPlayer(max_voices=1)
    p.preload([wav])

    check(p.play(wav, volume=0.01) is True, "첫 재생")
    check(p.play(wav, volume=0.01) is True, "같은 클립 재트리거는 허용돼야 함(재시작)")


def test_volume_variance_stays_in_range():
    """Volume ±가 0..1을 벗어나지 않는다."""
    wav = make_wav("var.wav")
    p = SoundPlayer()
    p.preload([wav])

    for _ in range(50):
        v = p._resolve_volume(1.0, 0.5)
        if not (0.0 <= v <= 1.0):
            check(False, f"볼륨이 범위를 벗어남: {v}")
            return
    lo = p._resolve_volume(0.0, 1.0)
    check(0.0 <= lo <= 1.0, f"0 볼륨에 변동폭을 줘도 범위 안: {lo}")
    check(p._resolve_volume(0.5, 0.0) == 0.5, "변동폭 0이면 원본 그대로")


def test_stop_all_and_clear():
    wav = make_wav("stopme.wav", seconds=1.0)
    p = SoundPlayer()
    p.preload([wav])
    p.play(wav, volume=0.01)

    p.stop_all()
    check(p.playing_count() == 0, f"stop_all 후 재생 중이 없어야 함: {p.playing_count()}")

    p.clear()
    check(p.is_loaded(wav) is False, "clear 후에는 로드 상태가 아니어야 함")
    check(p.play(wav) is False, "clear 후 재생은 False")


def test_collect_clip_paths_resolves_relative():
    """§6.4 - params.clip은 .action.json 위치 기준 상대 경로다."""
    action = ActionData(
        name="A", fps=30, total_frames=10, sections=[],
        events=[
            ActionEvent(frame=1, type=EventType.SOUND, params={"clip": "sfx/a.wav"}),
            ActionEvent(frame=2, type=EventType.HIT, params={"clip": "무시돼야함.wav"}),
            ActionEvent(frame=3, type=EventType.SOUND, params={"clip": "sfx/a.wav"}),
            ActionEvent(frame=4, type=EventType.SOUND, params={}),
        ],
        layers=[],
    )

    paths = collect_clip_paths(action, base_dir=os.path.join("C:", os.sep, "proj"))
    check(len(paths) == 1, f"중복 제거 + Sound만 + clip 없는 것 제외 = 1개여야 함: {paths}")
    check(paths[0].endswith(os.path.join("sfx", "a.wav")), f"상대 경로가 풀려야 함: {paths}")
    check(os.path.isabs(paths[0]), f"base_dir 기준 절대 경로여야 함: {paths}")

    no_base = collect_clip_paths(action)
    check(no_base == ["sfx/a.wav"], f"base_dir 없으면 그대로: {no_base}")


TESTS = [
    test_preload_and_play_wav,
    test_missing_file_does_not_raise,
    test_non_wav_format_is_rejected_not_crashed,
    test_voice_cap_refuses_new_without_cutting,
    test_same_clip_retrigger_is_allowed_by_cap,
    test_volume_variance_stays_in_range,
    test_stop_all_and_clear,
    test_collect_clip_paths_resolves_relative,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Sound Lite Phase 2 - SoundPlayer")
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
