"""
editor/gamebuild/game_builder.py

Build Game 로직. docs/GAME_BUILD_PLAN.md Phase 4.

**PySide6를 import하지 않는다(D3).** 표준 라이브러리와 Qt 없는 에디터 모듈(project, core.action_data,
core.sound_player)만 쓴다. 그래서 단위 테스트가 쉽고, 에디터 없이도 실행할 수 있다:

    python -m gamebuild.game_builder <프로젝트 폴더> [--out 출력폴더] [--no-smoke-test]

Build Game은 **컴파일하지 않는다(C1).** Player 템플릿(runtime/win-x64, runtime/stage_template.py가
조립)을 복사하고, 시작 씬에서 참조되는 파일만 골라 함께 넣는다.

빌드 단계(build_game):
  1. 템플릿 검증    runtime_manifest.json — 엔진 버전·Python ABI 일치, 파일 해시(변조·누락)
  2. 씬 검증        시작 씬의 헤더(format/version)
  3. 의존성 수집    컴포넌트의 경로 필드 → 파일, 액션 → 클립, 스크립트 → import하는 프로젝트 모듈
  4. 누락 파일      참조된 파일이 없으면 빌드 실패(어느 엔티티·컴포넌트·필드인지 보고)
  5. import 점검    표준 라이브러리·quarterflying·프로젝트 모듈 외의 import는 경고
  6. 조립           임시 폴더에 만든 뒤 성공하면 출력 폴더와 교체. Qt 파일이 섞였으면 실패
  7. 자체 검증      출력된 실행 파일을 --smoke-test로 실행해 종료 코드 확인
  8. (선택) zip

판정 기준(§0.5 맞물림 4): **참조된 파일이 없으면 실패, 수집 규칙이 모르는 구성은 허용.** 스크립트가
런타임에 여는 파일처럼 정적으로 찾을 수 없는 것은 include_dirs로 넣는다.
"""

from __future__ import annotations

import ast
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Dict, Iterable, List, Optional, Set, Tuple

if __package__ in (None, ""):
    # `python gamebuild/game_builder.py`로 직접 실행할 때도 editor/의 모듈(project, core)을 찾게 한다.
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from project import Project, ProjectError, SCENE_SUFFIX  # noqa: E402

TEMPLATE_FORMAT = "quarterflying.runtime-template"
TEMPLATE_VERSION = 1
MANIFEST = "runtime_manifest.json"
SCENE_FORMAT = "quarterflying.scene"
SCENE_VERSION = 1
GAME_CONFIG_VERSION = 1

# 컴포넌트 이름 → (경로 필드, 기준). 기준: "project" = 프로젝트 폴더, "assets" = assets 폴더.
# 새 컴포넌트(예: 인게임 UI의 폰트·이미지 경로)가 생기면 여기에 행만 추가한다.
COMPONENT_PATH_FIELDS: Dict[str, List[Tuple[str, str]]] = {
    "RenderableComponent": [("meshPath", "project"), ("texturePath", "project")],
    "ActionPlayerComponent": [("action", "assets")],
}

# 빌드 출력 폴더에 있으면 안 되는 것(설계 제약 C4).
FORBIDDEN_TOP_LEVEL = ("editor",)
FORBIDDEN_NAME_PREFIXES = ("PySide6", "shiboken6", "Qt6")

_INVALID_NAME_CHARS = re.compile(r'[<>:"/\\|?*\x00-\x1f]')

ProgressFn = Callable[[str, float], None]
CancelFn = Callable[[], bool]


class BuildError(Exception):
    """빌드 실패. 메시지는 사용자에게 그대로 보여 줄 수 있게 원인을 담는다."""


class BuildCancelled(BuildError):
    pass


@dataclass
class BuildSettings:
    project: Project
    out_dir: Path
    name: str
    start_scene: str                      # 프로젝트 상대 경로
    window_width: int = 1280
    window_height: int = 720
    fullscreen: bool = False
    include_dirs: List[str] = field(default_factory=list)
    zip_output: bool = False
    template_dir: Optional[Path] = None   # 비우면 find_runtime_template()
    run_smoke_test: bool = True
    smoke_test_frames: int = 30
    smoke_test_timeout: float = 60.0

    @classmethod
    def from_project(cls, project: Project) -> "BuildSettings":
        b = project.build
        name = sanitize_name(b.name or project.name or "Game")
        # 상대 경로는 프로젝트 폴더 기준이다. 예전에는 기본값(project.root/Build/...)에 project.root를 한 번 더
        # 붙여서, 상대 경로로 연 프로젝트("inst_proj")의 출력이 inst_proj/inst_proj/Build/...가 됐다.
        root = project.root.resolve()
        out = Path(b.output_dir) if b.output_dir else Path("Build") / name
        if not out.is_absolute():
            out = root / out
        return cls(project=project, out_dir=out, name=name,
                   start_scene=b.start_scene or project.start_scene,
                   window_width=b.window_width, window_height=b.window_height,
                   fullscreen=b.fullscreen, include_dirs=list(b.include_dirs))


@dataclass
class BuildReport:
    output_dir: Path
    exe_path: Path
    file_count: int = 0
    total_bytes: int = 0
    content_files: List[str] = field(default_factory=list)   # 템플릿 외에 복사한 게임 파일(출력 상대 경로)
    warnings: List[str] = field(default_factory=list)
    smoke_test_log: str = ""
    zip_path: Optional[Path] = None


def sanitize_name(name: str) -> str:
    cleaned = _INVALID_NAME_CHARS.sub("_", name).strip().rstrip(".")
    return cleaned or "Game"


# ── 템플릿 ───────────────────────────────────────────────────────────────────

def template_search_paths(explicit: Optional[Path] = None) -> List[Path]:
    # 템플릿을 명시했으면(인자 또는 QF_RUNTIME_TEMPLATE) 그 경로만 본다. 틀린 경로를 줬는데 다른
    # 템플릿(예: 개발 빌드의 것)으로 조용히 넘어가면, 의도와 다른 엔진으로 빌드된 걸 알 수 없다.
    if explicit:
        return [Path(explicit)]
    if os.environ.get("QF_RUNTIME_TEMPLATE"):
        return [Path(os.environ["QF_RUNTIME_TEMPLATE"])]
    editor_dir = Path(__file__).resolve().parents[1]           # engine/editor 또는 <설치>/editor
    paths = []
    paths.append(editor_dir.parent / "runtime" / "win-x64")             # 설치본(§3.1)
    paths.append(editor_dir.parent / "build" / "runtime" / "win-x64")   # 개발 빌드(player_template 타깃)
    return paths


def find_runtime_template(explicit: Optional[Path] = None) -> Path:
    searched = template_search_paths(explicit)
    for p in searched:
        if (p / MANIFEST).is_file():
            return p
    raise BuildError("Player 템플릿을 찾지 못했습니다(runtime_manifest.json이 있는 폴더). 찾아본 곳:\n  "
                     + "\n  ".join(str(p) for p in searched)
                     + "\n개발 환경이면 `cmake --build . --config Release --target player_template`로 만드세요.")


def _sha256(path: Path) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def load_manifest(template_dir: Path) -> dict:
    path = template_dir / MANIFEST
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as e:
        raise BuildError(f"템플릿 매니페스트를 읽을 수 없습니다: {path}: {e}") from None
    if manifest.get("format") != TEMPLATE_FORMAT:
        raise BuildError(f"{path}: Player 템플릿 매니페스트가 아닙니다(format이 다름)")
    if int(manifest.get("templateVersion", 0)) > TEMPLATE_VERSION:
        raise BuildError(f"{path}: 템플릿 버전 {manifest.get('templateVersion')}은 이 에디터보다 새 버전입니다")
    return manifest


def current_python_abi() -> str:
    return f"cp{sys.version_info.major}{sys.version_info.minor}"


def current_engine_version() -> Optional[str]:
    """에디터가 쓰는 엔진 모듈의 버전. 엔진 없이(더미 모드/CLI) 돌면 None — 이 경우 버전 비교는 건너뛴다."""
    try:
        import quarterflying  # noqa: PLC0415
        return getattr(quarterflying, "__version__", None)
    except ImportError:
        return None


def verify_template(template_dir: Path, manifest: dict,
                    expected_abi: Optional[str], expected_engine: Optional[str]) -> None:
    problems = []
    if expected_abi and manifest.get("pythonAbi") != expected_abi:
        problems.append(f"Python ABI 불일치: 템플릿 {manifest.get('pythonAbi')} / 에디터 {expected_abi}")
    if expected_engine and manifest.get("engineVersion") != expected_engine:
        problems.append(f"엔진 버전 불일치: 템플릿 {manifest.get('engineVersion')} / 에디터 {expected_engine}")
    for rel, digest in sorted(manifest.get("files", {}).items()):
        p = template_dir / rel
        if not p.is_file():
            problems.append(f"템플릿 파일 누락: {rel}")
        elif _sha256(p) != digest:
            problems.append(f"템플릿 파일이 바뀜(해시 불일치): {rel}")
    player = manifest.get("player", "")
    if not player or not (template_dir / player).is_file():
        problems.append(f"템플릿에 Player 실행 파일이 없습니다: {player!r}")
    if problems:
        raise BuildError("Player 템플릿 검증 실패 (" + str(template_dir) + "):\n  " + "\n  ".join(problems[:20])
                         + ("\n  ..." if len(problems) > 20 else ""))


# ── 씬 / 의존성 ──────────────────────────────────────────────────────────────

@dataclass
class Dependencies:
    files: Set[Path] = field(default_factory=set)        # 절대 경로
    errors: List[str] = field(default_factory=list)
    warnings: List[str] = field(default_factory=list)


def load_scene_document(path: Path) -> dict:
    try:
        doc = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        raise BuildError(f"씬 파일이 없습니다: {path}") from None
    except (OSError, json.JSONDecodeError) as e:
        raise BuildError(f"씬 파일을 읽을 수 없습니다: {path}: {e}") from None
    if not isinstance(doc, dict) or doc.get("format") != SCENE_FORMAT:
        raise BuildError(f"{path}: 씬 파일이 아닙니다(\"format\": \"{SCENE_FORMAT}\" 헤더가 없음). "
                         "에디터의 File > Save Scene으로 저장한 *.scene.json만 빌드할 수 있습니다.")
    version = doc.get("version")
    if not isinstance(version, int) or version > SCENE_VERSION:
        raise BuildError(f"{path}: 지원하지 않는 씬 버전입니다: {version!r}")
    if not isinstance(doc.get("entities"), list):
        raise BuildError(f"{path}: \"entities\" 배열이 없습니다")
    return doc


def _entity_label(entity: dict) -> str:
    return entity.get("name") or f"uuid {entity.get('uuid')}"


def _inside(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def collect_dependencies(project: Project, scene_path: Path) -> Dependencies:
    deps = Dependencies()
    scene_rel = project.relative(scene_path)
    doc = load_scene_document(scene_path)
    deps.files.add(scene_path.resolve())
    script_modules: Dict[str, str] = {}   # module_name -> 처음 참조한 엔티티

    for entity in doc["entities"]:
        comps = entity.get("components") or {}
        label = _entity_label(entity)
        for comp_name, fields in COMPONENT_PATH_FIELDS.items():
            comp = comps.get(comp_name)
            if not isinstance(comp, dict):
                continue
            for field_name, base in fields:
                value = comp.get(field_name)
                if not isinstance(value, str) or not value.strip():
                    continue
                where = f"{scene_rel}: 엔티티 '{label}' {comp_name}.{field_name} = '{value}'"
                if value.startswith("builtin:"):
                    continue          # Player 템플릿에 있는 엔진 내장 리소스(D5)
                root = project.root if base == "project" else project.assets_dir
                target = Path(value) if os.path.isabs(value) else root / value
                if not target.is_file():
                    deps.errors.append(f"{where} → 파일이 없습니다: {target}")
                    continue
                if not _inside(target, project.root):
                    deps.errors.append(f"{where} → 프로젝트 폴더 밖의 파일은 빌드에 넣을 수 없습니다: {target}")
                    continue
                deps.files.add(target.resolve())
                if comp_name == "ActionPlayerComponent":
                    _collect_action_clips(project, target, where, deps)

        script = comps.get("ScriptComponent")
        if isinstance(script, dict):
            module = (script.get("module_name") or "").strip()
            if module:
                script_modules.setdefault(module, label)

    for module, label in sorted(script_modules.items()):
        path = resolve_project_module(project, module)
        if path is None:
            deps.errors.append(f"{scene_rel}: 엔티티 '{label}' ScriptComponent.module_name = '{module}' → "
                               f"{project.scripts_dir}에서 모듈을 찾을 수 없습니다")
            continue
        _collect_script_imports(project, path, deps)
    return deps


def _collect_action_clips(project: Project, action_path: Path, where: str, deps: Dependencies) -> None:
    from core.action_data import load_action          # noqa: PLC0415 (Qt 없음)
    from core.sound_player import collect_clip_paths   # noqa: PLC0415 (PySide6 import는 선택적)
    try:
        action = load_action(str(action_path))
    except Exception as e:  # 형식 오류도 빌드 실패로 보고한다(배포 후에 발견되면 늦다)
        deps.errors.append(f"{where} → 액션 파일을 읽을 수 없습니다: {e}")
        return
    for clip in collect_clip_paths(action, str(action_path.parent)):
        clip_path = Path(clip)
        if not clip_path.is_file():
            deps.errors.append(f"{where} → 액션이 참조하는 클립이 없습니다: {clip_path}")
        elif not _inside(clip_path, project.root):
            deps.errors.append(f"{where} → 프로젝트 폴더 밖의 클립은 빌드에 넣을 수 없습니다: {clip_path}")
        else:
            # 게임 빌드 오디오는 v1 제외(SOUND_LITE_PLAN §6.1)지만, 파일은 넣어 둔다 — 나중에 오디오
            # 백엔드가 생기면 다시 빌드하지 않아도 되게.
            deps.files.add(clip_path.resolve())


def resolve_project_module(project: Project, module: str) -> Optional[Path]:
    """scripts 폴더에서 모듈 파일을 찾는다("a.b" → scripts/a/b.py 또는 scripts/a/b/__init__.py)."""
    base = project.scripts_dir.joinpath(*module.split("."))
    for candidate in (base.with_suffix(".py"), base / "__init__.py"):
        if candidate.is_file():
            return candidate
    return None


def _collect_script_imports(project: Project, entry: Path, deps: Dependencies) -> None:
    stdlib = set(getattr(sys, "stdlib_module_names", ())) | set(sys.builtin_module_names)
    pending = [entry]
    seen: Set[Path] = set()
    while pending:
        path = pending.pop().resolve()
        if path in seen:
            continue
        seen.add(path)
        deps.files.add(path)
        # 패키지 안의 모듈이면 상위 __init__.py도 넣는다(import 시 실행된다).
        parent = path.parent
        while _inside(parent, project.scripts_dir) and parent.resolve() != project.scripts_dir.resolve():
            init = parent / "__init__.py"
            if init.is_file():
                pending.append(init)
            parent = parent.parent
        try:
            tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
        except SyntaxError as e:
            deps.errors.append(f"스크립트 문법 오류: {path}: {e}")
            continue
        for node in ast.walk(tree):
            names: List[str] = []
            if isinstance(node, ast.Import):
                names = [a.name for a in node.names]
            elif isinstance(node, ast.ImportFrom):
                if node.level:      # 상대 import: 같은 패키지 안
                    pkg = path.parent
                    for _ in range(node.level - 1):
                        pkg = pkg.parent
                    mod = pkg.joinpath(*(node.module or "").split(".")) if node.module else pkg
                    for cand in (mod.with_suffix(".py"), mod / "__init__.py"):
                        if cand.is_file():
                            pending.append(cand)
                    for alias in node.names:
                        sub = mod / f"{alias.name}.py"
                        if sub.is_file():
                            pending.append(sub)
                    continue
                # "from pkg import mod"에서 mod가 하위 모듈일 수 있다(속성이 아니라 파일).
                names = [node.module or ""] + [f"{node.module}.{a.name}" for a in node.names if node.module]
            for name in names:
                top = name.split(".")[0]
                if not top or top in stdlib or top == "quarterflying":
                    continue
                found = resolve_project_module(project, name) or resolve_project_module(project, top)
                if found:
                    pending.append(found)
                    continue
                if isinstance(node, ast.ImportFrom) and name != (node.module or ""):
                    continue      # "from pkg import 이름"의 이름이 모듈이 아니라 속성인 경우 — 정상
                rel = project.relative(path) if _inside(path, project.root) else str(path)
                if top in ("PySide6", "PyQt5", "PyQt6", "shiboken6"):
                    deps.warnings.append(
                        f"{rel}: '{name}'을 import합니다. 게임 화면의 UI는 인게임 UI 컴포넌트로 만듭니다 — "
                        "에디터 GUI(PySide6)는 빌드 결과에 들어가지 않으므로 실행 시 ImportError가 납니다.")
                else:
                    deps.warnings.append(
                        f"{rel}: '{name}'을 import합니다. v1 빌드는 표준 라이브러리와 프로젝트 scripts/의 "
                        "모듈만 넣습니다 — 서드파티 패키지는 실행 시 ImportError가 납니다.")


# ── 조립 ─────────────────────────────────────────────────────────────────────

def _output_relpath(project: Project, path: Path) -> str:
    """프로젝트 파일 → 빌드 출력 폴더 안의 경로. 스크립트는 scripts/ 아래로 모은다(Player의 sys.path)."""
    if _inside(path, project.scripts_dir):
        return "scripts/" + path.resolve().relative_to(project.scripts_dir.resolve()).as_posix()
    return project.relative(path)


def check_forbidden(output_dir: Path) -> List[str]:
    bad = []
    for name in FORBIDDEN_TOP_LEVEL:
        if (output_dir / name).exists():
            bad.append(name + "/")
    for p in output_dir.rglob("*"):
        if p.name.startswith(FORBIDDEN_NAME_PREFIXES):
            bad.append(p.relative_to(output_dir).as_posix())
    return bad


def make_game_config(settings: BuildSettings, start_scene_out: str) -> dict:
    return {
        "version": GAME_CONFIG_VERSION,
        "title": settings.name,
        "window": {"width": settings.window_width, "height": settings.window_height,
                   "fullscreen": settings.fullscreen},
        "startScene": start_scene_out,
        "assetRoot": "./",
        "engine": {"numWorkerThreads": 0, "logFrameInterval": 0},
    }


def run_smoke_test(exe: Path, frames: int, timeout: float,
                   is_cancelled: Optional[CancelFn] = None) -> Tuple[bool, str]:
    """빌드한 게임을 --smoke-test로 실행한다. 기다리는 동안에도 취소할 수 있다(취소하면 프로세스를 끝내고
    BuildCancelled). 예전에는 subprocess.run으로 끝날 때까지 막혀서, 이 단계(최대 timeout초)에서는 취소
    버튼이 듣지 않았다."""
    import tempfile
    import time
    with tempfile.TemporaryFile() as out:
        try:
            proc = subprocess.Popen([str(exe), "--smoke-test", str(frames)], cwd=str(exe.parent),
                                    stdout=out, stderr=subprocess.STDOUT)
        except OSError as e:
            return False, f"실행할 수 없습니다: {e}"
        deadline = time.monotonic() + timeout
        while proc.poll() is None:
            if is_cancelled is not None and is_cancelled():
                proc.kill()
                proc.wait()
                raise BuildCancelled("빌드를 취소했습니다")
            if time.monotonic() > deadline:
                proc.kill()
                proc.wait()
                return False, f"{timeout:.0f}초 안에 끝나지 않았습니다"
            time.sleep(0.05)
        out.seek(0)
        log = out.read().decode("utf-8", errors="replace")
    return proc.returncode == 0, f"exit code {proc.returncode}\n{log}"


def build_game(settings: BuildSettings,
               progress: Optional[ProgressFn] = None,
               is_cancelled: Optional[CancelFn] = None) -> BuildReport:
    progress = progress or (lambda stage, fraction: None)
    is_cancelled = is_cancelled or (lambda: False)
    project = settings.project
    warnings: List[str] = []

    def step(stage: str, fraction: float) -> None:
        if is_cancelled():
            raise BuildCancelled("빌드를 취소했습니다")
        progress(stage, fraction)

    if project.is_sample:
        raise BuildError("샘플 프로젝트는 빌드할 수 없습니다. 프로젝트를 만들거나 여세요.")
    if project.layout.get("assets") != "assets":
        warnings.append("프로젝트의 assets 폴더 이름이 'assets'가 아닙니다. Player는 액션 경로를 "
                        "'<게임 폴더>/assets' 기준으로 풀므로 액션 재생이 어긋날 수 있습니다.")

    # 1. 템플릿
    step("템플릿 검증", 0.02)
    template = find_runtime_template(settings.template_dir)
    manifest = load_manifest(template)
    verify_template(template, manifest, current_python_abi(), current_engine_version())

    # 2~4. 씬과 의존성
    step("씬 검증", 0.15)
    if not settings.start_scene:
        raise BuildError("시작 씬이 정해지지 않았습니다. Build Settings에서 시작 씬을 고르거나 씬을 저장하세요.")
    start_scene = project.resolve(settings.start_scene)
    if not _inside(start_scene, project.root):
        raise BuildError(f"시작 씬이 프로젝트 폴더 밖에 있습니다: {start_scene}")

    step("의존성 수집", 0.25)
    deps = collect_dependencies(project, start_scene)
    for d in settings.include_dirs:
        folder = project.resolve(d)
        if not folder.is_dir() or not _inside(folder, project.root):
            deps.errors.append(f"포함 폴더가 없거나 프로젝트 밖에 있습니다: {d}")
            continue
        for p in folder.rglob("*"):
            if p.is_file() and "__pycache__" not in p.parts:
                deps.files.add(p.resolve())
    if deps.errors:
        raise BuildError("참조된 파일 문제로 빌드할 수 없습니다:\n  " + "\n  ".join(deps.errors))
    warnings.extend(deps.warnings)

    # 6. 조립 (임시 폴더 → 성공 시 교체)
    out_dir = Path(settings.out_dir)
    staging = out_dir.with_name(f".{out_dir.name}.building")
    if staging.exists():
        shutil.rmtree(staging)
    try:
        step("템플릿 복사", 0.35)
        player_name = manifest.get("player", "quarterflying_player.exe")
        shutil.copytree(template, staging, ignore=shutil.ignore_patterns(MANIFEST, "__pycache__"))
        exe = staging / f"{settings.name}.exe"
        os.replace(staging / player_name, exe)

        step("게임 파일 복사", 0.5)
        copied: List[str] = []
        files = sorted(deps.files)
        for i, src in enumerate(files):
            if is_cancelled():
                raise BuildCancelled("빌드를 취소했습니다")
            rel = _output_relpath(project, src)
            dst = staging / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            copied.append(rel)
            progress("게임 파일 복사", 0.5 + 0.2 * (i + 1) / max(1, len(files)))

        game_json = make_game_config(settings, _output_relpath(project, start_scene))
        (staging / "game.json").write_text(json.dumps(game_json, ensure_ascii=False, indent=2) + "\n",
                                           encoding="utf-8")

        step("Qt 파일 검사", 0.72)
        forbidden = check_forbidden(staging)
        if forbidden:
            raise BuildError("빌드 결과에 에디터/Qt 파일이 섞였습니다(배포하면 안 됨): " + ", ".join(forbidden))

        # 7. 자체 검증 — 출력 폴더와 다른 경로(임시 폴더)에서 실행하므로 경로가 코드에 박혀 있으면 걸린다.
        smoke_log = ""
        if settings.run_smoke_test:
            step("실행 검증(스모크 테스트)", 0.75)
            ok, smoke_log = run_smoke_test(exe, settings.smoke_test_frames, settings.smoke_test_timeout,
                                             is_cancelled)
            if not ok:
                tail = "\n".join(smoke_log.splitlines()[-40:])
                raise BuildError("빌드한 게임이 실행 검증을 통과하지 못했습니다:\n" + tail)

        step("출력 폴더 교체", 0.9)
        old = out_dir.with_name(f".{out_dir.name}.old")
        if old.exists():
            shutil.rmtree(old)
        if out_dir.exists():
            os.replace(out_dir, old)
        os.replace(staging, out_dir)
        if old.exists():
            shutil.rmtree(old, ignore_errors=True)
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise

    report = BuildReport(output_dir=out_dir, exe_path=out_dir / f"{settings.name}.exe",
                         content_files=copied, warnings=warnings, smoke_test_log=smoke_log)
    for p in out_dir.rglob("*"):
        if p.is_file():
            report.file_count += 1
            report.total_bytes += p.stat().st_size

    if settings.zip_output:
        step("zip 만들기", 0.95)
        report.zip_path = Path(shutil.make_archive(str(out_dir), "zip", root_dir=out_dir.parent,
                                                   base_dir=out_dir.name))
    progress("완료", 1.0)
    return report


def main(argv: Optional[List[str]] = None) -> int:
    import argparse
    from project import open_project
    ap = argparse.ArgumentParser(description="Quarter Flying Build Game (CLI)")
    ap.add_argument("project", help="프로젝트 폴더")
    ap.add_argument("--out", help="출력 폴더(기본: project.json의 build 설정)")
    ap.add_argument("--template", help="Player 템플릿 폴더")
    ap.add_argument("--no-smoke-test", action="store_true")
    args = ap.parse_args(argv)
    try:
        settings = BuildSettings.from_project(open_project(args.project))
        if args.out:
            settings.out_dir = Path(args.out).resolve()
        if args.template:
            settings.template_dir = Path(args.template)
        settings.run_smoke_test = not args.no_smoke_test
        report = build_game(settings, progress=lambda s, f: print(f"[{f * 100:5.1f}%] {s}"))
    except (BuildError, ProjectError) as e:
        print(f"BUILD FAILED: {e}", file=sys.stderr)
        return 1
    for w in report.warnings:
        print(f"WARNING: {w}")
    print(f"BUILD OK: {report.exe_path} ({report.file_count} files, {report.total_bytes / 1e6:.1f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
