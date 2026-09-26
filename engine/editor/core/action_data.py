"""
editor/core/action_data.py

ActionData — Motion Editor 핵심 데이터 구조.

규칙:
  - 이 파일에서만 직렬화/역직렬화를 담당한다.
  - 다른 패널은 ActionData 객체만 받는다.
  - Enum을 쓴다. str 비교를 쓰지 않는다.
"""

from __future__ import annotations

import json
import os
from dataclasses import dataclass, field, asdict
from enum import Enum
from typing import List, Optional


# ─────────────────────────────────────────────────────────────────────────────
# Enums
# ─────────────────────────────────────────────────────────────────────────────

class EventType(str, Enum):
    """이벤트 종류. 여기 있는 것만 쓴다. 무한 확장하지 않는다."""
    HIT          = "Hit"
    EFFECT       = "Effect"
    SOUND        = "Sound"
    CAMERA_SHAKE = "CameraShake"


class SectionRole(str, Enum):
    """섹션의 의미론적 역할. 이름은 자유, 역할은 이것 중 하나."""
    WIND_UP  = "WindUp"
    ATTACK   = "Attack"
    RECOVERY = "Recovery"
    LOOP     = "Loop"
    CHARGE   = "Charge"


# SectionRole → UI 표시 색상 (QColor hex)
SECTION_ROLE_COLORS: dict[SectionRole, str] = {
    SectionRole.WIND_UP:  "#2a4a8a",   # 파랑
    SectionRole.ATTACK:   "#8a2a2a",   # 빨강
    SectionRole.RECOVERY: "#2a6a3a",   # 초록
    SectionRole.LOOP:     "#5a4a2a",   # 황갈
    SectionRole.CHARGE:   "#5a2a6a",   # 보라
}

# EventType → UI 마커 색상
EVENT_TYPE_COLORS: dict[EventType, str] = {
    EventType.HIT:          "#ff4444",
    EventType.EFFECT:       "#aa44ff",
    EventType.SOUND:        "#44aaff",
    EventType.CAMERA_SHAKE: "#ffcc00",
}


# ─────────────────────────────────────────────────────────────────────────────
# Dataclasses
# ─────────────────────────────────────────────────────────────────────────────

@dataclass
class ActionSection:
    """하나의 모션 구간."""
    name:        str         # 자유 이름: "ComboA", "Uppercut", "Recover" 등
    role:        SectionRole # 의미론적 역할
    start_frame: int
    end_frame:   int
    speed:       float = 1.0  # Retiming 배율 (0.5 = 50%, 1.2 = 120%)

    @property
    def duration(self) -> int:
        return max(0, self.end_frame - self.start_frame)


@dataclass
class ActionEvent:
    """특정 프레임에 발생하는 게임플레이 이벤트."""
    frame:  int
    type:   EventType
    params: dict = field(default_factory=dict)


@dataclass
class AnimationLayer:
    """애니메이션 믹서 레이어 하나."""
    name:      str
    clip_name: str
    weight:    float = 1.0   # 0.0 ~ 1.0
    mask:      str   = "Full"  # "Full" / "UpperBody" / "LowerBody"
    speed:     float = 1.0   # 레이어 재생 속도 배율


@dataclass
class ActionData:
    """
    하나의 액션(공격/스킬/이동 등)에 대한 모든 데이터.
    에디터는 이 객체를 편집하고, .action.json 으로 저장한다.
    """
    name:         str
    fps:          int  = 30
    total_frames: int  = 60

    sections: List[ActionSection]  = field(default_factory=list)
    events:   List[ActionEvent]    = field(default_factory=list)
    layers:   List[AnimationLayer] = field(default_factory=list)

    # 이 액션이 저장/로드된 파일 경로. **직렬화하지 않는다**(to_dict에 없음) - 파일
    # 안에 자기 경로를 적어두면 파일을 옮기는 순간 거짓말이 되기 때문이다.
    #
    # Sound Lite가 이 값을 쓴다: params.clip이 `.action.json` 위치 기준 상대 경로라
    # (docs/SOUND_LITE_PLAN.md §6.4) 클립을 로드하려면 기준 폴더를 알아야 하는데,
    # 액션 객체가 자기 출처를 들고 있으면 패널마다 경로를 따로 배선할 필요가 없다.
    # 아직 한 번도 저장되지 않은 액션은 None이고, 그 경우 clip은 절대 경로로 둔다.
    source_path: Optional[str] = None

    # ── 직렬화 ───────────────────────────────────────────────────────────────

    def to_dict(self) -> dict:
        return {
            "name":         self.name,
            "fps":          self.fps,
            "total_frames": self.total_frames,
            "sections": [
                {
                    "name":        s.name,
                    "role":        s.role.value,
                    "start_frame": s.start_frame,
                    "end_frame":   s.end_frame,
                    "speed":       s.speed,
                }
                for s in self.sections
            ],
            "events": [
                {
                    "frame":  e.frame,
                    "type":   e.type.value,
                    "params": e.params,
                }
                for e in self.events
            ],
            "layers": [
                {
                    "name":      l.name,
                    "clip_name": l.clip_name,
                    "weight":    l.weight,
                    "mask":      l.mask,
                    "speed":     l.speed,
                }
                for l in self.layers
            ],
        }

    def to_json(self, indent: int = 2) -> str:
        return json.dumps(self.to_dict(), ensure_ascii=False, indent=indent)

    @staticmethod
    def from_dict(d: dict) -> "ActionData":
        sections = [
            ActionSection(
                name        = s["name"],
                role        = SectionRole(s["role"]),
                start_frame = s["start_frame"],
                end_frame   = s["end_frame"],
                speed       = s.get("speed", 1.0),
            )
            for s in d.get("sections", [])
        ]
        events = [
            ActionEvent(
                frame  = e["frame"],
                type   = EventType(e["type"]),
                params = e.get("params", {}),
            )
            for e in d.get("events", [])
        ]
        layers = [
            AnimationLayer(
                name      = l["name"],
                clip_name = l["clip_name"],
                weight    = l.get("weight", 1.0),
                mask      = l.get("mask", "Full"),
                speed     = l.get("speed", 1.0),
            )
            for l in d.get("layers", [])
        ]
        return ActionData(
            name         = d["name"],
            fps          = d.get("fps", 30),
            total_frames = d.get("total_frames", 60),
            sections     = sections,
            events       = events,
            layers       = layers,
        )

    @staticmethod
    def from_json(json_str: str) -> "ActionData":
        return ActionData.from_dict(json.loads(json_str))


# ─────────────────────────────────────────────────────────────────────────────
# File I/O helpers
# ─────────────────────────────────────────────────────────────────────────────

def relativize_clip_paths(data: ActionData, base_dir: str) -> None:
    """Sound 이벤트의 절대 clip 경로를 base_dir 기준 상대 경로로 바꾼다.

    docs/SOUND_LITE_PLAN.md §6.4. 클립을 고르는 시점에는 어느 폴더에 저장될지 모르므로
    일단 절대 경로로 들고 있다가, **저장 시점에** 여기서 상대 경로로 계산한다.

    다른 드라이브에 있는 클립처럼 상대 경로를 만들 수 없으면 절대 경로를 그대로 둔다 -
    깨진 상대 경로를 쓰느니 옮길 때 깨지는 절대 경로가 낫다.
    """
    for e in data.events:
        if e.type != EventType.SOUND:
            continue
        clip = (e.params or {}).get("clip")
        if not clip or not os.path.isabs(clip):
            continue
        try:
            e.params["clip"] = os.path.relpath(clip, base_dir).replace(os.sep, "/")
        except ValueError:
            pass  # 예: Windows에서 드라이브가 다른 경우


def save_action(data: ActionData, filepath: str) -> None:
    """ActionData를 .action.json 파일로 저장."""
    abs_path = os.path.abspath(filepath)
    os.makedirs(os.path.dirname(abs_path), exist_ok=True)

    # 저장될 폴더가 확정된 지금이 clip 경로를 상대화할 유일한 시점이다(§6.4).
    relativize_clip_paths(data, os.path.dirname(abs_path))

    with open(filepath, "w", encoding="utf-8") as f:
        f.write(data.to_json())
    data.source_path = abs_path


def load_action(filepath: str) -> ActionData:
    """파일에서 ActionData 로드."""
    with open(filepath, "r", encoding="utf-8") as f:
        action = ActionData.from_json(f.read())
    # clip의 상대 경로를 풀 기준 폴더를 액션 자신이 들고 있게 한다(§6.4).
    action.source_path = os.path.abspath(filepath)
    return action


# ─────────────────────────────────────────────────────────────────────────────
# Factory helpers
# ─────────────────────────────────────────────────────────────────────────────

def make_default_action(name: str = "NewAction", total_frames: int = 60) -> ActionData:
    """기본 섹션 3개가 있는 빈 ActionData 생성."""
    third = total_frames // 3
    return ActionData(
        name         = name,
        fps          = 30,
        total_frames = total_frames,
        sections     = [
            ActionSection("WindUp",   SectionRole.WIND_UP,  0,          third,          0.8),
            ActionSection("Attack",   SectionRole.ATTACK,   third,      third * 2,      1.2),
            ActionSection("Recovery", SectionRole.RECOVERY, third * 2,  total_frames,   0.7),
        ],
        layers = [
            AnimationLayer("Base", "", weight=1.0, mask="Full", speed=1.0),
        ],
    )
