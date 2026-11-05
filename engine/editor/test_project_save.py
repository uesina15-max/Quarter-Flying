"""
프로젝트 단위 저장(File > Save Project) + 프로젝트 세션(.quarterflying/session.json) 검증.

엔진 없이 돈다. 씬 저장(엔진 SceneIO)은 여기서 다루지 않는다 - 그 부분은 에디터를 HAS_ENGINE=True로 띄워 확인한다.
기존 engine/editor/test_*.py 관례를 따른다(pytest 미사용, 직접 실행). 경로에 한글을 넣는다(CLAUDE.md 경로 UTF-8 메모).

    python test_project_save.py        (engine/editor에서)
"""

import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
sys.path.insert(0, str(Path(__file__).resolve().parent))

from core.action_data import ActionEvent, EventType, load_action, make_default_action, save_action  # noqa: E402
from project import (CameraView, EditorSession, MODE_MOTION, SaveItem, create_project, from_session_path,  # noqa: E402
                     load_session, save_items, save_session, session_file, to_session_path)

_tmp = Path(tempfile.mkdtemp(prefix="qf_projsave_"))
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


def new_project(name):
    return create_project(_tmp / "한글 폴더" / name)


# ── 세션 파일 ────────────────────────────────────────────────────────────────

def test_session_roundtrip():
    print("test_session_roundtrip")
    project = new_project("세션왕복")
    outside = _tmp / "밖" / "Other.action.json"
    session = EditorSession(
        scene=to_session_path(project, project.root / "scenes" / "메인.scene.json"),
        actions=[to_session_path(project, project.actions_dir / "공격.action.json"), to_session_path(project, outside)],
        selected_action=to_session_path(project, project.actions_dir / "공격.action.json"),
        mode=MODE_MOTION,
        camera=CameraView(target=[1.0, 2.0, 3.0], distance=7.5, yaw=30.0, pitch=-10.0),
    )
    check(session.scene == "scenes/메인.scene.json", f"프로젝트 안 경로는 상대 경로여야 함: {session.scene}")
    check(Path(session.actions[1]).is_absolute(), f"프로젝트 밖 경로는 절대 경로여야 함: {session.actions[1]}")
    path = save_session(project, session)
    check(path == session_file(project) and path.is_file(), "세션 파일이 .quarterflying/session.json에 써져야 함")
    loaded, warnings = load_session(project)
    check(warnings == [], f"경고가 없어야 함: {warnings}")
    check(loaded == session, f"왕복 결과가 같아야 함:\n  {loaded}\n  {session}")
    check(from_session_path(project, loaded.scene) == (project.root / "scenes" / "메인.scene.json").resolve(),
          "상대 경로가 프로젝트 기준으로 풀려야 함")
    check(from_session_path(project, loaded.actions[1]) == outside.resolve(), "절대 경로는 그대로여야 함")

    # 프로젝트 폴더를 통째로 옮겨도 세션이 맞는다(상대 경로라서).
    moved_root = _tmp / "한글 폴더" / "옮긴 프로젝트"
    shutil.move(str(project.root), str(moved_root))
    from project import open_project
    moved = open_project(moved_root)
    loaded2, _ = load_session(moved)
    check(from_session_path(moved, loaded2.scene) == (moved_root / "scenes" / "메인.scene.json").resolve(),
          "옮긴 프로젝트에서도 세션 씬이 새 위치로 풀려야 함")


def test_session_missing_and_broken():
    print("test_session_missing_and_broken")
    project = new_project("깨진세션")
    session, warnings = load_session(project)
    check(session == EditorSession() and warnings == [], "세션 파일이 없으면 빈 세션, 경고 없음")

    file = session_file(project)
    file.parent.mkdir(parents=True, exist_ok=True)

    def load_text(text):
        file.write_text(text, encoding="utf-8")
        return load_session(project)

    session, warnings = load_text("{ not json")
    check(session == EditorSession() and len(warnings) == 1 and "cannot read session" in warnings[0],
          f"깨진 JSON은 예외 없이 빈 세션 + 경고: {warnings}")
    session, warnings = load_text(json.dumps({"format": "something.else", "version": 1}))
    check(session == EditorSession() and "format" in warnings[0], f"format이 틀리면 빈 세션: {warnings}")
    session, warnings = load_text(json.dumps({"format": "quarterflying.editor-session", "version": 99,
                                             "scene": "scenes/X.scene.json"}))
    check(session.scene == "" and "unsupported session version 99" in warnings[0],
          f"더 새 버전은 읽지 않음: {warnings}")

    # 필드 하나가 틀려도 나머지는 살린다.
    session, warnings = load_text(json.dumps({
        "format": "quarterflying.editor-session", "version": 1,
        "scene": "scenes/Main.scene.json",
        "actions": ["a.action.json", 3],
        "selectedAction": None,
        "mode": 1,                                    # Play는 복원하지 않는다
        "camera": {"target": [0, 0], "distance": 5, "yaw": 0, "pitch": 0},
    }))
    check(session.scene == "scenes/Main.scene.json", "맞는 필드(scene)는 살아야 함")
    check(session.actions == [] and session.selected_action == "" and session.mode == 0 and session.camera is None,
          f"틀린 필드는 기본값: {session}")
    joined = "\n".join(warnings)
    for key in ("actions", "selectedAction", "mode", "camera.target"):
        check(key in joined, f"'{key}' 경고가 있어야 함: {warnings}")

    session, warnings = load_text(json.dumps({"format": "quarterflying.editor-session", "version": 1,
                                             "camera": {"target": [0, 0, 0], "distance": 0, "yaw": 0, "pitch": 0}}))
    check(session.camera is None and "distance" in warnings[0], f"distance 0은 거절: {warnings}")
    session, warnings = load_text(json.dumps({"format": "quarterflying.editor-session", "version": 1,
                                             "camera": {"target": [0, True, 0], "distance": 1, "yaw": 0, "pitch": 0}}))
    check(session.camera is None and warnings, "bool은 숫자로 받지 않음")


# ── 저장 실행부 ──────────────────────────────────────────────────────────────

def test_save_items():
    print("test_save_items")
    calls = []

    def ok(name):
        return lambda: calls.append(name) or True

    def boom():
        calls.append("boom")
        raise OSError("디스크 꽉 참")

    def cancelled():
        calls.append("cancel")
        return False

    result = save_items([SaveItem("씬 A", ok("A")), SaveItem("액션 B", boom), SaveItem("액션 C", cancelled),
                         SaveItem("세션", ok("S"))])
    check(calls == ["A", "boom", "cancel", "S"], f"하나가 실패해도 나머지를 계속 저장해야 함: {calls}")
    check(result.saved == ["씬 A", "세션"], f"저장된 목록: {result.saved}")
    check([label for label, _ in result.not_saved] == ["액션 B", "액션 C"], f"못 한 목록: {result.not_saved}")
    check("OSError: 디스크 꽉 참" in result.failure_details(), f"예외 원인이 대상 이름과 함께 남아야 함: "
                                                              f"{result.failure_details()}")
    check(not result.ok and "저장 못 함 2개" in result.summary(), f"요약: {result.summary()}")
    empty = save_items([])
    check(empty.ok and empty.summary() == "저장할 변경 없음", f"빈 목록 요약: {empty.summary()}")


# ── 액션 변경 감지 ───────────────────────────────────────────────────────────

def test_action_modified():
    print("test_action_modified")
    action = make_default_action("공격")
    check(action.is_modified(), "파일이 없는 새 액션은 변경 있음")
    folder = _tmp / "한글 폴더" / "액션"
    clip = _tmp / "한글 폴더" / "소리" / "히트.wav"
    action.events.append(ActionEvent(frame=3, type=EventType.SOUND, params={"clip": str(clip)}))
    path = folder / "공격.action.json"
    save_action(action, str(path))
    # save_action은 clip을 상대 경로로 바꾸며 저장한다 - 그 뒤의 내용이 기준이어야 "저장 직후 변경 없음"이다.
    check(not action.is_modified(), "저장 직후에는 변경 없음 (clip 상대화 후 기준)")
    action.total_frames += 1
    check(action.is_modified(), "필드를 바꾸면 변경 있음")
    action.total_frames -= 1
    check(not action.is_modified(), "되돌리면 변경 없음(내용 비교)")
    action.events[0].params["clip"] = "다른.wav"
    check(action.is_modified(), "중첩된 params를 바꿔도 잡혀야 함")
    loaded = load_action(str(path))
    check(not loaded.is_modified(), "불러온 직후에는 변경 없음")
    check(loaded == load_action(str(path)), "기준 스냅샷 필드는 == 비교에 들어가지 않아야 함(motion_list가 index()로 찾는다)")


# ── 에디터 연결 (오프스크린 Qt) ──────────────────────────────────────────────

def test_editor_integration():
    print("test_editor_integration")
    try:
        from PySide6.QtWidgets import QApplication, QMainWindow, QMessageBox
    except ImportError:
        print("  (PySide6 없음 - 건너뜀)")
        return
    app = QApplication.instance() or QApplication([])
    import project_actions
    from motion_editor import MotionEditorWidget
    from project import set_current_project

    class FakeStatus:
        def __init__(self):
            self.lines = []

        def log(self, text):
            self.lines.append(text)

    class Window(project_actions.ProjectActionsMixin, QMainWindow):
        def __init__(self):
            super().__init__()
            self._engine = None
            self._world = None          # 엔진 없음 → 씬 항목은 나오지 않는다(씬은 실제 실행으로 확인)
            self.is_playing = False
            self.status = FakeStatus()
            self.motion_editor = MotionEditorWidget(shared_viewport=None)
            # 실제 패널을 쓴다 - 처음 연결 때 is_dirty(프로퍼티)를 메서드로 불러서 실제 실행에서야 터졌다.
            from panels.input_actions_panel import InputActionsPanel
            self.input_actions_panel = InputActionsPanel()
            self.modes = []

        def _set_mode(self, index):
            self.modes.append(index)

    project = new_project("에디터연결")
    set_current_project(project)
    a1 = make_default_action("걷기")
    a2 = make_default_action("공격")
    save_action(a1, str(project.actions_dir / "걷기.action.json"))
    save_action(a2, str(project.actions_dir / "공격.action.json"))

    w = Window()
    w.motion_editor.restore_actions([a1.source_path, a2.source_path], selected=a2.source_path)
    check(w._unsaved_items(include_project_data=True) == [], "불러온 그대로면 저장할 것이 없어야 함")

    # 편집 → 목록에 나타남. 씬만 볼 때는 나타나지 않음.
    w.motion_editor.motion_list.get_actions()[0].fps = 60
    items = w._unsaved_items(include_project_data=True)
    check([i.label for i in items] == ["액션 걷기"], f"바뀐 액션만 목록에: {[i.label for i in items]}")
    check(w._unsaved_items(include_project_data=False) == [], "씬 전환 확인에는 액션이 들어가지 않아야 함")
    w.input_actions_panel._dirty = True
    labels = [i.label for i in w._unsaved_items(include_project_data=True)]
    check(labels[0].startswith("입력 액션 맵"), f"입력 액션 맵이 바뀌면 목록에: {labels}")
    w.input_actions_panel._dirty = False

    # 종료 확인: 취소 → False, 저장 안 함 → True, 저장 → 파일에 기록되고 True.
    original_question = QMessageBox.question
    try:
        QMessageBox.question = staticmethod(lambda *a, **k: QMessageBox.Cancel)
        check(w._confirm_unsaved("Exit", True) is False, "취소하면 계속하지 않음")
        QMessageBox.question = staticmethod(lambda *a, **k: QMessageBox.Discard)
        check(w._confirm_unsaved("Exit", True) is True, "저장 안 함이면 계속함")
        check(json.loads(Path(a1.source_path).read_text(encoding="utf-8"))["fps"] == 30, "저장 안 함이면 파일 그대로")
        QMessageBox.question = staticmethod(lambda *a, **k: QMessageBox.Save)
        check(w._confirm_unsaved("Exit", True) is True, "저장하면 계속함")
        check(json.loads(Path(a1.source_path).read_text(encoding="utf-8"))["fps"] == 60, "저장하면 파일에 반영")
        check(w._unsaved_items(True) == [], "저장 뒤에는 변경 없음")

        # 새 액션(파일 없음)의 저장 위치 대화상자를 취소하면 종료를 막아야 한다(그 액션이 사라지므로).
        w.motion_editor.motion_list.set_actions(w.motion_editor.motion_list.get_actions() + [make_default_action("새것")])
        from PySide6.QtWidgets import QFileDialog
        original_save_dialog = QFileDialog.getSaveFileName
        original_warning = QMessageBox.warning
        warned = []
        try:
            QFileDialog.getSaveFileName = staticmethod(lambda *a, **k: ("", ""))
            QMessageBox.warning = staticmethod(lambda *a, **k: warned.append(a[2] if len(a) > 2 else ""))
            check(w._confirm_unsaved("Exit", True) is False, "저장 위치를 취소하면 계속하지 않음")
            check(warned and "액션 새것 (새 액션)" in warned[0], f"어느 항목을 못 했는지 알려야 함: {warned}")
        finally:
            QFileDialog.getSaveFileName = original_save_dialog
            QMessageBox.warning = original_warning
        w.motion_editor.motion_list.set_actions(w.motion_editor.motion_list.get_actions()[:2])
    finally:
        QMessageBox.question = original_question

    # Save Project: 세션에 열어 둔 액션·선택·모드가 기록된다.
    w.motion_editor.motion_list.select_action(w.motion_editor.motion_list.get_actions()[1])
    w._on_save_project()
    check(any("프로젝트 저장" in line and "Saved" in line for line in w.status.lines),
          f"상태바에 저장 결과: {w.status.lines[-1:]}")
    session, warnings = load_session(project)
    check(warnings == [], f"세션 경고 없음: {warnings}")
    check(session.actions == ["assets/actions/걷기.action.json", "assets/actions/공격.action.json"],
          f"세션의 액션 목록(프로젝트 상대): {session.actions}")
    check(session.selected_action == "assets/actions/공격.action.json", f"선택: {session.selected_action}")

    # 다시 연 에디터가 같은 목록과 선택으로 돌아온다. 없어진 파일은 건너뛴다.
    session.actions.append("assets/actions/없어진.action.json")
    session.mode = MODE_MOTION
    save_session(project, session)
    w2 = Window()
    w2._apply_session_ui(w2._load_project_session())
    names = [a.name for a in w2.motion_editor.motion_list.get_actions()]
    check(names == ["걷기", "공격"], f"세션에서 액션 목록 복원(없는 파일은 건너뜀): {names}")
    current = w2.motion_editor.motion_list.current_action()
    check(current is not None and current.name == "공격", f"선택 복원: {current and current.name}")
    w2.stack = type("Stack", (), {"currentIndex": staticmethod(lambda: 0)})()
    w2._apply_session_mode()
    check(w2.modes == [MODE_MOTION], f"모드 복원: {w2.modes}")
    w2.is_playing = True
    w2.modes.clear()
    w2._apply_session_mode()
    check(w2.modes == [], "Play 중에는 모드를 바꾸지 않음")
    app.processEvents()


if __name__ == "__main__":
    try:
        test_session_roundtrip()
        test_session_missing_and_broken()
        test_save_items()
        test_action_modified()
        test_editor_integration()
    finally:
        shutil.rmtree(_tmp, ignore_errors=True)
    if _failures:
        print(f"\n{len(_failures)} FAILED")
        sys.exit(1)
    print("\nALL PASSED")
