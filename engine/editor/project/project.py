"""
editor/project/project.py

프로젝트 폴더 — 게임 개발자의 씬·에셋·스크립트가 있는 곳. docs/GAME_BUILD_PLAN.md D4, Phase 1.

**PySide6를 import하지 않는다.** Build Game 로직(editor/build/game_builder.py)도 이 모듈을 쓰는데,
그 로직은 Qt 없이 단위 테스트되고 CI에서도 돌아야 한다(D3).

폴더 구성(§3.2):

    MyProject/
    ├─ project.json
    ├─ assets/        models/ textures/ fonts/ prefabs/ actions/ sfx/ motion/ input/ materials/
    ├─ scenes/        *.scene.json
    └─ scripts/       ScriptComponent.module_name이 가리키는 .py

경로 규칙(엔진과 같다):
  - RenderableComponent.meshPath/texturePath는 **프로젝트 폴더 기준**이다("assets/models/cube.obj").
    그래서 엔진의 에셋 기준 폴더(EngineConfig.assetRoot)에는 프로젝트 폴더를 넣는다.
  - ActionPlayerComponent.action의 상대 경로는 **assets 폴더 기준**이다(에디터 main.py의 Play 처리와 같음).

프로젝트를 열지 않았을 때는 "샘플 프로젝트"(= 이 저장소의 engine/ 폴더, 에셋은 engine/assets)를 쓴다.
그래야 프로젝트 개념이 생기기 전의 개발 흐름(데모 씬 등)이 그대로 유지된다.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional

PROJECT_FILE = "project.json"
PROJECT_FORMAT = "quarterflying.project"
PROJECT_VERSION = 1

SCENE_FORMAT = "quarterflying.scene"      # engine/scene/SceneIO.h kSceneFormatName
SCENE_VERSION = 2                         # engine/scene/SceneIO.h kSceneFormatVersion (2: 프리팹 경로가 프로젝트 상대)
SCENE_SUFFIX = ".scene.json"

DEFAULT_LAYOUT = {"assets": "assets", "scenes": "scenes", "scripts": "scripts"}
ASSET_SUBDIRS = ("models", "textures", "fonts", "prefabs", "actions", "sfx", "motion", "input", "materials")

# 입력 액션 맵(docs/USABILITY_IMPROVEMENT_PLAN.md G0a/G0b). 형식은 엔진(input/InputActions.h)만 안다 -
# 여기서는 경로와 "빈 맵" 파일만 다룬다. 위치가 <프로젝트>/input/이 아니라 assets/input/인 이유: 샘플 프로젝트의
# 루트는 engine/이고 engine/input/은 C++ 소스 폴더라서, 거기에 프로젝트 데이터를 두면 소스와 섞인다.
INPUT_ACTIONS_RELPATH = "input/actions.json"     # assets 폴더 기준
INPUT_ACTIONS_FORMAT = "quarterflying.input-actions"
INPUT_ACTIONS_VERSION = 1

# 이 파일: engine/editor/project/project.py → 샘플 프로젝트 루트는 engine/
_ENGINE_DIR = Path(__file__).resolve().parents[2]


class ProjectError(Exception):
    """프로젝트 열기/만들기/저장 실패. 메시지에 원인(어느 파일, 어느 키)을 담는다."""


@dataclass
class BuildConfig:
    """project.json의 "build" 절. Build Game 대화상자가 편집하고 game_builder가 읽는다."""

    name: str = ""                      # 빌드 결과 실행 파일 이름(확장자 없이). 비면 프로젝트 이름
    start_scene: str = ""               # 프로젝트 상대 경로. 비면 Project.start_scene
    window_width: int = 1280
    window_height: int = 720
    fullscreen: bool = False
    cursor_lock: bool = False           # 게임 시작 시 커서 잠금 요청(game.json window.cursorLock, 스크립트로 바꿀 수 있음)
    include_dirs: List[str] = field(default_factory=list)   # 자동 수집과 무관하게 통째로 넣을 폴더
    output_dir: str = ""                # 비면 <프로젝트>/Build

    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "startScene": self.start_scene,
            "window": {"width": self.window_width, "height": self.window_height, "fullscreen": self.fullscreen,
                       "cursorLock": self.cursor_lock},
            "includeDirs": list(self.include_dirs),
            "outputDir": self.output_dir,
        }

    @classmethod
    def from_dict(cls, data: dict, where: str) -> "BuildConfig":
        if not isinstance(data, dict):
            raise ProjectError(f"{where}: \"build\" must be an object")
        cfg = cls()
        cfg.name = _get(data, "name", str, cfg.name, where)
        cfg.start_scene = _get(data, "startScene", str, cfg.start_scene, where)
        window = data.get("window", {})
        if not isinstance(window, dict):
            raise ProjectError(f"{where}: \"build.window\" must be an object")
        cfg.window_width = _get(window, "width", int, cfg.window_width, where, "build.window.")
        cfg.window_height = _get(window, "height", int, cfg.window_height, where, "build.window.")
        cfg.fullscreen = _get(window, "fullscreen", bool, cfg.fullscreen, where, "build.window.")
        cfg.cursor_lock = _get(window, "cursorLock", bool, cfg.cursor_lock, where, "build.window.")
        dirs = data.get("includeDirs", [])
        if not isinstance(dirs, list) or not all(isinstance(d, str) for d in dirs):
            raise ProjectError(f"{where}: \"build.includeDirs\" must be a list of strings")
        cfg.include_dirs = list(dirs)
        cfg.output_dir = _get(data, "outputDir", str, cfg.output_dir, where)
        return cfg


def _get(data: dict, key: str, typ, default, where: str, prefix: str = ""):
    """키가 있으면 타입을 검사해 돌려준다. 타입이 틀리면 조용히 기본값으로 바꾸지 않고 에러를 낸다."""
    if key not in data:
        return default
    value = data[key]
    # bool은 int의 하위 타입이라 따로 막는다(width: true 같은 실수).
    if typ is int and (isinstance(value, bool) or not isinstance(value, int)):
        raise ProjectError(f"{where}: \"{prefix}{key}\" must be an integer")
    if typ is not int and not isinstance(value, typ):
        raise ProjectError(f"{where}: \"{prefix}{key}\" must be {typ.__name__}")
    return value


@dataclass
class Project:
    root: Path
    name: str
    start_scene: str = ""                               # 프로젝트 상대 경로("scenes/Main.scene.json")
    layout: Dict[str, str] = field(default_factory=lambda: dict(DEFAULT_LAYOUT))
    build: BuildConfig = field(default_factory=BuildConfig)
    is_sample: bool = False
    engine_version: str = ""

    # ── 폴더 ─────────────────────────────────────────────────────────────────

    @property
    def project_file(self) -> Path:
        return self.root / PROJECT_FILE

    @property
    def assets_dir(self) -> Path:
        return self.root / self.layout["assets"]

    @property
    def scenes_dir(self) -> Path:
        return self.root / self.layout["scenes"]

    @property
    def scripts_dir(self) -> Path:
        return self.root / self.layout["scripts"]

    @property
    def prefabs_dir(self) -> Path:
        return self.assets_dir / "prefabs"

    @property
    def motion_dir(self) -> Path:
        return self.assets_dir / "motion"

    @property
    def actions_dir(self) -> Path:
        return self.assets_dir / "actions"

    @property
    def input_actions_path(self) -> Path:
        return self.assets_dir / INPUT_ACTIONS_RELPATH

    # ── 경로 변환 ────────────────────────────────────────────────────────────

    def resolve(self, relative: str) -> Path:
        """프로젝트 상대 경로 → 절대 경로."""
        return (self.root / relative).resolve()

    def relative(self, path) -> str:
        """절대 경로 → 프로젝트 상대 경로("/" 구분). 프로젝트 밖이면 ProjectError."""
        p = Path(path).resolve()
        try:
            return p.relative_to(self.root.resolve()).as_posix()
        except ValueError:
            raise ProjectError(f"'{p}' is outside the project folder '{self.root}'") from None

    def list_scenes(self) -> List[str]:
        """scenes 폴더의 씬 파일(프로젝트 상대 경로) 목록."""
        if not self.scenes_dir.is_dir():
            return []
        return sorted(self.relative(p) for p in self.scenes_dir.rglob(f"*{SCENE_SUFFIX}"))

    @property
    def start_scene_path(self) -> Optional[Path]:
        return self.resolve(self.start_scene) if self.start_scene else None

    # ── 저장 ─────────────────────────────────────────────────────────────────

    def to_dict(self) -> dict:
        return {
            "format": PROJECT_FORMAT,
            "version": PROJECT_VERSION,
            "name": self.name,
            "engineVersion": self.engine_version,
            "startScene": self.start_scene,
            "layout": dict(self.layout),
            "build": self.build.to_dict(),
        }

    def save(self) -> None:
        if self.is_sample:
            # 샘플 프로젝트(engine/)에 project.json을 만들면 저장소에 사용자 설정이 섞인다.
            raise ProjectError("the sample project has no project.json; create or open a project first")
        _write_json_atomic(self.project_file, self.to_dict())


def _write_json_atomic(path: Path, data: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    tmp = path.with_name(path.name + ".tmp")
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=2)
        f.write("\n")
    os.replace(tmp, path)


def empty_scene_document() -> dict:
    """엔티티가 없는 씬 문서. 엔진 없이도 만들 수 있어야 해서 SceneIO와 같은 헤더를 여기서 쓴다."""
    return {"format": SCENE_FORMAT, "version": SCENE_VERSION, "entities": []}


def default_scene_document() -> dict:
    """새 프로젝트의 시작 씬: Main Camera 하나.

    예전에는 빈 씬을 만들어서, 새 프로젝트를 열면 카메라가 없었다(게임 화면이 기본 카메라 위치로만
    그려지고 Player는 "no main camera" 오류를 남긴다). 필드 이름은 리플렉션 등록 이름과 같다
    (engine/ecs/Reflection.cpp RegisterPODComponentsReflection).
    """
    import secrets
    doc = empty_scene_document()
    doc["entities"].append({
        "uuid": secrets.randbits(62) + 1,
        "name": "Main Camera",
        "components": {
            "TransformComponent": {"position": [0.0, 5.0, 15.0], "rotation": [-18.0, 0.0, 0.0],
                                   "scale": [1.0, 1.0, 1.0]},
            "CameraComponent": {"fov": 60.0, "nearPlane": 0.1, "farPlane": 1000.0,
                                "isMainCamera": True, "priority": 0, "blendInSeconds": 0.0},
        },
    })
    return doc


def create_project(path, name: Optional[str] = None) -> Project:
    """새 프로젝트 폴더를 만든다. 폴더가 이미 있고 비어 있지 않으면 거절한다(남의 파일을 덮지 않게)."""
    root = Path(path).resolve()
    if root.exists() and (not root.is_dir() or any(root.iterdir())):
        raise ProjectError(f"'{root}' already exists and is not an empty folder")
    project = Project(root=root, name=name or root.name, start_scene=f"scenes/Main{SCENE_SUFFIX}")
    for sub in ASSET_SUBDIRS:
        (project.assets_dir / sub).mkdir(parents=True, exist_ok=True)
    project.scenes_dir.mkdir(parents=True, exist_ok=True)
    project.scripts_dir.mkdir(parents=True, exist_ok=True)
    _write_json_atomic(project.resolve(project.start_scene), default_scene_document())
    # 빈 액션 맵. 액션 이름과 키 배치는 게임 개발자가 정한다(엔진이 기본 액션을 정하지 않는다).
    _write_json_atomic(project.input_actions_path, {
        "format": INPUT_ACTIONS_FORMAT, "version": INPUT_ACTIONS_VERSION, "actions": {}})
    project.save()
    return project


def open_project(path) -> Project:
    """프로젝트 폴더(또는 그 안의 project.json)를 연다."""
    p = Path(path)
    file = p if p.name == PROJECT_FILE else p / PROJECT_FILE
    if not file.is_file():
        raise ProjectError(f"'{file}' not found — not a Quarter Flying project folder")
    where = str(file)
    try:
        with open(file, "r", encoding="utf-8") as f:
            data = json.load(f)
    except json.JSONDecodeError as e:
        raise ProjectError(f"{where}: JSON syntax error: {e}") from None
    if not isinstance(data, dict) or data.get("format") != PROJECT_FORMAT:
        raise ProjectError(f"{where}: missing or wrong \"format\" (expected \"{PROJECT_FORMAT}\")")
    version = data.get("version")
    if not isinstance(version, int) or isinstance(version, bool):
        raise ProjectError(f"{where}: \"version\" must be an integer")
    if version > PROJECT_VERSION:
        raise ProjectError(f"{where}: project version {version} is newer than this editor supports ({PROJECT_VERSION})")

    layout = dict(DEFAULT_LAYOUT)
    raw_layout = data.get("layout", {})
    if not isinstance(raw_layout, dict):
        raise ProjectError(f"{where}: \"layout\" must be an object")
    for key in DEFAULT_LAYOUT:
        layout[key] = _get(raw_layout, key, str, layout[key], where, "layout.")

    return Project(
        root=file.parent.resolve(),
        name=_get(data, "name", str, file.parent.name, where),
        start_scene=_get(data, "startScene", str, "", where),
        layout=layout,
        build=BuildConfig.from_dict(data.get("build", {}), where),
        engine_version=_get(data, "engineVersion", str, "", where),
    )


# 샘플 프로젝트의 시작 씬(docs/USABILITY_IMPROVEMENT_PLAN.md X-1b). 예전에는 데모 월드를 에디터가 시작할 때마다
# 코드(demo_scene_seed.py)와 scene.json으로 만들었다. 그래서 VFX·카메라 수치를 바꾸려면 Python을 고쳐야 했고,
# 같은 월드를 Player로 빌드할 방법이 없었다. 지금은 보통의 씬 파일이다 - 코드 시드는 이 파일이 없을 때의 대체 경로다.
SAMPLE_START_SCENE = "assets/scenes/Demo.scene.json"


def sample_project() -> Project:
    """프로젝트를 열지 않았을 때 쓰는 샘플 프로젝트(= engine/). 기존 개발 흐름을 그대로 유지한다."""
    return Project(
        root=_ENGINE_DIR,
        name="Sample",
        start_scene=SAMPLE_START_SCENE,
        layout={"assets": "assets", "scenes": "assets/scenes", "scripts": "assets/scripts"},
        is_sample=True,
    )


# ── 현재 프로젝트 ────────────────────────────────────────────────────────────
# 패널들은 경로를 모듈 상수로 들고 있지 않고, 쓰는 순간 여기서 받는다. 예전에는 패널마다
# engine/assets 기준 경로를 import 시점에 계산해 두어서(§1.2 항목 3의 7곳), 다른 프로젝트를
# 열 방법이 없었다.

_current: Optional[Project] = None


def current_project() -> Project:
    global _current
    if _current is None:
        _current = sample_project()
    return _current


def set_current_project(project: Project) -> None:
    global _current
    _current = project
