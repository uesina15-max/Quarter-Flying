"""
Sound Lite Phase 4 — 이벤트 파라미터 편집 + 클립 경로 처리 검증.

docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §3 Phase 4.

QFileDialog를 띄우는 부분(사용자가 파일을 고르는 순간)은 자동화하지 않는다 - 대신
"고른 뒤에 무슨 일이 일어나는가"를 검증한다: 절대 경로로 들고 있다가 저장 시점에
`.action.json` 기준 상대 경로가 되고, 다시 로드하면 그 기준으로 풀린다(§6.4).

    python test_sound_event_editing.py
"""

import json
import math
import os
import struct
import sys
import tempfile
import wave

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from PySide6.QtWidgets import QApplication

from core.action_data import (
    ActionData, ActionEvent, EventType, save_action, load_action,
)
from panels.section_inspector import SectionInspectorPanel, SoundEventRowWidget
from panels.animation_preview import AnimationPreviewPanel

_app = QApplication.instance() or QApplication([])
_tmpdir = tempfile.mkdtemp(prefix="soundlite_p4_")
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


def make_wav(relpath: str) -> str:
    path = os.path.join(_tmpdir, relpath)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    rate = 44100
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(2); w.setframerate(rate)
        w.writeframes(b"".join(
            struct.pack("<h", int(0.2 * 32767 * math.sin(2 * math.pi * 440 * i / rate)))
            for i in range(int(rate * 0.1))
        ))
    return path


def empty_action(name="Edit") -> ActionData:
    return ActionData(name=name, fps=30, total_frames=20, sections=[], events=[], layers=[])


# ── 테스트 ───────────────────────────────────────────────────────────────────

def test_add_sound_event_creates_editable_row():
    panel = SectionInspectorPanel()
    action = empty_action()
    panel.set_action(action)

    check(len(panel._sound_widgets) == 0, "처음에는 사운드 행이 없어야 함")

    panel._on_add_sound()
    check(len(action.events) == 1, f"이벤트가 추가돼야 함: {action.events}")
    check(action.events[0].type == EventType.SOUND, "타입이 Sound여야 함")
    check(len(panel._sound_widgets) == 1, "행 위젯이 하나 생겨야 함")


def test_editing_row_writes_into_event_params():
    """행 위젯의 값이 ActionEvent.params에 그대로 들어간다(§4.1 - 자료구조 안 바꿈)."""
    panel = SectionInspectorPanel()
    action = empty_action()
    panel.set_action(action)
    panel._on_add_sound()

    row = panel._sound_widgets[0]
    row.spin_frame.setValue(7)
    row.clip_edit.setText("C:/sfx/hit.wav")
    row.spin_volume.setValue(0.6)
    row.spin_var.setValue(0.25)
    row._on_changed()

    e = action.events[0]
    check(e.frame == 7, f"프레임 반영: {e.frame}")
    check(e.params["clip"] == "C:/sfx/hit.wav", f"클립 반영: {e.params}")
    check(abs(e.params["volume"] - 0.6) < 1e-6, f"볼륨 반영: {e.params}")
    check(abs(e.params["volumeVar"] - 0.25) < 1e-6, f"변동폭 반영: {e.params}")


def test_delete_removes_only_that_sound_event():
    """events에 다른 타입이 섞여 있어도 인덱스가 아니라 객체로 지운다."""
    panel = SectionInspectorPanel()
    action = empty_action()
    action.events = [
        ActionEvent(frame=1, type=EventType.HIT, params={}),
        ActionEvent(frame=2, type=EventType.SOUND, params={"clip": "a.wav"}),
        ActionEvent(frame=3, type=EventType.CAMERA_SHAKE, params={}),
        ActionEvent(frame=4, type=EventType.SOUND, params={"clip": "b.wav"}),
    ]
    panel.set_action(action)
    check(len(panel._sound_widgets) == 2, f"Sound만 행으로: {len(panel._sound_widgets)}")

    panel._on_delete_sound(panel._sound_widgets[0])   # a.wav 삭제

    kinds = [e.type for e in action.events]
    check(len(action.events) == 3, f"하나만 지워져야 함: {action.events}")
    check(EventType.HIT in kinds and EventType.CAMERA_SHAKE in kinds,
          "다른 타입 이벤트가 함께 지워지면 안 됨")
    remaining = [e for e in action.events if e.type == EventType.SOUND]
    check(remaining[0].params["clip"] == "b.wav", f"남은 것이 b.wav여야 함: {remaining}")


def test_save_converts_absolute_clip_to_relative():
    """§6.4 - 저장 시점에 .action.json 기준 상대 경로가 된다."""
    wav = make_wav(os.path.join("sfx", "swing.wav"))
    action = empty_action("Swing")
    action.events = [ActionEvent(frame=3, type=EventType.SOUND,
                                 params={"clip": wav, "volume": 1.0, "volumeVar": 0.0})]

    action_path = os.path.join(_tmpdir, "actions", "Swing.action.json")
    save_action(action, action_path)

    with open(action_path, "r", encoding="utf-8") as f:
        raw = json.load(f)
    saved_clip = raw["events"][0]["params"]["clip"]

    check(not os.path.isabs(saved_clip), f"저장된 clip이 상대 경로여야 함: {saved_clip}")
    check(saved_clip == "../sfx/swing.wav", f"기준 폴더 대비 상대 경로: {saved_clip}")
    check("source_path" not in raw, "source_path는 직렬화되면 안 됨")
    check(action.source_path == os.path.abspath(action_path), "저장 후 출처가 기록돼야 함")


def test_loaded_relative_clip_resolves_and_preloads():
    """저장 -> 로드 후 프리뷰가 클립을 실제로 로드한다(액션이 자기 출처를 안다)."""
    wav = make_wav(os.path.join("sfx2", "boom.wav"))
    action = empty_action("Boom")
    action.events = [ActionEvent(frame=2, type=EventType.SOUND,
                                 params={"clip": wav, "volume": 0.01, "volumeVar": 0.0})]

    action_path = os.path.join(_tmpdir, "acts2", "Boom.action.json")
    save_action(action, action_path)

    loaded = load_action(action_path)
    check(loaded.source_path == os.path.abspath(action_path), "로드 시 출처가 기록돼야 함")
    check(not os.path.isabs(loaded.events[0].params["clip"]), "clip은 상대 경로 상태")

    # base_dir를 넘기지 않아도 액션의 source_path로 스스로 알아내야 한다.
    panel = AnimationPreviewPanel()
    panel.set_action(loaded)

    check(panel._sound.is_loaded(os.path.normpath(wav)),
          "상대 경로가 풀려 클립이 로드돼야 함")


def test_save_leaves_unresolvable_absolute_path_alone():
    """상대 경로를 만들 수 없으면(다른 드라이브 등) 절대 경로를 그대로 둔다."""
    action = empty_action("Weird")
    weird = "Z:\\somewhere\\far.wav" if os.name == "nt" else "/mnt/other/far.wav"
    action.events = [ActionEvent(frame=1, type=EventType.SOUND, params={"clip": weird})]

    path = os.path.join(_tmpdir, "acts3", "Weird.action.json")
    save_action(action, path)

    saved = action.events[0].params["clip"]
    # 같은 드라이브면 상대화되고, 다른 드라이브면 그대로 남는다. 어느 쪽이든 예외는 없어야 한다.
    check(isinstance(saved, str) and saved, f"경로가 유지돼야 함: {saved}")


def test_non_sound_event_clip_is_not_touched_on_save():
    """Sound가 아닌 이벤트의 params는 건드리지 않는다."""
    wav = make_wav(os.path.join("sfx3", "x.wav"))
    action = empty_action("Mixed")
    action.events = [ActionEvent(frame=1, type=EventType.HIT, params={"clip": wav})]

    path = os.path.join(_tmpdir, "acts4", "Mixed.action.json")
    save_action(action, path)

    check(action.events[0].params["clip"] == wav,
          f"Sound 아닌 이벤트는 그대로여야 함: {action.events[0].params}")


TESTS = [
    test_add_sound_event_creates_editable_row,
    test_editing_row_writes_into_event_params,
    test_delete_removes_only_that_sound_event,
    test_save_converts_absolute_clip_to_relative,
    test_loaded_relative_clip_resolves_and_preloads,
    test_save_leaves_unresolvable_absolute_path_alone,
    test_non_sound_event_clip_is_not_touched_on_save,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Sound Lite Phase 4 - 이벤트 편집 + 클립 경로")
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
