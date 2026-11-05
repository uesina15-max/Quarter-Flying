"""
editor/project/session.py

프로젝트별 에디터 세션 — 프로젝트를 다시 열면 "하던 자리"(열어 둔 씬, Motion Editor에 열어 둔 액션,
에디터 카메라 위치, 모드)로 돌아오게 하는 파일. File > Save Project와 에디터 종료·프로젝트 전환 때 쓴다.

**PySide6를 import하지 않는다**(project.py와 같은 이유 - Qt 없이 단위 테스트한다).

위치: <프로젝트>/.quarterflying/session.json
  - project.json에 넣지 않는 이유: project.json은 게임 개발자가 버전 관리에 넣는 프로젝트 편집 설정이다
    (docs/USABILITY_IMPROVEMENT_PLAN.md R4). 열어 둔 씬·카메라 위치처럼 작업할 때마다 바뀌는 값을 넣으면
    커밋마다 diff가 생기고, 여러 사람이 같은 프로젝트를 쓰면 서로 덮어쓴다(Visual Studio의 .vs/와 같은 이유).
  - config.json(에디터 설정)에 넣지 않는 이유: 프로젝트마다 값이 다르다. 프로젝트 폴더를 옮기거나 다른 PC에서
    열어도 같이 따라가야 한다.
  - Build Game은 씬에서 출발한 의존성과 build.includeDirs만 복사하므로(gamebuild/game_builder.py) 이 폴더는
    빌드 출력에 들어가지 않는다. 샘플 프로젝트(engine/)도 같은 위치를 쓰고, 저장소 .gitignore가 이 폴더를 뺀다.

경로 규칙: 프로젝트 안의 파일은 프로젝트 상대 경로("scenes/Main.scene.json")로 적는다 - 프로젝트 폴더를 통째로
옮겨도 세션이 맞는다. 프로젝트 밖의 파일(다른 폴더에서 연 액션 등)만 절대 경로로 적는다.

project.json과 달리 **읽기는 실패하지 않는다**. 세션은 편의 데이터라, 깨진 세션 파일 때문에 프로젝트를 못 여는
일이 있으면 안 된다. 형식이 틀린 필드는 기본값으로 두고 이유를 warnings로 돌려준다(호출부가 로그로 남긴다 -
조용히 버리지 않는다, CLAUDE.md 관례 3).
"""

from __future__ import annotations

import json
import math
import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional, Tuple

SESSION_DIRNAME = ".quarterflying"
SESSION_FILENAME = "session.json"
SESSION_FORMAT = "quarterflying.editor-session"
SESSION_VERSION = 1

# 복원하는 에디터 모드(main.py의 QStackedWidget 페이지 번호): 0 = Scene Editor, 2 = Motion Editor.
# Play(1)는 복원하지 않는다 - 에디터를 열자마자 게임이 돌면 안 되고, Play 중의 월드는 저장 대상도 아니다.
MODE_SCENE = 0
MODE_MOTION = 2
RESTORABLE_MODES = (MODE_SCENE, MODE_MOTION)


@dataclass
class CameraView:
    """에디터 오빗 카메라(core/editor_camera.py OrbitCamera)의 상태."""

    target: List[float] = field(default_factory=lambda: [0.0, 0.0, 0.0])
    distance: float = 10.0
    yaw: float = 0.0
    pitch: float = 0.0

    def to_dict(self) -> dict:
        return {"target": list(self.target), "distance": self.distance, "yaw": self.yaw, "pitch": self.pitch}


@dataclass
class EditorSession:
    scene: str = ""                                       # 열어 둔 씬 파일(저장된 적 없는 새 씬이면 "")
    actions: List[str] = field(default_factory=list)      # Motion Editor에 열어 둔 액션 파일들(목록 순서)
    selected_action: str = ""                             # 그중 선택했던 것
    mode: int = MODE_SCENE
    camera: Optional[CameraView] = None                   # None이면 카메라 기본 위치

    def to_dict(self) -> dict:
        return {
            "format": SESSION_FORMAT,
            "version": SESSION_VERSION,
            "scene": self.scene,
            "actions": list(self.actions),
            "selectedAction": self.selected_action,
            "mode": self.mode,
            "camera": self.camera.to_dict() if self.camera is not None else None,
        }


def session_file(project) -> Path:
    return project.root / SESSION_DIRNAME / SESSION_FILENAME


# ── 경로 ─────────────────────────────────────────────────────────────────────

def to_session_path(project, path) -> str:
    """세션에 적을 경로. 프로젝트 안이면 프로젝트 상대 경로, 밖이면 절대 경로("/" 구분)."""
    p = Path(path).resolve()
    try:
        return p.relative_to(project.root.resolve()).as_posix()
    except ValueError:
        return p.as_posix()


def from_session_path(project, value: str) -> Path:
    p = Path(value)
    return p if p.is_absolute() else project.resolve(value)


# ── 읽기·쓰기 ────────────────────────────────────────────────────────────────

def _is_number(v) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def _parse_camera(raw, where: str, warnings: List[str]) -> Optional[CameraView]:
    if raw is None:
        return None
    if not isinstance(raw, dict):
        warnings.append(f"{where}: \"camera\" must be an object or null - using the default camera")
        return None
    target = raw.get("target")
    distance, yaw, pitch = raw.get("distance"), raw.get("yaw"), raw.get("pitch")
    if not (isinstance(target, list) and len(target) == 3 and all(_is_number(v) for v in target)):
        warnings.append(f"{where}: \"camera.target\" must be 3 numbers - using the default camera")
        return None
    if not (_is_number(distance) and distance > 0 and _is_number(yaw) and _is_number(pitch)):
        warnings.append(f"{where}: \"camera.distance/yaw/pitch\" must be numbers (distance > 0) - "
                        "using the default camera")
        return None
    return CameraView(target=[float(v) for v in target], distance=float(distance), yaw=float(yaw), pitch=float(pitch))


def load_session(project) -> Tuple[EditorSession, List[str]]:
    """(세션, 경고 목록). 파일이 없으면 빈 세션과 빈 경고. 예외를 내지 않는다(모듈 주석)."""
    file = session_file(project)
    where = str(file)
    session = EditorSession()
    warnings: List[str] = []
    if not file.is_file():
        return session, warnings
    try:
        with open(file, "r", encoding="utf-8") as f:
            data = json.load(f)
    except (OSError, UnicodeDecodeError, json.JSONDecodeError) as e:
        return session, [f"{where}: cannot read session ({type(e).__name__}: {e}) - starting without it"]
    if not isinstance(data, dict) or data.get("format") != SESSION_FORMAT:
        return session, [f"{where}: missing or wrong \"format\" (expected \"{SESSION_FORMAT}\") - starting without it"]
    version = data.get("version")
    if not isinstance(version, int) or isinstance(version, bool) or version > SESSION_VERSION:
        # 새 에디터가 쓴 세션일 수 있다. 모르는 의미로 읽느니 버린다(덮어쓰는 것은 다음 저장 때다).
        return session, [f"{where}: unsupported session version {version!r} (this editor: {SESSION_VERSION}) "
                         "- starting without it"]

    scene = data.get("scene", "")
    if isinstance(scene, str):
        session.scene = scene
    else:
        warnings.append(f"{where}: \"scene\" must be a string - ignored")

    actions = data.get("actions", [])
    if isinstance(actions, list) and all(isinstance(a, str) for a in actions):
        session.actions = list(actions)
    else:
        warnings.append(f"{where}: \"actions\" must be a list of strings - ignored")

    selected = data.get("selectedAction", "")
    if isinstance(selected, str):
        session.selected_action = selected
    else:
        warnings.append(f"{where}: \"selectedAction\" must be a string - ignored")

    mode = data.get("mode", MODE_SCENE)
    if isinstance(mode, int) and not isinstance(mode, bool) and mode in RESTORABLE_MODES:
        session.mode = mode
    else:
        warnings.append(f"{where}: \"mode\" must be one of {list(RESTORABLE_MODES)} - using Scene Editor")

    session.camera = _parse_camera(data.get("camera"), where, warnings)
    return session, warnings


def save_session(project, session: EditorSession) -> Path:
    """세션 파일을 쓴다(임시 파일 → 교체). 실패하면 OSError를 그대로 낸다 - 호출부가 출처 태그로 로그한다."""
    file = session_file(project)
    file.parent.mkdir(parents=True, exist_ok=True)
    tmp = file.with_name(file.name + ".tmp")
    with open(tmp, "w", encoding="utf-8", newline="\n") as f:
        json.dump(session.to_dict(), f, ensure_ascii=False, indent=2)
        f.write("\n")
    os.replace(tmp, file)
    return file
