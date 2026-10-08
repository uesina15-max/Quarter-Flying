"""
Build Game(docs/GAME_BUILD_PLAN.md Phase 4) 검증 + 프로젝트 폴더(Phase 1, editor/project) 검증.

엔진 없이 돈다. 가짜 Player 템플릿(빈 exe 파일 + 매니페스트)으로 조립 단계까지 확인하고,
실제 템플릿(build/runtime/win-x64)이 있으면 마지막 테스트에서 진짜로 빌드해 실행 검증까지 한다.

    python gamebuild/test_game_builder.py        (engine/editor에서)
"""

import hashlib
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from core.action_data import ActionData, ActionEvent, EventType, save_action  # noqa: E402
from gamebuild.game_builder import (BuildError, BuildSettings, MANIFEST, TEMPLATE_FORMAT,  # noqa: E402
                                    build_game, collect_dependencies, find_runtime_template,
                                    load_manifest, sanitize_name, verify_template)
from project import (ProjectError, create_project, empty_scene_document, open_project,  # noqa: E402
                     sample_project)

_tmp = Path(tempfile.mkdtemp(prefix="qf_gamebuild_"))
_failures = []


def check(condition, message):
    if not condition:
        _failures.append(message)
        print(f"  FAIL: {message}")
    return condition


def expect_error(exc_type, fn, contains=""):
    try:
        fn()
    except exc_type as e:
        check(contains in str(e), f"에러 메시지에 '{contains}'가 없음: {e}")
        return str(e)
    check(False, f"{exc_type.__name__}이 나야 하는데 나지 않음")
    return ""


def make_fake_template(root: Path, abi=None, engine="0.1.0") -> Path:
    t = root / "template"
    (t / "python" / "core").mkdir(parents=True)
    (t / "quarterflying_player.exe").write_bytes(b"MZ fake player")
    (t / "quarterflying_engine.dll").write_bytes(b"fake dll")
    (t / "python" / "runtime_bootstrap.py").write_text("# fake\n", encoding="utf-8")
    files = {}
    for p in sorted(t.rglob("*")):
        if p.is_file():
            files[p.relative_to(t).as_posix()] = hashlib.sha256(p.read_bytes()).hexdigest()
    manifest = {"format": TEMPLATE_FORMAT, "templateVersion": 1, "engineVersion": engine,
                "pythonAbi": abi or f"cp{sys.version_info.major}{sys.version_info.minor}",
                "player": "quarterflying_player.exe", "files": files}
    (t / MANIFEST).write_text(json.dumps(manifest), encoding="utf-8")
    return t


def write_scene(path: Path, entities):
    doc = empty_scene_document()
    doc["entities"] = entities
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc), encoding="utf-8")


def new_project(name: str):
    return create_project(_tmp / name)


# ── 프로젝트 (Phase 1) ───────────────────────────────────────────────────────

def test_create_and_open_project_round_trip():
    p = new_project("RoundTrip")
    check(p.project_file.is_file(), "project.json이 생기지 않음")
    check((p.assets_dir / "models").is_dir() and p.scenes_dir.is_dir() and p.scripts_dir.is_dir(), "기본 폴더 누락")
    check(p.start_scene_path.is_file(), "시작 씬 파일이 생기지 않음")
    p.build.window_width = 800
    p.build.include_dirs = ["assets/prefabs"]
    p.save()
    q = open_project(p.root)
    check(q.name == "RoundTrip", f"이름이 다름: {q.name}")
    check(q.start_scene == "scenes/Main.scene.json", f"시작 씬이 다름: {q.start_scene}")
    check(q.build.window_width == 800 and q.build.include_dirs == ["assets/prefabs"], "빌드 설정이 저장되지 않음")


def test_create_project_refuses_nonempty_folder():
    folder = _tmp / "NotEmpty"
    folder.mkdir()
    (folder / "keep.txt").write_text("x", encoding="utf-8")
    expect_error(ProjectError, lambda: create_project(folder), "not an empty folder")
    check((folder / "keep.txt").read_text(encoding="utf-8") == "x", "기존 파일이 바뀜")


def test_open_project_errors_name_the_problem():
    expect_error(ProjectError, lambda: open_project(_tmp / "nowhere"), "not found")
    bad = _tmp / "BadType"
    bad.mkdir()
    (bad / "project.json").write_text(json.dumps({"format": "quarterflying.project", "version": 1,
                                                   "build": {"window": {"width": "wide"}}}), encoding="utf-8")
    expect_error(ProjectError, lambda: open_project(bad), "build.window.width")
    newer = _tmp / "Newer"
    newer.mkdir()
    (newer / "project.json").write_text(json.dumps({"format": "quarterflying.project", "version": 99}),
                                        encoding="utf-8")
    expect_error(ProjectError, lambda: open_project(newer), "newer")


def test_sample_project_paths_match_engine_assets():
    s = sample_project()
    check(s.is_sample, "샘플 프로젝트 표시가 없음")
    check((s.assets_dir / "prefabs").is_dir(), f"샘플 프로젝트 prefabs 폴더가 없음: {s.assets_dir}")
    expect_error(ProjectError, s.save, "sample")


def test_project_relative_rejects_outside_paths():
    p = new_project("Relative")
    check(p.relative(p.root / "assets" / "a.obj") == "assets/a.obj", "상대 경로 변환이 틀림")
    expect_error(ProjectError, lambda: p.relative(_tmp), "outside")


# ── 의존성 수집 (빌드 단계 3~5) ──────────────────────────────────────────────

def _project_with_content(name: str):
    p = new_project(name)
    (p.assets_dir / "models" / "ship.obj").write_text("v 0 0 0\n", encoding="utf-8")
    (p.assets_dir / "models" / "unused.obj").write_text("v 0 0 0\n", encoding="utf-8")
    (p.assets_dir / "textures" / "ship.png").write_bytes(b"png")
    (p.assets_dir / "sfx" / "blip.wav").write_bytes(b"RIFF")
    action = ActionData(name="Blink", fps=30, total_frames=10)
    action.events.append(ActionEvent(frame=1, type=EventType.SOUND, params={"clip": "../sfx/blip.wav"}))
    save_action(action, str(p.assets_dir / "actions" / "blink.action.json"))
    (p.scripts_dir / "helpers").mkdir()
    (p.scripts_dir / "helpers" / "__init__.py").write_text("", encoding="utf-8")
    (p.scripts_dir / "helpers" / "mathx.py").write_text("import math\n", encoding="utf-8")
    (p.scripts_dir / "player.py").write_text(
        "import os\nimport quarterflying\nfrom helpers import mathx\nimport numpy\n\n"
        "class Player:\n    def __init__(self, e):\n        pass\n", encoding="utf-8")
    write_scene(p.start_scene_path, [
        {"uuid": 1, "name": "Ship", "components": {
            "RenderableComponent": {"meshPath": "assets/models/ship.obj", "texturePath": "assets/textures/ship.png"},
            "ActionPlayerComponent": {"action": "actions/blink.action.json"},
            "ScriptComponent": {"module_name": "player", "class_name": "Player"}}},
        {"uuid": 2, "name": "Cube", "components": {"RenderableComponent": {"meshPath": ""}}},
        {"uuid": 3, "name": "Builtin", "components": {"RenderableComponent": {"meshPath": "builtin:cube"}}},
    ])
    return p


def test_dependencies_follow_meshes_textures_actions_clips_and_scripts():
    p = _project_with_content("Deps")
    deps = collect_dependencies(p, p.start_scene_path)
    rel = {p.relative(f) for f in deps.files}
    check(not deps.errors, f"에러가 나면 안 됨: {deps.errors}")
    for expected in ("scenes/Main.scene.json", "assets/models/ship.obj", "assets/textures/ship.png",
                     "assets/actions/blink.action.json", "assets/sfx/blip.wav", "scripts/player.py",
                     "scripts/helpers/mathx.py", "scripts/helpers/__init__.py"):
        check(expected in rel, f"수집되지 않음: {expected}")
    check("assets/models/unused.obj" not in rel, "참조되지 않은 에셋이 수집됨")
    check(any("numpy" in w for w in deps.warnings), f"서드파티 import 경고가 없음: {deps.warnings}")
    check(not any("'os'" in w or "quarterflying" in w for w in deps.warnings), "표준 라이브러리/엔진 모듈을 경고함")


def test_missing_files_fail_with_entity_component_and_path():
    p = new_project("Missing")
    write_scene(p.start_scene_path, [
        {"uuid": 1, "name": "Broken", "components": {"RenderableComponent": {"meshPath": "assets/models/nope.obj"}}},
        {"uuid": 2, "name": "NoScript", "components": {"ScriptComponent": {"module_name": "ghost", "class_name": "G"}}},
    ])
    deps = collect_dependencies(p, p.start_scene_path)
    text = "\n".join(deps.errors)
    check("'Broken'" in text and "RenderableComponent.meshPath" in text and "nope.obj" in text,
          f"누락 메시 보고에 엔티티/컴포넌트/경로가 없음: {text}")
    check("'NoScript'" in text and "ghost" in text, f"누락 스크립트 보고가 없음: {text}")


def test_pyside_import_gets_specific_warning():
    p = new_project("Qt")
    (p.scripts_dir / "ui.py").write_text("from PySide6.QtWidgets import QLabel\n", encoding="utf-8")
    write_scene(p.start_scene_path, [{"uuid": 1, "components": {"ScriptComponent": {"module_name": "ui",
                                                                                     "class_name": "U"}}}])
    deps = collect_dependencies(p, p.start_scene_path)
    check(any("인게임 UI" in w and "PySide6" in w for w in deps.warnings), f"PySide6 전용 경고가 없음: {deps.warnings}")


def test_non_scene_json_is_rejected():
    p = new_project("Legacy")
    p.start_scene_path.write_text(json.dumps({"version": 1, "objects": []}), encoding="utf-8")
    expect_error(BuildError, lambda: collect_dependencies(p, p.start_scene_path), "Save Scene")


# ── 템플릿 (빌드 단계 1) ─────────────────────────────────────────────────────

def test_template_verification_catches_abi_engine_and_tampering():
    root = _tmp / "tpl_checks"
    t = make_fake_template(root)
    m = load_manifest(t)
    verify_template(t, m, m["pythonAbi"], "0.1.0")     # 통과해야 함
    expect_error(BuildError, lambda: verify_template(t, m, "cp299", None), "ABI")
    expect_error(BuildError, lambda: verify_template(t, m, None, "9.9.9"), "엔진 버전")
    (t / "quarterflying_engine.dll").write_bytes(b"tampered")
    expect_error(BuildError, lambda: verify_template(t, m, None, None), "해시")
    (t / "quarterflying_engine.dll").unlink()
    expect_error(BuildError, lambda: verify_template(t, m, None, None), "누락")


def test_missing_template_lists_searched_paths():
    msg = expect_error(BuildError, lambda: find_runtime_template(_tmp / "no_template_here"), "player_template")
    check(str(_tmp / "no_template_here") in msg, "찾아본 경로가 메시지에 없음")


# ── 조립 (빌드 단계 6) ───────────────────────────────────────────────────────

def _settings(p, template, out_name="Out"):
    s = BuildSettings.from_project(p)
    s.template_dir = template
    s.out_dir = _tmp / out_name / s.name
    s.run_smoke_test = False
    return s


def test_build_assembles_only_reachable_files_and_game_json():
    p = _project_with_content("Assemble")
    p.build.name = "My Game"
    t = make_fake_template(_tmp / "tpl_assemble")
    report = build_game(_settings(p, t))
    out = report.output_dir
    check((out / "My Game.exe").is_file(), "실행 파일 이름이 바뀌지 않음")
    check(not (out / "quarterflying_player.exe").exists(), "원래 Player 이름이 남음")
    check(not (out / MANIFEST).exists(), "템플릿 매니페스트가 출력에 남음")
    check((out / "assets/models/ship.obj").is_file(), "메시가 복사되지 않음")
    check((out / "scripts/player.py").is_file(), "스크립트가 scripts/로 복사되지 않음")
    check(not (out / "assets/models/unused.obj").exists(), "참조되지 않은 에셋이 복사됨")
    game = json.loads((out / "game.json").read_text(encoding="utf-8"))
    check(game["startScene"] == "scenes/Main.scene.json" and game["title"] == "My Game", f"game.json이 틀림: {game}")
    check(game["window"]["width"] == p.build.window_width, "창 설정이 game.json에 들어가지 않음")
    check(any("numpy" in w for w in report.warnings), "경고가 보고서에 없음")


def test_build_failure_leaves_previous_output_untouched():
    p = _project_with_content("KeepOld")
    t = make_fake_template(_tmp / "tpl_keep")
    s = _settings(p, t)
    build_game(s)
    marker = s.out_dir / "previous_build_marker.txt"
    marker.write_text("old", encoding="utf-8")
    (p.assets_dir / "models" / "ship.obj").unlink()          # 다음 빌드는 실패해야 한다
    expect_error(BuildError, lambda: build_game(s), "ship.obj")
    check(marker.is_file(), "실패한 빌드가 이전 결과를 지움")
    check(not s.out_dir.with_name(f".{s.out_dir.name}.building").exists(), "임시 폴더가 남음")


def test_cancel_removes_partial_output():
    p = _project_with_content("Cancel")
    t = make_fake_template(_tmp / "tpl_cancel")
    s = _settings(p, t)
    calls = {"n": 0}

    def cancel_after_some_steps():
        calls["n"] += 1
        return calls["n"] > 4

    expect_error(BuildError, lambda: build_game(s, is_cancelled=cancel_after_some_steps), "취소")
    check(not s.out_dir.exists(), "취소한 빌드의 출력 폴더가 생김")
    check(not s.out_dir.with_name(f".{s.out_dir.name}.building").exists(), "취소 후 임시 폴더가 남음")


def test_qt_files_in_output_fail_the_build():
    p = _project_with_content("QtLeak")
    t = make_fake_template(_tmp / "tpl_qt")
    (t / "Qt6Core.dll").write_bytes(b"x")       # 템플릿이 오염된 경우(매니페스트에는 없는 파일)
    expect_error(BuildError, lambda: build_game(_settings(p, t)), "Qt6Core.dll")


def test_sample_project_cannot_be_built():
    s = BuildSettings.from_project(sample_project())
    expect_error(BuildError, lambda: build_game(s), "샘플")


def test_sanitize_name():
    check(sanitize_name('a<b>:c?') == "a_b__c_", f"파일 이름 정리가 틀림: {sanitize_name('a<b>:c?')}")
    check(sanitize_name("  ...") == "Game", "빈 이름 대체가 틀림")


# ── 실제 템플릿으로 끝까지 (있을 때만) ────────────────────────────────────────

def test_real_template_build_and_smoke_test_from_moved_folder():
    try:
        template = find_runtime_template()
    except BuildError:
        print("  SKIP: 실제 Player 템플릿이 없음(cmake --build . --target player_template)")
        return
    # 한글 폴더 + 실제 메시 파일: 경로를 ANSI로 해석하던 버그(core/Utf8Path.h)는 meshPath가 빈 씬에서
    # 드러나지 않았으므로 둘 다 넣는다.
    p = new_project("RealGame 한글")
    shutil.copy2(Path(__file__).resolve().parents[2] / "assets" / "models" / "pyramid.obj",
                 p.assets_dir / "models" / "pyramid.obj")
    (p.scripts_dir / "hello.py").write_text(
        "class Hello:\n    def __init__(self, e):\n        pass\n"
        "    def on_create(self):\n        print('[hello] on_create', flush=True)\n", encoding="utf-8")
    write_scene(p.start_scene_path, [
        {"uuid": 1, "name": "Main Camera", "components": {
            "TransformComponent": {"position": [0, 2, 6], "rotation": [-15, 0, 0], "scale": [1, 1, 1]},
            "CameraComponent": {"isMainCamera": True, "fov": 60.0, "nearPlane": 0.1, "farPlane": 100.0}}},
        {"uuid": 2, "name": "Cube", "components": {
            "TransformComponent": {"position": [0, 0, 0], "rotation": [0, 0, 0], "scale": [1, 1, 1]},
            "RenderableComponent": {"meshPath": "assets/models/pyramid.obj"}}},
        {"uuid": 3, "name": "Hello", "components": {"ScriptComponent": {"module_name": "hello", "class_name": "Hello"}}},
    ])
    s = BuildSettings.from_project(p)
    s.template_dir = template
    s.out_dir = _tmp / "RealOut" / s.name
    report = build_game(s)
    check("[hello] on_create" in report.smoke_test_log, f"빌드 중 실행 검증에서 스크립트가 돌지 않음:\n{report.smoke_test_log[-800:]}")
    check("Loaded mesh" in report.smoke_test_log and "pyramid.obj" in report.smoke_test_log,
          f"한글 경로의 메시를 불러오지 못함:\n{report.smoke_test_log[-800:]}")
    # 다른 경로로 옮겨도 실행되는가(경로가 코드에 박혀 있지 않은가)
    moved = _tmp / "Moved Folder 한글" / s.name
    shutil.copytree(report.output_dir, moved)
    from gamebuild.game_builder import run_smoke_test
    ok, log = run_smoke_test(moved / f"{s.name}.exe", 20, 60)
    check(ok and "[hello] on_create" in log, f"옮긴 폴더에서 실행 실패:\n{log[-800:]}")
    print(f"  (실제 빌드: {report.file_count} files, {report.total_bytes / 1e6:.1f} MB)")


TESTS = [
    test_create_and_open_project_round_trip,
    test_create_project_refuses_nonempty_folder,
    test_open_project_errors_name_the_problem,
    test_sample_project_paths_match_engine_assets,
    test_project_relative_rejects_outside_paths,
    test_dependencies_follow_meshes_textures_actions_clips_and_scripts,
    test_missing_files_fail_with_entity_component_and_path,
    test_pyside_import_gets_specific_warning,
    test_non_scene_json_is_rejected,
    test_template_verification_catches_abi_engine_and_tampering,
    test_missing_template_lists_searched_paths,
    test_build_assembles_only_reachable_files_and_game_json,
    test_build_failure_leaves_previous_output_untouched,
    test_cancel_removes_partial_output,
    test_qt_files_in_output_fail_the_build,
    test_sample_project_cannot_be_built,
    test_sanitize_name,
    test_real_template_build_and_smoke_test_from_moved_folder,
]

if __name__ == "__main__":
    print("=" * 60)
    print("Build Game (Phase 4) + 프로젝트 폴더 (Phase 1)")
    print("=" * 60)
    for t in TESTS:
        before = len(_failures)
        print(f"\n[{t.__name__}]")
        try:
            t()
        except Exception as e:
            _failures.append(f"{t.__name__} 예외: {type(e).__name__}: {e}")
            print(f"  ERROR: {type(e).__name__}: {e}")
        if len(_failures) == before:
            print("  OK")
    shutil.rmtree(_tmp, ignore_errors=True)
    print("\n" + "=" * 60)
    if _failures:
        print(f"FAILED: {len(_failures)}건")
        for f in _failures:
            print(f"  - {f}")
        sys.exit(1)
    print(f"PASSED: {len(TESTS)} tests")
