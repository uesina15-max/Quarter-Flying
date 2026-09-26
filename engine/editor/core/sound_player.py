"""
editor/core/sound_player.py

Sound Lite의 오디오 재생 래퍼. docs/SOUND_LITE_IMPLEMENTATION_PLAN.md §2.2.

**재생 호출부를 이 한 곳으로 모으는 것**이 이 모듈의 존재 이유다. 범위 정의서 §4.3에서
v1의 오디오 백엔드를 엔진이 아니라 에디터(QtMultimedia)에 두기로 했는데, 나중에 엔진
오디오가 생기면 이 클래스 안쪽만 바뀌고 호출부(재생 루프 두 곳)는 그대로 남는다.

지키는 규칙(범위 정의서 §5):
  §5.3  클립은 preload()에서 한 번 로드하고, play()에서는 파일 I/O도 디코딩도 하지 않는다.
        재생 시점의 디스크 접근은 곧 "틀린 타이밍"이다.
  §5.2  동시 재생 보이스 상한. 초과 시 **신규 재생을 거부**하고, 이미 나고 있는 소리를
        끊지 않는다.
  §5.6  클립 로딩/재생 실패가 액션 재생을 막지 않는다. 예외를 던지지 않고 False를
        돌려주며 로그만 남긴다.

v1의 알려진 한계: **클립 하나당 QSoundEffect 인스턴스 하나**를 쓴다. 따라서 같은 클립이
아직 울리는 중에 다시 트리거되면 겹쳐 나지 않고 **처음부터 다시 난다**(QSoundEffect의
동작). 클립마다 인스턴스 풀을 두면 겹칠 수 있지만, 그러려면 "클립당 몇 개"라는 새 값이
생기고 프리로드 비용도 그만큼 늘어난다 - 짧은 효과음이 자기 자신과 겹치는 경우는 드물어서
v1은 이 한계를 받아들인다.
"""

from __future__ import annotations

import os
import random
from typing import Dict, Iterable, Optional

from PySide6.QtCore import QUrl
from PySide6.QtMultimedia import QSoundEffect


class SoundPlayer:
    """액션 사운드 재생기. 액션 하나당 하나를 만들어 쓰는 것을 전제로 한다."""

    # 동시에 울릴 수 있는 소리 개수(범위 정의서 §5.2). 이펙트 하나가 조절할 값이 아니라
    # 엔진 정책이므로 §2의 3개 값에 넣지 않는다.
    DEFAULT_MAX_VOICES = 16

    def __init__(self, max_voices: int = DEFAULT_MAX_VOICES, rng: Optional[random.Random] = None):
        self._effects: Dict[str, QSoundEffect] = {}
        self._max_voices = max(1, int(max_voices))
        self._rng = rng or random.Random()

    # ── 로딩 ─────────────────────────────────────────────────────────────────

    def preload(self, clip_paths: Iterable[str], wait_ms: int = 1000) -> Dict[str, bool]:
        """클립들을 미리 로드한다. 액션을 열 때 **한 번만** 부른다.

        QSoundEffect의 로딩은 비동기라, 여기서 Ready가 될 때까지 잠깐 이벤트를 돌린다.
        액션 열기는 프레임 루프가 아니므로 잠깐 기다려도 된다 - **매 프레임 호출 금지.**

        반환: 경로 -> 성공 여부. 실패한 클립을 호출부가 알 수 있게 조용히 넘기지 않는다.
        """
        paths = [p for p in dict.fromkeys(clip_paths) if p]  # 중복 제거, 순서 유지

        for path in paths:
            if path in self._effects:
                continue
            effect = QSoundEffect()
            if os.path.isfile(path):
                effect.setSource(QUrl.fromLocalFile(os.path.abspath(path)))
            # 파일이 없으면 source를 아예 설정하지 않는다 - status가 Null로 남고
            # is_loaded()가 False를 돌려준다.
            self._effects[path] = effect

        self._wait_until_loaded(paths, wait_ms)

        results = {p: self.is_loaded(p) for p in paths}
        for path, ok in results.items():
            if not ok:
                print(f"[SoundPlayer] 클립 로드 실패(무시하고 계속): {path}")
        return results

    def is_loaded(self, clip: str) -> bool:
        effect = self._effects.get(clip)
        return effect is not None and effect.status() == QSoundEffect.Status.Ready

    def _wait_until_loaded(self, paths, wait_ms: int) -> None:
        if wait_ms <= 0 or not paths:
            return
        # QApplication이 없는 환경(순수 로직 테스트 등)에서는 그냥 넘어간다.
        from PySide6.QtWidgets import QApplication
        app = QApplication.instance()
        if app is None:
            return

        from PySide6.QtCore import QDeadlineTimer, QElapsedTimer
        timer = QElapsedTimer()
        timer.start()
        while timer.elapsed() < wait_ms:
            pending = [p for p in paths
                       if self._effects[p].status() == QSoundEffect.Status.Loading]
            if not pending:
                break
            app.processEvents()

    # ── 재생 ─────────────────────────────────────────────────────────────────

    def play(self, clip: str, volume: float = 1.0, volume_var: float = 0.0) -> bool:
        """클립 하나를 재생한다. 실패해도 예외를 던지지 않고 False를 돌려준다(§5.6).

        volume_var는 여기서 적용한다 - 재생 루프가 둘이라(범위 정의서 §6.2.2) 호출부에
        두면 두 군데에 같은 난수 로직이 복사된다.
        """
        effect = self._effects.get(clip)
        if effect is None:
            print(f"[SoundPlayer] 프리로드되지 않은 클립(재생 건너뜀): {clip}")
            return False

        if effect.status() != QSoundEffect.Status.Ready:
            print(f"[SoundPlayer] 클립이 준비되지 않음(재생 건너뜀): {clip} "
                  f"status={effect.status()}")
            return False

        # §5.2 보이스 상한. 이미 울리는 소리를 끊지 않고 신규 재생만 거부한다.
        # 이 클립이 이미 울리는 중이면 새 보이스를 쓰는 게 아니라 그 인스턴스를
        # 재시작하는 것이므로 상한 검사에서 제외한다(위 "알려진 한계" 참고).
        if not effect.isPlaying() and self.playing_count() >= self._max_voices:
            return False

        effect.setVolume(self._resolve_volume(volume, volume_var))
        effect.play()
        return True

    def _resolve_volume(self, volume: float, volume_var: float) -> float:
        v = float(volume)
        if volume_var:
            v *= 1.0 + self._rng.uniform(-1.0, 1.0) * float(volume_var)
        return max(0.0, min(1.0, v))

    def playing_count(self) -> int:
        return sum(1 for e in self._effects.values() if e.isPlaying())

    def stop_all(self) -> None:
        """Stop/모드 전환 시. 소리가 다음 모드로 새어나가지 않게 한다."""
        for effect in self._effects.values():
            if effect.isPlaying():
                effect.stop()

    def clear(self) -> None:
        """액션을 닫을 때. 로드한 클립을 전부 버린다."""
        self.stop_all()
        self._effects.clear()


def collect_clip_paths(action, base_dir: Optional[str] = None) -> list:
    """액션의 Sound 이벤트들이 참조하는 클립 경로를 모은다(프리로드용).

    base_dir가 주어지면 상대 경로를 그 기준으로 푼다 - 범위 정의서 §6.4대로
    params.clip은 `.action.json` 파일 위치 기준 상대 경로로 저장되기 때문이다.
    """
    from core.action_data import EventType

    paths = []
    for e in getattr(action, "events", []):
        if e.type != EventType.SOUND:
            continue
        clip = (e.params or {}).get("clip")
        if not clip:
            continue
        if base_dir and not os.path.isabs(clip):
            clip = os.path.normpath(os.path.join(base_dir, clip))
        paths.append(clip)
    return list(dict.fromkeys(paths))
