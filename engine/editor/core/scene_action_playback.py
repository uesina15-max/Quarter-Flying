"""
editor/core/scene_action_playback.py

Scene Editor의 Play에서 액션을 재생한다.
docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.4 / §3 Phase 6.

범위 정의서 §6.2가 **(b) 파이썬이 액션을 재생한다**로 확정됐기 때문에 필요한 모듈이다 -
엔진에는 Action이라는 개념이 없고(§1.2), 만들지 않기로 했다.

Qt를 모른다. registry는 GetAllEntities()/GetComponentJson()만 쓰므로 테스트에서
가짜 객체로 대체할 수 있고, 그래서 엔진 없이 유닛테스트로 닫힌다.

**발화 규칙은 여기 없다.** Motion Editor와 이 모듈은 같은 ActionPlaybackCursor를
공유한다 - 재생 루프가 둘이 되는 것은 어쩔 수 없어도 규칙까지 둘이 되면 한쪽만
고쳐지는 버그가 반드시 생기기 때문이다(§6.2.2).
"""

from __future__ import annotations

import json
import os
from typing import Callable, List, Optional

from core.action_data import ActionData, EventType, load_action
from core.action_playback import ActionPlaybackCursor, events_of_type
from core.sound_player import SoundPlayer, collect_clip_paths

COMPONENT_NAME = "ActionPlayerComponent"


class _Entry:
    """재생 중인 엔티티 하나."""

    __slots__ = ("entity_id", "cursor", "fps", "accumulator")

    def __init__(self, entity_id: int, cursor: ActionPlaybackCursor, fps: int):
        self.entity_id = entity_id
        self.cursor = cursor
        # 액션마다 fps가 다르다. 틱 수가 아니라 경과 시간으로 프레임을 전진시키는 데 쓴다.
        self.fps = fps if fps > 0 else 30
        # 프레임의 소수부 누적기. dt가 작을 때 매번 버림하면 재생이 아예 멈춘다
        # (VFX Lite의 스폰 누적기와 같은 이유).
        self.accumulator = 0.0


class ScenePlaybackController:
    """ActionPlayerComponent를 가진 엔티티들의 액션을 Play 동안 재생한다."""

    def __init__(self,
                 sound_player: Optional[SoundPlayer] = None,
                 action_loader: Callable[[str], ActionData] = load_action):
        self._sound = sound_player if sound_player is not None else SoundPlayer()
        self._load_action = action_loader
        self._entries: List[_Entry] = []

    # ── 상태 ─────────────────────────────────────────────────────────────────

    @property
    def active_count(self) -> int:
        return len(self._entries)

    @property
    def sound_player(self) -> SoundPlayer:
        return self._sound

    # ── 수명주기 ─────────────────────────────────────────────────────────────

    def start(self, registry, base_dir: Optional[str] = None) -> int:
        """Play 진입 시. 커서를 만들고 클립을 프리로드한다. 시작된 엔티티 수를 돌려준다.

        base_dir는 ActionPlayerComponent.action이 상대 경로일 때의 기준이다. 액션 파일
        자체가 참조하는 clip 경로는 그 액션 파일 위치 기준으로 따로 풀린다(§6.4) -
        두 기준을 섞지 않는다.
        """
        self.stop()

        for entity_id, comp in self._iter_action_players(registry):
            if not comp.get("playOnStart", True):
                continue

            path = (comp.get("action") or "").strip()
            if not path:
                continue
            if base_dir and not os.path.isabs(path):
                path = os.path.normpath(os.path.join(base_dir, path))

            try:
                action = self._load_action(path)
            except Exception as e:
                # 액션 하나가 깨져도 나머지 재생을 막지 않는다(§5.6과 같은 정신).
                print(f"[ScenePlayback] 액션 로드 실패(건너뜀): {path}: {e}")
                continue

            cursor = ActionPlaybackCursor(action, loop=bool(comp.get("loop", True)))
            self._entries.append(_Entry(entity_id, cursor, action.fps))

            # §5.3 - 재생 시작 전에 한 번만 로드한다.
            clip_base = os.path.dirname(action.source_path) if getattr(action, "source_path", None) else None
            clips = collect_clip_paths(action, clip_base)
            if clips:
                self._sound.preload(clips)

        return len(self._entries)

    def stop(self) -> None:
        """Stop/모드 전환 시. 소리가 다음 모드로 새어나가지 않게 한다."""
        self._entries.clear()
        self._sound.stop_all()

    # ── 매 프레임 ────────────────────────────────────────────────────────────

    def tick(self, dt_seconds: float) -> int:
        """경과 시간만큼 전진시키고 Sound 이벤트를 재생한다. 재생 요청 수를 돌려준다.

        **틱 수가 아니라 경과 시간으로 전진시키는 것이 중요하다.** 호출자(main.py의
        _on_fps_tick)는 60fps 고정인데 액션 fps는 다를 수 있어서, 틱 수를 프레임 수로
        쓰면 30fps 액션이 두 배 빨리 재생된다.
        """
        if dt_seconds <= 0.0 or not self._entries:
            return 0

        played = 0
        for entry in self._entries:
            entry.accumulator += dt_seconds * entry.fps
            frames = int(entry.accumulator)
            if frames <= 0:
                continue
            entry.accumulator -= frames

            fired = entry.cursor.advance(frames)
            played += self._play_sounds(fired)

        return played

    def _play_sounds(self, fired_events) -> int:
        """발화된 이벤트 중 Sound만 재생한다.

        커서는 EventType을 모른다 - v1이 Sound만 소비한다는 결정(범위 정의서 §3)은
        호출부의 것이다. Motion Editor 쪽과 같은 규칙을 쓴다.
        """
        if not fired_events:
            return 0
        count = 0
        for e in events_of_type(fired_events, EventType.SOUND.value):
            params = e.params or {}
            clip = params.get("clip")
            if not clip:
                continue
            if self._sound.play(clip,
                                volume=float(params.get("volume", 1.0)),
                                volume_var=float(params.get("volumeVar", 0.0))):
                count += 1
        return count

    # ── 내부 ─────────────────────────────────────────────────────────────────

    @staticmethod
    def _iter_action_players(registry):
        """(entity_id, component dict) 목록. 엔진 변경 없이 기존 바인딩만 쓴다."""
        try:
            entities = registry.GetAllEntities()
        except Exception as e:
            print(f"[ScenePlayback] 엔티티 열거 실패: {e}")
            return

        for entity in entities:
            entity_id = getattr(entity, "id", entity)
            try:
                raw = registry.GetComponentJson(entity, COMPONENT_NAME)
            except Exception:
                continue
            if not raw or raw == "{}":
                continue
            try:
                comp = json.loads(raw)
            except json.JSONDecodeError:
                continue
            yield entity_id, comp
