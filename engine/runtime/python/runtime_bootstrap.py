"""
runtime/python/runtime_bootstrap.py

게임 실행 파일(Player)이 시작할 때 불러오는 Python 쪽 런타임. docs/GAME_BUILD_PLAN.md §3.5, Phase 2.

에디터에서는 Qt 타이머(main.py의 _on_fps_tick)가 하던 일을, Player에서는 PlayerMain.cpp의
메인 루프가 매 프레임 tick(dt)을 불러서 한다. 지금 하는 일은 액션 재생(ActionPlayerComponent)뿐이다.

**PySide6를 import하지 않는다.** Player 폴더에는 Qt가 없다(설계 제약 C4). 액션 재생 코드는
에디터와 같은 core/ 모듈을 쓰되(규칙을 두 벌로 만들지 않기 위해), 소리는 NullSoundPlayer로
낸다 — 게임 빌드 오디오는 v1에서 제외했다(SOUND_LITE_PLAN §6.1, D9).
"""

from __future__ import annotations

from core.scene_action_playback import ScenePlaybackController
from core.sound_player import NullSoundPlayer

# 한 프레임에 전진시킬 최대 시간(초). 창을 끌거나 디버거에 멈춰 있다가 돌아오면 dt가 커지는데,
# 그만큼을 한 번에 진행하면 액션 이벤트가 한꺼번에 쏟아진다(에디터의 _on_fps_tick과 같은 상한).
MAX_STEP_SECONDS = 0.1

_controller: ScenePlaybackController | None = None


def start(registry, assets_dir: str) -> int:
    """씬을 불러오고 World.Play() 직후에 한 번 불린다. 재생을 시작한 엔티티 수를 돌려준다.

    assets_dir는 ActionPlayerComponent.action이 상대 경로일 때의 기준 폴더다. 에디터와 같은 규칙으로
    "<game.json의 assetRoot>/assets"가 들어온다(에디터는 "<프로젝트>/assets").
    """
    global _controller
    _controller = ScenePlaybackController(sound_player=NullSoundPlayer())
    started = _controller.start(registry, assets_dir)
    print(f"[runtime_bootstrap] action playback started for {started} entities")
    return started


def tick(dt_seconds: float) -> None:
    """매 프레임 TickFrame 뒤에 불린다."""
    if _controller is not None:
        _controller.tick(min(float(dt_seconds), MAX_STEP_SECONDS))
