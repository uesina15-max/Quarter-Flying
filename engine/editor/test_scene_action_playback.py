"""
Sound Lite Phase 6 — Scene Editor Play 재생 검증.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 6.

registry를 가짜 객체로 대신하므로 **엔진 없이** 돈다. Phase 6의 관문 중 가청을 뺀
전부를 여기서 확인한다 - 특히 §2.4가 경고한 것:

    _on_fps_tick은 60fps 고정인데 액션 fps는 다를 수 있다. 틱 수를 프레임 수로 쓰면
    30fps 액션이 두 배 빨리 재생된다.

    python test_scene_action_playback.py
"""

import json
import math
import os
import struct
import sys
import tempfile
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from core.action_data import ActionData, ActionEvent, EventType, save_action
from core.scene_action_playback import ScenePlaybackController

_tmpdir = tempfile.mkdtemp(prefix="soundlite_p6_")
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


# ── 가짜 registry / SoundPlayer ──────────────────────────────────────────────

class FakeEntity:
    def __init__(self, eid): self.id = eid


class FakeRegistry:
    """GetAllEntities / GetComponentJson만 흉내낸다 - 컨트롤러가 그 둘만 쓴다."""

    def __init__(self, components: dict):
        # {entity_id: {"ActionPlayerComponent": {...}}}
        self._components = components

    def GetAllEntities(self):
        return [FakeEntity(i) for i in sorted(self._components)]

    def GetComponentJson(self, entity, name):
        comp = self._components.get(getattr(entity, "id", entity), {}).get(name)
        return json.dumps(comp) if comp else "{}"


class FakeSound:
    """실제 오디오 장치를 건드리지 않고 재생 요청만 기록한다."""

    def __init__(self):
        self.played = []
        self.preloaded = []
        self.stopped = 0

    def preload(self, clips, wait_ms=1000):
        self.preloaded.extend(clips)
        return {c: True for c in clips}

    def play(self, clip, volume=1.0, volume_var=0.0):
        self.played.append(clip)
        return True

    def stop_all(self):
        self.stopped += 1


def make_wav(name: str) -> str:
    path = os.path.join(_tmpdir, name)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    rate = 44100
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b"".join(
            struct.pack("<h", int(0.2 * 32767 * math.sin(2 * math.pi * 440 * i / rate)))
            for i in range(int(rate * 0.05))
        ))
    return path


def write_action(name: str, fps: int, total_frames: int, event_frames, clip: str) -> str:
    action = ActionData(
        name=name, fps=fps, total_frames=total_frames, sections=[],
        events=[ActionEvent(frame=f, type=EventType.SOUND,
                            params={"clip": clip, "volume": 0.01})
                for f in event_frames],
        layers=[],
    )
    path = os.path.join(_tmpdir, "actions", f"{name}.action.json")
    save_action(action, path)
    return path


class CameraFakeRegistry(FakeRegistry):
    """Camera 이벤트용: 이름과 부분 SetComponentJson(엔진 deserialize처럼 준 필드만 덮어씀)을 흉내낸다."""

    def __init__(self, components: dict, names: dict):
        super().__init__(components)
        self._names = names

    def GetEntityName(self, entity):
        return self._names.get(getattr(entity, "id", entity), f"Entity_{entity}")

    def SetComponentJson(self, entity, name, js):
        eid = getattr(entity, "id", entity)
        self._components.setdefault(eid, {}).setdefault(name, {}).update(json.loads(js))


class StrictFakeSound(FakeSound):
    """실제 SoundPlayer와 같은 계약: preload한 경로로만 play가 성공한다.

    FakeSound는 아무 경로나 받아서, preload 키와 play 키가 달라 실제로는 무음이던 버그
    (sound_player.resolve_clip_path 주석)를 잡지 못했다.
    """

    def __init__(self):
        super().__init__()
        self.rejected = []

    def play(self, clip, volume=1.0, volume_var=0.0):
        if clip not in self.preloaded:
            self.rejected.append(clip)
            return False
        self.played.append(clip)
        return True


def controller_with(components, sound=None):
    return ScenePlaybackController(sound_player=sound or FakeSound()), FakeRegistry(components)


# ── 테스트 ───────────────────────────────────────────────────────────────────

def test_start_only_picks_up_action_players_with_play_on_start():
    wav = make_wav(os.path.join("sfx", "a.wav"))
    path = write_action("A", 30, 10, [5], wav)

    comps = {
        1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": True}},
        2: {"ActionPlayerComponent": {"action": path, "playOnStart": False, "loop": True}},
        3: {},                                            # 컴포넌트 없음
        4: {"ActionPlayerComponent": {"action": "", "playOnStart": True}},  # 경로 비어있음
    }
    ctrl, reg = controller_with(comps)
    started = ctrl.start(reg)

    check(started == 1, f"playOnStart=True이고 경로가 있는 1개만 시작돼야 함: {started}")
    check(ctrl.active_count == 1, f"active_count도 1: {ctrl.active_count}")


def test_action_fps_is_respected_not_tick_count():
    """§2.4의 핵심 - 틱 수가 아니라 경과 시간으로 전진시킨다.

    30fps 액션에 1초를 주면 30프레임이 지나야 한다. 만약 60fps 타이머의 틱 수를
    프레임 수로 썼다면 60프레임이 지나 두 배 빨리 재생된다.
    """
    wav = make_wav(os.path.join("sfx", "b.wav"))
    # 30fps, 60프레임(=2초), 이벤트는 프레임 30에 하나.
    path = write_action("B", 30, 60, [30], wav)

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": False}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    # 0.5초 = 15프레임. 아직 프레임 30에 못 미친다.
    ctrl.tick(0.5)
    check(len(sound.played) == 0,
          f"0.5초(15프레임)에는 프레임 30 이벤트가 나오면 안 됨: {sound.played}")

    # 누적 1.0초 = 30프레임. 이제 발화해야 한다.
    ctrl.tick(0.5)
    check(len(sound.played) == 1,
          f"1.0초(30프레임)에 정확히 1번 발화해야 함: {sound.played}")


def test_fractional_frames_accumulate_instead_of_stalling():
    """dt가 작아 프레임이 1 미만이어도 누적돼야 한다. 매번 버림하면 재생이 멈춘다."""
    wav = make_wav(os.path.join("sfx", "c.wav"))
    path = write_action("C", 30, 30, [10], wav)

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": False}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    # 60fps 틱(0.0166초)은 30fps 액션에서 0.5프레임씩이다.
    for _ in range(20):          # 20틱 = 10프레임
        ctrl.tick(1.0 / 60.0)

    check(len(sound.played) == 1,
          f"소수 프레임이 누적돼 프레임 10에 도달해야 함: {sound.played}")


def test_stop_clears_cursors_and_stops_sound():
    wav = make_wav(os.path.join("sfx", "d.wav"))
    path = write_action("D", 30, 30, [1], wav)

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": True}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)
    check(ctrl.active_count == 1, "시작됨")

    ctrl.stop()
    check(ctrl.active_count == 0, "Stop하면 커서가 비워져야 함")
    check(sound.stopped >= 1, "Stop하면 소리도 멈춰야 함")

    # 멈춘 뒤에는 전진해도 아무 일도 없어야 한다.
    before = len(sound.played)
    ctrl.tick(1.0)
    check(len(sound.played) == before, "Stop 후에는 재생되면 안 됨")


def test_broken_action_does_not_block_others():
    """액션 하나가 깨져도 나머지가 재생돼야 한다."""
    wav = make_wav(os.path.join("sfx", "e.wav"))
    good = write_action("E", 30, 30, [5], wav)
    missing = os.path.join(_tmpdir, "actions", "없는파일.action.json")

    comps = {
        1: {"ActionPlayerComponent": {"action": missing, "playOnStart": True}},
        2: {"ActionPlayerComponent": {"action": good, "playOnStart": True}},
    }
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    started = ctrl.start(reg)

    check(started == 1, f"깨진 것 하나는 건너뛰고 1개만: {started}")
    ctrl.tick(0.5)
    check(len(sound.played) >= 1, f"멀쩡한 액션은 재생돼야 함: {sound.played}")


def test_loop_flag_from_component_is_honored():
    """loop=False면 끝나고 더 이상 발화하지 않는다."""
    wav = make_wav(os.path.join("sfx", "f.wav"))
    path = write_action("F", 30, 10, [5], wav)

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": False}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    for _ in range(10):
        ctrl.tick(0.5)           # 넉넉히 지나간다

    check(len(sound.played) == 1,
          f"loop=False면 이벤트가 한 번만 나야 함: {len(sound.played)}회")


def test_looping_action_repeats():
    wav = make_wav(os.path.join("sfx", "g.wav"))
    path = write_action("G", 30, 30, [10], wav)   # 한 바퀴 = 31프레임 ≈ 1.033초

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True, "loop": True}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    for _ in range(180):          # 3초 (60fps 틱)
        ctrl.tick(1.0 / 60.0)

    check(len(sound.played) >= 2,
          f"루프면 여러 번 반복돼야 함: {len(sound.played)}회")


def test_clip_relative_to_action_file_is_preloaded():
    """§6.4 - clip은 액션 파일 기준 상대 경로이고, 그 기준으로 풀려 프리로드돼야 한다."""
    wav = make_wav(os.path.join("sfx", "h.wav"))
    path = write_action("H", 30, 10, [3], wav)   # save_action이 clip을 상대화한다

    with open(path, encoding="utf-8") as f:
        saved_clip = json.load(f)["events"][0]["params"]["clip"]
    check(not os.path.isabs(saved_clip), f"파일에는 상대 경로로 저장돼야 함: {saved_clip}")

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    check(len(sound.preloaded) == 1, f"클립 하나가 프리로드돼야 함: {sound.preloaded}")
    check(os.path.isabs(sound.preloaded[0]),
          f"프리로드 시점에는 절대 경로로 풀려야 함: {sound.preloaded}")
    check(os.path.isfile(sound.preloaded[0]),
          f"풀린 경로가 실제 파일이어야 함: {sound.preloaded}")


def test_relative_action_path_resolves_against_base_dir():
    """ActionPlayerComponent.action이 상대 경로면 base_dir 기준으로 푼다."""
    wav = make_wav(os.path.join("sfx", "i.wav"))
    write_action("I", 30, 10, [2], wav)

    comps = {1: {"ActionPlayerComponent": {"action": "actions/I.action.json",
                                           "playOnStart": True}}}
    ctrl, reg = controller_with(comps)
    started = ctrl.start(reg, base_dir=_tmpdir)

    check(started == 1, f"base_dir 기준으로 액션을 찾아야 함: {started}")


def test_zero_dt_does_nothing():
    wav = make_wav(os.path.join("sfx", "j.wav"))
    path = write_action("J", 30, 10, [1], wav)

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True}}}
    sound = FakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)

    check(ctrl.tick(0.0) == 0, "dt=0이면 아무 일도 없어야 함")
    check(ctrl.tick(-1.0) == 0, "음수 dt도 마찬가지")
    check(len(sound.played) == 0, f"재생도 없어야 함: {sound.played}")



def test_relative_clip_is_played_with_the_same_key_it_was_preloaded_with():
    """파일에서 불러온 액션(clip이 상대 경로)이 실제로 재생돼야 한다 - preload 키와 play 키 일치."""
    wav = make_wav(os.path.join("sfx", "strict.wav"))
    path = write_action("Strict", 30, 10, [2], wav)   # 저장 시 clip이 상대 경로가 된다

    comps = {1: {"ActionPlayerComponent": {"action": path, "playOnStart": True}}}
    sound = StrictFakeSound()
    ctrl, reg = controller_with(comps, sound)
    ctrl.start(reg)
    ctrl.tick(0.2)   # 30fps에서 6프레임 -> frame 2 이벤트 발화

    check(not sound.rejected, f"preload 안 된 키로 재생 시도됨(무음 버그): {sound.rejected}")
    check(len(sound.played) >= 1, f"소리가 한 번 이상 재생돼야 함: {sound.played}")


def _camera_action(name: str, target_camera: str) -> str:
    action = ActionData(
        name=name, fps=30, total_frames=10, sections=[],
        events=[ActionEvent(frame=3, type=EventType.CAMERA, params={"camera": target_camera})],
        layers=[],
    )
    path = os.path.join(_tmpdir, "actions", f"{name}.action.json")
    save_action(action, path)
    return path


def test_camera_event_makes_named_camera_highest_priority():
    """Camera 이벤트가 발화하면 그 카메라가 최고 priority + isMainCamera가 된다(엔진 규칙이 고른다)."""
    path = _camera_action("CamSwitch", "Cam B")
    comps = {
        1: {"ActionPlayerComponent": {"action": path, "playOnStart": True}},
        2: {"CameraComponent": {"isMainCamera": True, "priority": 4}},    # Main Camera
        3: {"CameraComponent": {"isMainCamera": False, "priority": 0}},   # Cam B
    }
    reg = CameraFakeRegistry(comps, {2: "Main Camera", 3: "Cam B"})
    ctrl = ScenePlaybackController(sound_player=FakeSound())
    ctrl.start(reg)
    ctrl.tick(0.2)   # 30fps에서 6프레임 -> frame 3 발화

    cam_b = comps[3]["CameraComponent"]
    check(cam_b["isMainCamera"] is True, f"Cam B가 후보가 돼야 함: {cam_b}")
    check(cam_b["priority"] == 5, f"Cam B priority는 기존 최고(4)+1이어야 함: {cam_b}")


def test_camera_event_with_unknown_name_is_ignored():
    path = _camera_action("CamMissing", "Nope")
    comps = {
        1: {"ActionPlayerComponent": {"action": path, "playOnStart": True}},
        2: {"CameraComponent": {"isMainCamera": True, "priority": 0}},
    }
    reg = CameraFakeRegistry(comps, {2: "Main Camera"})
    ctrl = ScenePlaybackController(sound_player=FakeSound())
    ctrl.start(reg)
    ctrl.tick(0.2)
    check(comps[2]["CameraComponent"] == {"isMainCamera": True, "priority": 0},
          f"없는 카메라 이름이면 아무것도 바꾸지 않아야 함: {comps[2]}")

TESTS = [
    test_camera_event_makes_named_camera_highest_priority,
    test_camera_event_with_unknown_name_is_ignored,
    test_relative_clip_is_played_with_the_same_key_it_was_preloaded_with,
    test_start_only_picks_up_action_players_with_play_on_start,
    test_action_fps_is_respected_not_tick_count,
    test_fractional_frames_accumulate_instead_of_stalling,
    test_stop_clears_cursors_and_stops_sound,
    test_broken_action_does_not_block_others,
    test_loop_flag_from_component_is_honored,
    test_looping_action_repeats,
    test_clip_relative_to_action_file_is_preloaded,
    test_relative_action_path_resolves_against_base_dir,
    test_zero_dt_does_nothing,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Sound Lite Phase 6 - Scene Editor Play 재생")
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
